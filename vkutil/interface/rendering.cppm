export module vkutil:rendering;

import std;
import vk_mem_alloc_hpp;
import vulkan_hpp;
import :image;
import :ref_holder;

export namespace vkutil{
    struct RenderingAttachment {
        Image image;
        vk::raii::ImageView view;
    };

    class RenderingAttachmentGroup {
        const vk::raii::Device *device;
        vma::Allocator allocator;
        vk::Extent2D extent;

        [[nodiscard]] auto createAttachmentImage(vk::Format format, vk::SampleCountFlagBits sampleCount, vk::ImageUsageFlags usage) const -> AllocatedImage {
            return { allocator, vk::ImageCreateInfo {
                {},
                vk::ImageType::e2D,
                format,
                vk::Extent3D { extent, 1 },
                1, 1,
                sampleCount,
                vk::ImageTiling::eOptimal,
                usage,
            }, vma::AllocationCreateInfo {
                vma::AllocationCreateFlagBits::eDedicatedMemory,
                vma::MemoryUsage::eAutoPreferDevice,
            } };
        }

    public:
        struct RenderingInfo {
            vk::AttachmentLoadOp loadOp;
            vk::AttachmentStoreOp storeOp;
            vk::ClearValue clearValue;
        };

        struct ColorInfo : RenderingInfo {
            vk::ImageView imageView;
            vk::ResolveModeFlagBits resolveMode;
            vk::ImageView resolveImageView;
            vk::ImageLayout resolveImageLayout;

            explicit ColorInfo(vk::ImageView imageView, vk::ResolveModeFlagBits resolveMode = {}, vk::ImageView resolveImageView = {}, vk::ImageLayout resolveImageLayout = {}, vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore, const vk::ClearColorValue &clearValue = { 0.f, 0.f, 0.f, 1.f })
                : RenderingInfo { loadOp, storeOp, clearValue },
                  imageView { imageView },
                  resolveMode { resolveMode },
                  resolveImageView { resolveImageView },
                  resolveImageLayout { resolveImageLayout } {

            }
        };

        struct DepthStencilInfo : RenderingInfo {
            vk::ImageView imageView;

            explicit DepthStencilInfo(vk::ImageView imageView, vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eDontCare, const vk::ClearDepthStencilValue &clearValue = { 1.f, 0 })
                : RenderingInfo { loadOp, storeOp, clearValue },
                  imageView { imageView } {

            }
        };

        std::vector<RenderingAttachment> colorAttachments;
        std::optional<RenderingAttachment> depthAttachment;

        RenderingAttachmentGroup(const vk::raii::Device &device, vma::Allocator allocator, vk::Extent2D extent)
            : device { &device },
              allocator { allocator },
              extent { extent } {

        }

        auto addColorAttachment(Image image) -> RenderingAttachment& {
            return colorAttachments.emplace_back(image, vk::raii::ImageView { *device, image.getViewCreateInfo() });
        }

        [[nodiscard]] auto createColorAttachmentImage(vk::Format format, vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1, vk::ImageUsageFlags usage = {}) const -> AllocatedImage {
            if (sampleCount != vk::SampleCountFlagBits::e1) {
                usage |= vk::ImageUsageFlagBits::eTransientAttachment;
            }
            return createAttachmentImage(format, sampleCount, usage | vk::ImageUsageFlagBits::eColorAttachment);
        }

        [[nodiscard]] auto addColorAttachment(vk::Format format, vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1, vk::ImageUsageFlags usage = {}) -> std::pair<AllocatedImage, RenderingAttachment&> {
            AllocatedImage image = createColorAttachmentImage(format, sampleCount, usage);
            RenderingAttachment &attachment = addColorAttachment(static_cast<Image>(image));
            return { std::move(image), attachment };
        }

        auto setDepthAttachment(Image image) -> RenderingAttachment& {
            return depthAttachment.emplace(image, vk::raii::ImageView { *device, image.getViewCreateInfo(
                vk::ImageViewType::e2D,
                vk::ImageSubresourceRange { vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 })
            });
        }

        [[nodiscard]] auto createDepthAttachmentImage(vk::Format format, vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1, vk::ImageUsageFlags usage = {}) const -> AllocatedImage {
            return createAttachmentImage(format, sampleCount, usage | vk::ImageUsageFlagBits::eDepthStencilAttachment);
        }

        [[nodiscard]] auto setDepthAttachment(vk::Format format, vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1, vk::ImageUsageFlags usage = {}) -> std::pair<AllocatedImage, RenderingAttachment&> {
            AllocatedImage image = createDepthAttachmentImage(format, sampleCount, usage);
            RenderingAttachment &attachment = setDepthAttachment(static_cast<Image>(image));
            return { std::move(image), attachment };
        }

        [[nodiscard]] auto getFramebufferCreateInfo(vk::RenderPass renderPass) const -> RefHolder<vk::FramebufferCreateInfo, std::vector<vk::ImageView>> {
            return {
                [this, renderPass](std::span<const vk::ImageView> attachmentViews) {
                    return vk::FramebufferCreateInfo {
                        {},
                        renderPass,
                        attachmentViews,
                        extent.width, extent.height,
                        1,
                    };
                },
                std::vector { std::from_range, colorAttachments | std::views::transform([](const RenderingAttachment &attachment) {
                    return *attachment.view;
                }) }
            };
        }

        [[nodiscard]] auto getPipelineRenderingCreateInfo() const -> RefHolder<vk::PipelineRenderingCreateInfo, std::vector<vk::Format>> {
            return {
                [depthFormat = depthAttachment ? depthAttachment->image.format : vk::Format::eUndefined](std::span<const vk::Format> colorFormats) {
                    return vk::PipelineRenderingCreateInfoKHR { {}, colorFormats, depthFormat };
                },
                std::vector { std::from_range, colorAttachments | std::views::transform([](const RenderingAttachment &attachment) {
                    return attachment.image.format;
                }) }
            };
        }

        [[nodiscard]] auto getRenderingInfo(const std::vector<ColorInfo> &colorInfos = {}, const std::optional<DepthStencilInfo> &depthStencilInfo = {}) const -> RefHolder<vk::RenderingInfo, std::vector<vk::RenderingAttachmentInfo>, std::optional<vk::RenderingAttachmentInfo>> {
            return {
                [this](std::span<const vk::RenderingAttachmentInfoKHR> colorInfos, const std::optional<vk::RenderingAttachmentInfoKHR> &depthStencilInfo){
                    return vk::RenderingInfoKHR {
                        {},
                        { { 0, 0 }, extent },
                        1,
                        {},
                        colorInfos,
                        depthStencilInfo ? &*depthStencilInfo : nullptr,
                    };
                },
                std::vector { std::from_range, colorInfos | std::views::transform([](const ColorInfo &info) {
                    return vk::RenderingAttachmentInfoKHR {
                        info.imageView,
                        vk::ImageLayout::eColorAttachmentOptimal,
                        info.resolveMode, info.resolveImageView, info.resolveImageLayout,
                        info.loadOp, info.storeOp,
                        info.clearValue,
                    };
                }) },
                depthStencilInfo.transform([](const DepthStencilInfo &info) {
                    return vk::RenderingAttachmentInfoKHR {
                        info.imageView,
                        vk::ImageLayout::eDepthStencilAttachmentOptimal,
                        {}, {}, {},
                        info.loadOp, info.storeOp,
                        info.clearValue,
                    };
                })
            };
        }
    };
}