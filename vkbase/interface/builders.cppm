module;

#include <vulkan/vulkan_hpp_macros.hpp>

#define FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

export module vkbase:builders;

import std;
import vulkan_hpp;
import :primitives;
import :range_utils;
import :ref_holder;

export namespace vkbase {
    struct DefaultAppQueueFamilyIndices {
        std::uint32_t compute;

        explicit DefaultAppQueueFamilyIndices(vk::PhysicalDevice physicalDevice) {
            for (auto [idx, properties] : physicalDevice.getQueueFamilyProperties() | ru::enumerate<std::uint32_t>) {
                if (properties.queueFlags & vk::QueueFlagBits::eCompute) {
                    compute = idx;
                    return;
                }
            }

            throw std::invalid_argument { "Physical device doesn't have the required queue families." };
        }
    };

    struct DefaultAppQueues {
        vk::Queue compute;

        DefaultAppQueues(vk::Device device, const DefaultAppQueueFamilyIndices &queueFamilyIndices) noexcept
            : compute { device.getQueue(queueFamilyIndices.compute, 0) } {

        }

        [[nodiscard]] static auto getQueueCreateInfos(const DefaultAppQueueFamilyIndices &queueFamilyIndices) noexcept -> RefHolder<std::array<vk::DeviceQueueCreateInfo, 1>, float> {
            return {
                [&](const float &priority) {
                    return std::array {
                        vk::DeviceQueueCreateInfo {
                                {},
                                queueFamilyIndices.compute,
                                vk::ArrayProxyNoTemporaries(priority),
                            }
                    };
                },
                1.f, // queue priority.
            };
        }
    };

    template <
        std::constructible_from<vk::PhysicalDevice> QueueFamilyIndices = DefaultAppQueueFamilyIndices,
        std::constructible_from<vk::Device, QueueFamilyIndices> Queues = DefaultAppQueues>
    struct AppBuilder {
        struct DefaultPhysicalDeviceRater {
            [[nodiscard]] auto operator()(vk::PhysicalDevice physicalDevice) const -> std::uint32_t {
                // Check if given device supports the required queue families.
                try {
                    const QueueFamilyIndices queueFamilyIndices { physicalDevice }; (void)queueFamilyIndices;
                }
                catch (const std::runtime_error&) {
                    return 0;
                }

                std::uint32_t score = 0;

                // Rate physical device by its ability.
                const vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
                if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                    score += 1000;
                }
                score += properties.limits.maxImageDimension2D;

                return score;
            }
        };

        vk::InstanceCreateFlags instanceCreateFlags;
        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;
        std::function<std::uint32_t(vk::PhysicalDevice)> physicalDeviceRater = DefaultPhysicalDeviceRater{};
        std::vector<const char*> deviceLayers;
        std::vector<const char*> deviceExtensions;
        vk::PhysicalDeviceFeatures physicalDeviceFeatures;

        auto enableValidationLayer() -> AppBuilder& {
            instanceLayers.emplace_back("VK_LAYER_KHRONOS_validation");
            return *this;
        }

        auto enablePortabilityExtension() -> AppBuilder& {
            instanceCreateFlags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
            instanceExtensions.append_range(std::array { "VK_KHR_portability_enumeration", "VK_KHR_get_physical_device_properties2" });
            deviceExtensions.emplace_back("VK_KHR_portability_subset");
            return *this;
        }

        template <typename... Features>
        [[nodiscard]] auto build(const vk::ApplicationInfo &appInfo, Features &&...features) -> App<QueueFamilyIndices, Queues> {
            vk::raii::Context context;

            vk::raii::Instance instance = [this, &context, &appInfo] {
                return vk::raii::Instance { context, vk::InstanceCreateInfo {
                    instanceCreateFlags,
                    &appInfo,
                    instanceLayers,
                    instanceExtensions,
                } };
            }();
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
            VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
#endif

            vk::raii::PhysicalDevice physicalDevice = [this, &instance] {
                std::vector physicalDevices = instance.enumeratePhysicalDevices();
                if (physicalDevices.empty()) {
                    throw std::runtime_error { "No Vulkan physical device found." };
                }

                auto it = std::ranges::max_element(physicalDevices, {}, [this](const vk::raii::PhysicalDevice &physicalDevice) {
                    return physicalDeviceRater(*physicalDevice);
                });
                if (physicalDeviceRater(**it) == 0) {
                    throw std::runtime_error { "No physical device for required Vulkan queue family configuration." };
                }

                return *it;
            }();

            const QueueFamilyIndices queueFamilyIndices { *physicalDevice };

            vk::raii::Device device = [this, &physicalDevice, &queueFamilyIndices, &features...] {
                const auto queueCreateInfos = Queues::getQueueCreateInfos(queueFamilyIndices);
                return vk::raii::Device { physicalDevice, vk::StructureChain { vk::DeviceCreateInfo {
                    {},
                    queueCreateInfos.get(),
                    deviceLayers,
                    deviceExtensions,
                    &physicalDeviceFeatures,
                }, FWD(features)... }.get() };
            }();

            const Queues queues { *device, queueFamilyIndices };

            return {
                std::move(context),
                std::move(instance),
                std::move(physicalDevice),
                queueFamilyIndices,
                std::move(device),
                queues,
            };
        }
    };

    struct DefaultAppWithSwapchainQueueFamilyIndices {
        std::uint32_t graphics, compute, present;

        explicit DefaultAppWithSwapchainQueueFamilyIndices(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) {
            std::optional<std::uint32_t> graphics, compute, present;
            for (auto [idx, properties] : physicalDevice.getQueueFamilyProperties() | ru::enumerate<std::uint32_t>) {
                if (properties.queueFlags & vk::QueueFlagBits::eGraphics) {
                    graphics = idx;
                }
                if (properties.queueFlags & vk::QueueFlagBits::eCompute) {
                    compute = idx;
                }
                if (physicalDevice.getSurfaceSupportKHR(idx, surface)) {
                    present = idx;
                }

                if (graphics && compute && present) {
                    this->graphics = *graphics;
                    this->compute = *compute;
                    this->present = *present;
                    return;
                }
            }

            throw std::invalid_argument { "Physical device doesn't have the required queue families." };
        }
    };

    struct DefaultAppWithSwapchainQueues {
        vk::Queue graphics, compute, present;

        DefaultAppWithSwapchainQueues(vk::Device device, const DefaultAppWithSwapchainQueueFamilyIndices &queueFamilyIndices) noexcept
            : graphics { device.getQueue(queueFamilyIndices.graphics, 0) },
              compute { device.getQueue(queueFamilyIndices.compute, 0) },
              present { device.getQueue(queueFamilyIndices.present, 0) } {

        }

        [[nodiscard]] static auto getQueueCreateInfos(const DefaultAppWithSwapchainQueueFamilyIndices &queueFamilyIndices) noexcept -> RefHolder<std::vector<vk::DeviceQueueCreateInfo>, float> {
            std::vector uniqueQueueFamilyIndices { queueFamilyIndices.graphics, queueFamilyIndices.compute, queueFamilyIndices.present };
            std::ranges::sort(uniqueQueueFamilyIndices);
            const auto duplicateRange = std::ranges::unique(uniqueQueueFamilyIndices);
            uniqueQueueFamilyIndices.erase(duplicateRange.begin(), duplicateRange.end());

            return {
                [&](const float &priority) {
                    return std::vector { std::from_range, uniqueQueueFamilyIndices | std::views::transform([&](std::uint32_t queueFamilyIndex) {
                        return vk::DeviceQueueCreateInfo {
                            {},
                            queueFamilyIndex,
                            vk::ArrayProxyNoTemporaries(priority),
                        };
                    }) };
                },
                1.f, // queue priority.
            };
        }
    };

    template <
        std::constructible_from<vk::PhysicalDevice, vk::SurfaceKHR> QueueFamilyIndices = DefaultAppWithSwapchainQueueFamilyIndices,
        std::constructible_from<vk::Device, QueueFamilyIndices> Queues = DefaultAppWithSwapchainQueues>
    struct AppWithSwapchainBuilder {
        struct DefaultPhysicalDeviceRater {
            [[nodiscard]] auto operator()(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface) const -> std::uint32_t {
                // Check if given device supports the required queue families.
                try {
                    const QueueFamilyIndices queueFamilyIndices { physicalDevice, surface }; (void)queueFamilyIndices;
                }
                catch (const std::runtime_error&) {
                    return 0;
                }

                std::uint32_t score = 0;

                // Rate physical device by its ability.
                const vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
                if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                    score += 1000;
                }
                score += properties.limits.maxImageDimension2D;

                return score;
            }
        };

        vk::InstanceCreateFlags instanceCreateFlags;
        std::vector<const char*> instanceLayers;
        std::vector<const char*> instanceExtensions;
        std::function<std::uint32_t(vk::PhysicalDevice, vk::SurfaceKHR)> physicalDeviceRater = DefaultPhysicalDeviceRater{};
        std::vector<const char*> deviceLayers;
        std::vector<const char*> deviceExtensions;
        vk::PhysicalDeviceFeatures physicalDeviceFeatures;
        vk::Format swapchainImageFormat = vk::Format::eB8G8R8A8Srgb;
        vk::ColorSpaceKHR swapchainColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        vk::ImageUsageFlags swapchainImageUsage = vk::ImageUsageFlagBits::eColorAttachment;
        vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;

        auto enableValidationLayer() -> AppWithSwapchainBuilder& {
            instanceLayers.emplace_back("VK_LAYER_KHRONOS_validation");
            return *this;
        }

        auto enablePortabilityExtension() -> AppWithSwapchainBuilder& {
            instanceCreateFlags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
            instanceExtensions.append_range(std::array { "VK_KHR_portability_enumeration", "VK_KHR_get_physical_device_properties2" });
            deviceExtensions.emplace_back("VK_KHR_portability_subset");
            return *this;
        }

        template <typename... Features>
        [[nodiscard]] auto build(const vk::ApplicationInfo &appInfo, std::function<vk::SurfaceKHR(vk::Instance)> surfaceGen, vk::Extent2D swapchainExtent, Features &&...features) -> AppWithSwapchain<QueueFamilyIndices, Queues> {
            vk::raii::Context context;

            vk::raii::Instance instance = [this, &context, &appInfo] {
                return vk::raii::Instance { context, vk::InstanceCreateInfo {
                    instanceCreateFlags,
                    &appInfo,
                    instanceLayers,
                    instanceExtensions,
                } };
            }();
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
            VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
#endif

            vk::raii::SurfaceKHR surface { instance, surfaceGen(*instance) };

            vk::raii::PhysicalDevice physicalDevice = [this, &instance, &surface] {
                std::vector physicalDevices = instance.enumeratePhysicalDevices();
                if (physicalDevices.empty()) {
                    throw std::runtime_error { "No Vulkan physical device found." };
                }

                auto it = std::ranges::max_element(physicalDevices, {}, [&](const vk::raii::PhysicalDevice &physicalDevice) {
                    return physicalDeviceRater(*physicalDevice, *surface);
                });
                if (physicalDeviceRater(**it, *surface) == 0) {
                    throw std::runtime_error { "No physical device for required Vulkan queue family configuration." };
                }

                return *it;
            }();

            const QueueFamilyIndices queueFamilyIndices { *physicalDevice, *surface };

            deviceExtensions.emplace_back("VK_KHR_swapchain");
            vk::raii::Device device = [this, &physicalDevice, &queueFamilyIndices, &features...] {
                const auto queueCreateInfos = Queues::getQueueCreateInfos(queueFamilyIndices);
                return vk::raii::Device { physicalDevice, vk::StructureChain { vk::DeviceCreateInfo {
                    {},
                    queueCreateInfos.get(),
                    deviceLayers,
                    deviceExtensions,
                    &physicalDeviceFeatures,
                }, FWD(features)... }.get() };
            }();

            const Queues queues { *device, queueFamilyIndices };

            const vk::SurfaceCapabilitiesKHR capabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
            const vk::SwapchainCreateInfoKHR swapchainCreateInfo {
                {},
                *surface,
                std::max(capabilities.minImageCount + 1, capabilities.maxImageCount),
                swapchainImageFormat,
                swapchainColorSpace,
                swapchainExtent,
                1,
                swapchainImageUsage,
                {}, {},
                capabilities.currentTransform,
                vk::CompositeAlphaFlagBitsKHR::eOpaque,
                swapchainPresentMode,
            };
            vk::raii::SwapchainKHR swapchain { device, swapchainCreateInfo };
            std::vector swapchainImages = swapchain.getImages();

            return {
                std::move(context),
                std::move(instance),
                std::move(surface),
                std::move(physicalDevice),
                queueFamilyIndices,
                std::move(device),
                queues,
                swapchainCreateInfo,
                std::move(swapchain),
                std::move(swapchainImages),
            };
        }
    };
}