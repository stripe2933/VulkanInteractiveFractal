export module vkutil:image;

import std;
import vk_mem_alloc_hpp;
import vulkan_hpp;

export namespace vkutil {
    struct Image {
        vk::Image image;
        vk::Extent3D extent;
        vk::Format format;
        std::uint32_t mipLevels;
        std::uint32_t arrayLayers;

        operator vk::Image() const noexcept {
            return image;
        }

        [[nodiscard]] auto getFullSubresourceRange(vk::ImageAspectFlags aspect) const -> vk::ImageSubresourceRange {
            return { aspect, 0, mipLevels, 0, arrayLayers };
        }

        [[nodiscard]] auto getFullSubresourceLayers(vk::ImageAspectFlags aspect, std::uint32_t mipLevel) const -> vk::ImageSubresourceLayers {
            return { aspect, mipLevel, 0, arrayLayers };
        }

        [[nodiscard]] auto getMemoryBarrier(vk::ImageLayout newLayout, vk::AccessFlags srcAccessMask = {}, vk::AccessFlags dstAccessMask = {}, vk::ImageLayout oldLayout = {}, const std::optional<vk::ImageSubresourceRange> &subresourceRange = {}) const -> vk::ImageMemoryBarrier {
            return {
                srcAccessMask, dstAccessMask,
                oldLayout, newLayout,
                {}, {},
                image,
                subresourceRange.value_or(getFullSubresourceRange(vk::ImageAspectFlagBits::eColor))
            };
        }

        [[nodiscard]] auto getMemoryBarrier2(vk::ImageLayout newLayout, vk::PipelineStageFlags2 srcStageMask = {}, vk::AccessFlags2 srcAccessMask = {}, vk::PipelineStageFlags2 dstStageMask = vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlags2 dstAccessMask = {}, vk::ImageLayout oldLayout = {}, const std::optional<vk::ImageSubresourceRange> &subresourceRange = {}) const -> vk::ImageMemoryBarrier2 {
            return {
                srcStageMask, srcAccessMask,
                dstStageMask, dstAccessMask,
                oldLayout, newLayout,
                {}, {},
                image,
                subresourceRange.value_or(getFullSubresourceRange(vk::ImageAspectFlagBits::eColor))
            };
        }

        [[nodiscard]] auto getViewCreateInfo(vk::ImageViewType type = vk::ImageViewType::e2D, const std::optional<vk::ImageSubresourceRange> &subresourceRange = {}) const -> vk::ImageViewCreateInfo {
            return {
                {},
                image,
                type,
                format,
                {},
                subresourceRange.value_or(getFullSubresourceRange(vk::ImageAspectFlagBits::eColor)),
            };
        }
    };

    struct AllocatedImage : Image {
        vma::Allocator allocator;
        vma::Allocation allocation;

        AllocatedImage(vma::Allocator allocator, const vk::ImageCreateInfo &createInfo, const vma::AllocationCreateInfo &allocationCreateInfo)
            : allocator { allocator }{
            std::tie(image, allocation) = allocator.createImage(createInfo, allocationCreateInfo);
            extent = createInfo.extent;
            format = createInfo.format;
            mipLevels = createInfo.mipLevels;
            arrayLayers = createInfo.arrayLayers;
        }
        AllocatedImage(const AllocatedImage &) = delete;
        AllocatedImage(AllocatedImage &&src) noexcept
            : Image { static_cast<Image>(src) },
              allocator { src.allocator },
              allocation { std::exchange(src.allocation, nullptr) } {

        }
        auto operator=(const AllocatedImage &) -> AllocatedImage& = delete;
        auto operator=(AllocatedImage &&src) noexcept -> AllocatedImage& {
            if (allocation) {
                allocator.destroyImage(image, allocation);
            }

            static_cast<Image&>(*this) = static_cast<Image>(src);
            allocator = src.allocator;
            allocation = std::exchange(src.allocation, nullptr);
            return *this;
        }

        ~AllocatedImage() {
            if (allocation) {
                allocator.destroyImage(image, allocation);
            }
        }
    };
}