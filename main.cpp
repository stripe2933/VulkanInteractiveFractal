#include <GLFW/glfw3.h>
#include <vulkan/vulkan_hpp_macros.hpp>

#define INDEX_SEQ(Is, N, ...) [&]<std::size_t... Is>(std::index_sequence<Is...>) __VA_ARGS__ (std::make_index_sequence<N>{})

import std;
import glm;
import vk_mem_alloc_hpp;
import vkbase;
import vkutil;
import vulkan_hpp;

template <typename... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template <typename... Ts> overload(Ts...) -> overload<Ts...>;

class FractalImageWriter {
    const vk::raii::Device &device;

    vk::raii::DescriptorSetLayout descriptorSetLayout = createDescriptorSetLayout();
    vk::raii::PipelineLayout pipelineLayout = createPipelineLayout();
    vk::raii::Pipeline pipeline = createPipeline();

    [[nodiscard]] auto createDescriptorSetLayout() const -> vk::raii::DescriptorSetLayout {
        constexpr vk::DescriptorSetLayoutBinding layoutBinding {
            0,
            vk::DescriptorType::eStorageImage,
            1,
            vk::ShaderStageFlagBits::eCompute,
        };
        return { device, vk::DescriptorSetLayoutCreateInfo {
            {},
            layoutBinding,
        } };
    }

    [[nodiscard]] auto createPipelineLayout() const -> vk::raii::PipelineLayout {
        constexpr vk::PushConstantRange pushConstantRange {
            vk::ShaderStageFlagBits::eCompute,
            0, sizeof(PushConstantData),
        };

        return { device, vk::PipelineLayoutCreateInfo {
            {},
            *descriptorSetLayout,
            pushConstantRange,
        } };
    }

    [[nodiscard]] auto createPipeline() const -> vk::raii::Pipeline {
        const vk::raii::ShaderModule computeShaderModule { device, vkutil::getShaderModuleCreateInfo("shaders/fractal.comp.spv") };
        return { device, nullptr, vk::ComputePipelineCreateInfo {
            {},
            vk::PipelineShaderStageCreateInfo {
                {},
                vk::ShaderStageFlagBits::eCompute,
                *computeShaderModule,
                "main",
            },
            *pipelineLayout,
        } };
    }

public:
    struct PushConstantData {
        glm::vec4 bound;
        glm::vec2 c;
        std::uint32_t maxIterations;
    };

    explicit FractalImageWriter(const vk::raii::Device &device)
        : device { device }{

    }

    [[nodiscard]] auto allocateDescriptorSets(vk::DescriptorPool descriptorPool) const -> std::array<vk::DescriptorSet, 1> {
        return {
            (*device).allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                descriptorPool,
                *descriptorSetLayout,
            })[0]
        };
    }

    void updateDescriptorSets(std::span<const vk::DescriptorSet, 1> descriptorSets, vk::ImageView dstView) const {
        const vk::DescriptorImageInfo imageInfo {
            {},
            dstView,
            vk::ImageLayout::eGeneral,
        };
        (*device).updateDescriptorSets({
            vk::WriteDescriptorSet {
                descriptorSets[0],
                0,
                0,
                1,
                vk::DescriptorType::eStorageImage,
                &imageInfo,
            }
        }, {});
    }

    void write(vk::CommandBuffer commandBuffer, vk::DescriptorSet descriptorSet, vk::Extent2D imageSize, const PushConstantData &data) const {
        constexpr glm::uvec2 LOCAL_WORKGROUP_SIZE { 16, 16 };
        const glm::uvec2 groupCount = (glm::uvec2 { imageSize.width, imageSize.height } + LOCAL_WORKGROUP_SIZE - glm::uvec2 { 1 }) / LOCAL_WORKGROUP_SIZE;

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, descriptorSet, {});
        commandBuffer.pushConstants<PushConstantData>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, data);
        commandBuffer.dispatch(groupCount.x, groupCount.y, 1);
    }
};

struct AllocatorDestroyer {
    vma::Allocator allocator;

    ~AllocatorDestroyer() {
        allocator.destroy();
    }
};

class MainApp final
    : public vkutil::GlfwWindow,
      vkbase::AppWithSwapchain<vkbase::DefaultAppWithSwapchainQueueFamilyIndices, vkbase::DefaultAppWithSwapchainQueues> {
    struct Frame {
        static constexpr std::size_t COMPUTE_COMMAND_COUNT = 1;

        vk::CommandBuffer computeCommandBuffer;

        // Per-frame descriptors.
        vk::DescriptorSet descriptorSet;

        // Sync objects.
        vk::raii::Semaphore imageAcquireSema;
        vk::raii::Semaphore computeFinishSema;
        vk::raii::Fence inFlightFence;
    };

    struct PanState {
        struct NotPanning{};
        struct Intermediate { glm::dvec2 initialPos; };
        struct Panning { glm::dvec2 prevPos; };
        using type = std::variant<NotPanning, Intermediate, Panning>;
    };
    PanState::type panState = PanState::NotPanning{};

    static constexpr vk::ApplicationInfo appInfo {
        "Vulkan Interactive Fractal", 0,
        nullptr, 0,
        vk::makeApiVersion(0, 1, 0, 0),
    };
    static constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    vma::Allocator allocator = createAllocator();
    AllocatorDestroyer allocatorDestroyer { allocator };
    std::vector<vkutil::RenderingAttachmentGroup> attachmentGroups = createAttachmentGroups();
    vk::raii::CommandPool commandPool = createCommandPool();
    vk::raii::DescriptorPool descriptorPool = createDescriptorPool();

    FractalImageWriter fractalImageWriter { device };

    std::array<Frame, MAX_FRAMES_IN_FLIGHT> frames = createFrames();

    // Parameters.
    bool printIterationCount = false;
    bool printFps = false;
    std::array<glm::vec2, 2> bound { glm::vec2 { -2.f }, glm::vec2 { 2.f } };
    glm::vec2 c;
    std::uint32_t iterationCount = 200;

    [[nodiscard]] auto createVulkanApp() -> AppWithSwapchain{
        return vkbase::AppWithSwapchainBuilder{
            .instanceExtensions = getInstanceExtensions(),
            .swapchainImageUsage = vk::ImageUsageFlagBits::eStorage,
            .swapchainPresentMode = std::getenv("BENCH_FPS") ? vk::PresentModeKHR::eImmediate : vk::PresentModeKHR::eFifo,
        }
#ifndef NDEBUG
        .enableValidationLayer()
#endif
#if __APPLE__
        .enablePortabilityExtension()
#endif
        .build(appInfo, [this](vk::Instance instance) {
            return createSurface(instance);
        }, [this] {
            const glm::uvec2 framebufferSize = getFramebufferSize();
            return vk::Extent2D { framebufferSize.x, framebufferSize.y };
        }());
    }

    [[nodiscard]] auto createAllocator() const -> vma::Allocator {
        return vma::createAllocator(vma::AllocatorCreateInfo {
            {},
            *physicalDevice,
            *device,
            {}, {}, {}, {}, {},
            *instance,
            appInfo.apiVersion,
        });
    }

    [[nodiscard]] auto createAttachmentGroups() const -> std::vector<vkutil::RenderingAttachmentGroup> {
        return { std::from_range, swapchainImages | std::views::transform([this](vk::Image image) {
            vkutil::RenderingAttachmentGroup attachmentGroup { device, allocator, swapchainInfo.imageExtent };
            attachmentGroup.addColorAttachment(vkutil::Image {
                image,
                vk::Extent3D { swapchainInfo.imageExtent, 1 },
                swapchainInfo.imageFormat,
                1, 1,
            });
            return attachmentGroup;
        }) };
    }

    [[nodiscard]] auto createCommandPool() const -> vk::raii::CommandPool {
        return { device, vk::CommandPoolCreateInfo {
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            queueFamilyIndices.compute,
        } };
    }

    [[nodiscard]] auto createDescriptorPool() const -> vk::raii::DescriptorPool {
        constexpr vk::DescriptorPoolSize poolSize {
            vk::DescriptorType::eStorageImage,
            MAX_FRAMES_IN_FLIGHT
        };
        return { device, vk::DescriptorPoolCreateInfo {
            {},
            MAX_FRAMES_IN_FLIGHT,
            poolSize,
        } };
    }

    [[nodiscard]] auto createFrames() const -> std::array<Frame, MAX_FRAMES_IN_FLIGHT> {
        return INDEX_SEQ(Is, MAX_FRAMES_IN_FLIGHT, {
            const std::vector commandBuffers = (*device).allocateCommandBuffers({
                *commandPool,
                vk::CommandBufferLevel::ePrimary,
                MAX_FRAMES_IN_FLIGHT * Frame::COMPUTE_COMMAND_COUNT,
            });

            return std::array { Frame {
                commandBuffers[Frame::COMPUTE_COMMAND_COUNT * Is],
                get<0>(fractalImageWriter.allocateDescriptorSets(*descriptorPool)),
                { device, vk::SemaphoreCreateInfo{} },
                { device, vk::SemaphoreCreateInfo{} },
                { device, vk::FenceCreateInfo { vk::FenceCreateFlagBits::eSignaled } },
            }... };
        });
    }

    void handleSwapchainResizeSignal() {
        glm::uvec2 size;
        do {
            std::this_thread::yield();
            pollEvents();
            size = getFramebufferSize();
        } while (!shouldClose() && size == glm::uvec2 { 0 });

        device.waitIdle();
        recreateSwapchain({ size.x, size.y });

        // Recreate swapchain extent related stuffs.
        attachmentGroups = createAttachmentGroups();
    }

    void onMouseButtonCallback(int button, int action, int mods) override {
        if (action == GLFW_PRESS) {
            panState = PanState::Intermediate { getFramebufferCursorPos() };
        }
        else if (action == GLFW_RELEASE) {
            panState = PanState::NotPanning{};
        }
    }

    void onCursorPosCallback(glm::dvec2) override {
        // Note: should not use the function's cursorPos parameter, which is calculated from the window's logical size.
        const glm::dvec2 position = getFramebufferCursorPos();
        std::visit(overload {
            [](PanState::NotPanning&){},
            [&](const PanState::Intermediate &state) {
                constexpr double PAN_THRESHOLD = 2.0;
                if (length(position - state.initialPos) >= PAN_THRESHOLD) {
                    panState = PanState::Panning { state.initialPos };
                }
            },
            [&](PanState::Panning &state) {
                const glm::vec2 offset = (position - state.prevPos) / glm::dvec2 { getFramebufferSize() } * glm::dvec2 { get<1>(bound) - get<0>(bound) };
                get<0>(bound) -= offset;
                get<1>(bound) -= offset;
                state.prevPos = position;
            },
        }, panState);
    }

    void onScrollCallback(glm::dvec2 delta) override {
        const glm::vec2 scale = pow(glm::vec2 { 1.01f }, glm::vec2 { static_cast<float>(delta.y) });
        const glm::vec2 scaleCenter = mix(
            get<0>(bound), get<1>(bound),
            getFramebufferCursorPos() / glm::dvec2 { getFramebufferSize() }
        );
        bound = { (get<0>(bound) - scaleCenter) * scale + scaleCenter, (get<1>(bound) - scaleCenter) * scale + scaleCenter };
    }

    void onKeyCallback(int key, int scancode, int action, int mods) override {
        if (action == GLFW_PRESS) {
            switch (key) {
                case GLFW_KEY_R: {
                    const glm::vec2 boundCenter = (get<0>(bound) + get<1>(bound)) / 2.f;
                    const glm::vec2 halfExtent = (get<1>(bound) - get<0>(bound)) / 2.f;
                    const glm::vec2 multiplier { getFramebufferAspectRatio() * halfExtent.y / halfExtent.x, 1.f };
                    bound = { boundCenter - halfExtent * multiplier, boundCenter + halfExtent * multiplier };
                    std::println("Bound aspect ratio aligned to the window's aspect ratio");
                    break;
                }
                case GLFW_KEY_I: {
                    printIterationCount = !printIterationCount;
                    std::println("printIterationCount: {}", printIterationCount ? "on" : "off");
                    break;
                }
                case GLFW_KEY_F: {
                    printFps = !printFps;
                    std::println("printFps: {}", printFps ? "on" : "off");
                    break;
                }
            }
        }
    }

    void update(float timeDelta) {
        const std::uint32_t newIterationCount = std::max(
            200U,
            static_cast<std::uint32_t>(-50.f * std::log2f(length(get<1>(bound) - get<0>(bound)))));
        const bool iterationCountChanged = newIterationCount != iterationCount;
        iterationCount = newIterationCount;

        if (printIterationCount && iterationCountChanged) {
            std::println("Max iteration: {}", iterationCount);
        }

        static std::vector<float> fpsRecords(128);
        static std::size_t frameCounter = 0;
        fpsRecords[frameCounter++] = 1.f / timeDelta;
        if (frameCounter == fpsRecords.size()) {
            if (printFps) {
                const float averageFps = std::reduce(fpsRecords.cbegin(), fpsRecords.cend(), 0.f) / fpsRecords.size();
                std::println("FPS: {}", averageFps);
            }
            frameCounter = 0;
        }
    }

    void computeFractal(const Frame &frame, const vkutil::RenderingAttachmentGroup &attachmentGroup) const {
        frame.computeCommandBuffer.reset();
        frame.computeCommandBuffer.begin({ vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

        const vk::ImageMemoryBarrier barrier = attachmentGroup.colorAttachments[0].image.getMemoryBarrier(
            vk::ImageLayout::eGeneral,
            {}, vk::AccessFlagBits::eShaderWrite
        );
        frame.computeCommandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTopOfPipe,
            vk::PipelineStageFlagBits::eComputeShader,
            {}, {}, {}, barrier
        );

        fractalImageWriter.updateDescriptorSets(std::array { frame.descriptorSet }, *attachmentGroup.colorAttachments[0].view);
        fractalImageWriter.write(frame.computeCommandBuffer, frame.descriptorSet, swapchainInfo.imageExtent, FractalImageWriter::PushConstantData {
            { get<0>(bound), get<1>(bound) },
            c,
            iterationCount,
        });

        const vk::ImageMemoryBarrier barrier2 = attachmentGroup.colorAttachments[0].image.getMemoryBarrier(
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits::eShaderWrite, {},
            vk::ImageLayout::eGeneral
        );
        frame.computeCommandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eBottomOfPipe,
            {}, {}, {}, barrier2
        );

        frame.computeCommandBuffer.end();

        constexpr vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        queues.compute.submit(vk::SubmitInfo {
            *frame.imageAcquireSema,
            waitStage,
            frame.computeCommandBuffer,
            *frame.computeFinishSema,
        }, *frame.inFlightFence);
    }

    void onLoop(float timeDelta) override {
        static std::uint32_t frameIndex = 0;
        const Frame &currentFrame = frames[frameIndex];

        constexpr std::uint64_t TIMEOUT = std::numeric_limits<std::uint64_t>::max();
        if (device.waitForFences(*currentFrame.inFlightFence, vk::True, TIMEOUT) != vk::Result::eSuccess) {
            throw std::runtime_error { "Waiting previous frame failed" };
        }

        update(timeDelta);

        const std::optional imageIndex = acquireSwapchainImageIndex(*currentFrame.imageAcquireSema);
        if (!imageIndex) {
            handleSwapchainResizeSignal();
            return;
        }
        device.resetFences(*currentFrame.inFlightFence);

        computeFractal(currentFrame, attachmentGroups[*imageIndex]);

        if (!presentSwapchainImage(queues.present, *imageIndex, std::array { *currentFrame.computeFinishSema })) {
            handleSwapchainResizeSignal();
        }

        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void afterLoop() override {
        device.waitIdle();
    }

public:
    MainApp(const glm::vec2 &c)
        : GlfwWindow { 512, 512, "Vulkan Interactive Fractal" },
          AppWithSwapchain { createVulkanApp() },
          c { c }{
        std::println(
            "[Usage]\n"
            "- Press F to toggle printing the FPS.\n"
            "- Press I to toggle printing the max iteration count.\n"
            "- Press R to align the aspect ratio of the bound to the window's aspect ratio.\n"
            "- Pan with the mouse to move the bound.\n"
            "- Scroll to zoom in/out.");
    }
};

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

int main(int argc, char **argv) {
    if (argc != 3){
        std::println(std::cerr, "Usage: {} <real part of c> <imaginary part of c>", argv[0]);
        std::abort();
    }
    const glm::vec2 c { std::stof(argv[1]), std::stof(argv[2]) };

    VULKAN_HPP_DEFAULT_DISPATCHER.init();

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    MainApp app { c };
    app.run();

    glfwTerminate();
}
