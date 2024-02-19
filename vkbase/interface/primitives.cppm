export module vkbase:primitives;

import std;
import vulkan_hpp;

export namespace vkbase {
    template <typename QueueFamilyIndices, typename Queues>
    struct App {
        vk::raii::Context context;
        vk::raii::Instance instance;
        vk::raii::PhysicalDevice physicalDevice;
        QueueFamilyIndices queueFamilyIndices;
        vk::raii::Device device;
        Queues queues;
    };

    template <typename QueueFamilyIndices, typename Queues>
    struct AppWithSwapchain {
        vk::raii::Context context;
        vk::raii::Instance instance;
        vk::raii::SurfaceKHR surface;
        vk::raii::PhysicalDevice physicalDevice;
        QueueFamilyIndices queueFamilyIndices;
        vk::raii::Device device;
        Queues queues;
        vk::SwapchainCreateInfoKHR swapchainInfo;
        vk::raii::SwapchainKHR swapchain;
        std::vector<vk::Image> swapchainImages;

        [[nodiscard]] auto acquireSwapchainImageIndex(vk::Semaphore imageAvailableSema = {}, std::uint64_t timeout = std::numeric_limits<std::uint64_t>::max()) const -> std::optional<std::uint32_t> {
            switch (const auto [result, imageIndex] = (*device).acquireNextImageKHR(*swapchain, timeout, imageAvailableSema); result) {
                case vk::Result::eSuccess: case vk::Result::eSuboptimalKHR:
                    return imageIndex;
                case vk::Result::eErrorOutOfDateKHR:
                    return std::nullopt;
                default:
                    throw std::runtime_error { "Acquiring swapchain image failed" };
            }
        }

        [[nodiscard]] auto presentSwapchainImage(vk::Queue presentQueue, std::uint32_t imageIndex, std::span<const vk::Semaphore> waitSemas = {}) const -> bool {
            switch (presentQueue.presentKHR(vk::PresentInfoKHR { waitSemas, *swapchain, imageIndex })) {
                case vk::Result::eSuccess:
                    return true;
                case vk::Result::eErrorOutOfDateKHR:
                case vk::Result::eSuboptimalKHR:
                    return false;
                default:
                    throw std::runtime_error { "Presenting swapchain image failed" };
            }
        }

        [[nodiscard]] auto presentSwapchainImage(std::uint32_t imageIndex, std::span<const vk::Semaphore> waitSemas = {}) const -> bool
            requires requires { queues.getPresentQueue() -> vk::Queue; }{
            return presentSwapchainImage(queues.getPresentQueue(), imageIndex, waitSemas);
        }

        void recreateSwapchain(vk::Extent2D extent) {
            swapchainInfo.imageExtent = extent;
            swapchainInfo.oldSwapchain = *swapchain;
            swapchain = { device, swapchainInfo };
            swapchainImages = swapchain.getImages();
        }
    };
}