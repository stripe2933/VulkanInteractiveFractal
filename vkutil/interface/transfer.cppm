export module vkutil:transfer;

import std;
import vk_mem_alloc_hpp;
import vulkan_hpp;
import :buffer;
import :image;

export namespace vkutil{
    // Copy buffer to buffer.
    void copy(vk::CommandBuffer commandBuffer, const Buffer &srcBuffer, vk::Buffer dstBuffer) {
        constexpr vk::BufferCopy copyRegion {
            0, 0, vk::WholeSize,
        };
        commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);
    }

    // Copy buffer to image.
    void copy(vk::CommandBuffer commandBuffer, const Buffer &srcBuffer, const Image &dstImage, const std::optional<vk::ImageSubresourceLayers> &subresourceLayers = {}) {
        const vk::BufferImageCopy copyRegion {
            0, 0, 0,
            subresourceLayers.value_or(dstImage.getFullSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0)),
            {},
            dstImage.extent,
        };
        commandBuffer.copyBufferToImage(srcBuffer, dstImage, vk::ImageLayout::eTransferDstOptimal, copyRegion);
    }

    // Copy image to image.
    void copy(vk::CommandBuffer commandBuffer, const Image &srcImage, const Image &dstImage, const std::optional<vk::ImageSubresourceLayers> &srcSubresourceLayers = {}, const std::optional<vk::ImageSubresourceLayers> &dstSubresourceLayers = {}) {
        const vk::ImageCopy copyRegion {
            srcSubresourceLayers.value_or(srcImage.getFullSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0)),
            0,
            dstSubresourceLayers.value_or(dstImage.getFullSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0)),
            {},
            srcImage.extent,
        };
        commandBuffer.copyImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstImage, vk::ImageLayout::eTransferDstOptimal, copyRegion);
    }

    // Copy image to buffer.
    void copy(vk::CommandBuffer commandBuffer, const Image &srcImage, vk::Buffer dstBuffer, const std::optional<vk::ImageSubresourceLayers> &subresourceLayers = {}) {
        const vk::BufferImageCopy copyRegion {
            0, 0, 0,
            subresourceLayers.value_or(srcImage.getFullSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0)),
            {},
            srcImage.extent,
        };
        commandBuffer.copyImageToBuffer(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstBuffer, copyRegion);
    }

    // Blit image.
    void blit(vk::CommandBuffer commandBuffer, const Image &srcImage, const Image &dstImage, const std::optional<vk::ImageSubresourceLayers> &srcSubresourceLayers = {}, const std::optional<vk::ImageSubresourceLayers> &dstSubresourceLayers = {}, vk::Filter filter = vk::Filter::eNearest) {
        const vk::ImageBlit blitRegion {
            srcSubresourceLayers.value_or(srcImage.getFullSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0)),
            { vk::Offset3D{}, vk::Offset3D { static_cast<int>(srcImage.extent.width), static_cast<int>(srcImage.extent.height), static_cast<int>(srcImage.extent.depth) } },
            dstSubresourceLayers.value_or(dstImage.getFullSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0)),
            { vk::Offset3D{}, vk::Offset3D { static_cast<int>(dstImage.extent.width), static_cast<int>(dstImage.extent.height), static_cast<int>(dstImage.extent.depth) } },
        };
        commandBuffer.blitImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstImage, vk::ImageLayout::eTransferDstOptimal, blitRegion, filter);
    }

    // Host-visible buffer to device-visible buffer. Device-visible buffer will be created. Host-visible buffer usage must includes eTransferSrc.
    [[nodiscard]] auto stage(vma::Allocator allocator, vk::CommandBuffer commandBuffer, const Buffer &srcBuffer, vk::BufferUsageFlags dstBufferUsage = {}) -> AllocatedBuffer {
        AllocatedBuffer buffer { allocator, vk::BufferCreateInfo {
            {},
            srcBuffer.size,
            vk::BufferUsageFlagBits::eTransferSrc | dstBufferUsage,
        }, vma::AllocationCreateInfo {
            vma::AllocationCreateFlagBits::eDedicatedMemory,
            vma::MemoryUsage::eAutoPreferDevice,
        } };
        copy(commandBuffer, srcBuffer, buffer);
        return buffer;
    }

    // Host-visible buffer to device-visible image. Device-visible image will be created. Host-visible buffer usage must includes eTransferSrc.
    [[nodiscard]] auto stage(vma::Allocator allocator, vk::CommandBuffer commandBuffer, const Buffer &srcBuffer, vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags dstImageUsage = {}) -> AllocatedImage {
        AllocatedImage image { allocator, vk::ImageCreateInfo {
            {},
            vk::ImageType::e3D,
            format,
            extent,
            1,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferDst | dstImageUsage,
        }, vma::AllocationCreateInfo {
            vma::AllocationCreateFlagBits::eDedicatedMemory,
            vma::MemoryUsage::eAutoPreferDevice,
        } };
        copy(commandBuffer, srcBuffer, image);
        return image;
    }

    // Host data to device-visible buffer. Both host-visible and device-visible buffers will be created.
    [[nodiscard]] auto stage(vma::Allocator allocator, vk::CommandBuffer commandBuffer, std::span<const std::byte> data, vk::BufferUsageFlags dstBufferUsage = {}) -> std::pair<PersistentMappedBuffer, AllocatedBuffer> {
        PersistentMappedBuffer hostBuffer { allocator, vk::BufferCreateInfo {
            {},
            data.size_bytes(),
            vk::BufferUsageFlagBits::eTransferSrc,
        }, vma::AllocationCreateInfo {
            vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
            vma::MemoryUsage::eAutoPreferHost,
        } };
        std::ranges::copy(data, hostBuffer.data);

        AllocatedBuffer deviceBuffer { allocator, vk::BufferCreateInfo {
            {},
            hostBuffer.size,
            vk::BufferUsageFlagBits::eTransferDst | dstBufferUsage,
        }, vma::AllocationCreateInfo {
            vma::AllocationCreateFlagBits::eDedicatedMemory,
            vma::MemoryUsage::eAutoPreferDevice,
        } };
        copy(commandBuffer, hostBuffer, deviceBuffer);

        return { std::move(hostBuffer), std::move(deviceBuffer) };
    }

    // Host data to device-visible image. Both host-visible buffer and device-visible image will be created.
    [[nodiscard]] auto stage(vma::Allocator allocator, vk::CommandBuffer commandBuffer, std::span<const std::byte> data, vk::Extent3D dstImageExtent, vk::Format dstImageFormat, vk::ImageUsageFlags dstImageUsage = {}) -> std::pair<PersistentMappedBuffer, AllocatedImage> {
        PersistentMappedBuffer hostBuffer { allocator, vk::BufferCreateInfo {
            {},
            data.size_bytes(),
            vk::BufferUsageFlagBits::eTransferSrc,
        }, vma::AllocationCreateInfo {
            vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
            vma::MemoryUsage::eAutoPreferHost,
        } };
        std::ranges::copy(data, hostBuffer.data);

        AllocatedImage deviceImage { allocator, vk::ImageCreateInfo {
            {},
            vk::ImageType::e2D,
            dstImageFormat,
            dstImageExtent,
            1,
            1,
            vk::SampleCountFlagBits::e1,
            vk::ImageTiling::eOptimal,
            vk::ImageUsageFlagBits::eTransferDst | dstImageUsage,
        }, vma::AllocationCreateInfo {
            vma::AllocationCreateFlagBits::eDedicatedMemory,
            vma::MemoryUsage::eAutoPreferDevice,
        } };
        copy(commandBuffer, hostBuffer, deviceImage);

        return { std::move(hostBuffer), std::move(deviceImage) };
    }

    // Device-visible buffer to host-visible buffer. Host-visible buffer will be created. Device-visible buffer usage must includes eTransferSrc.
    [[nodiscard]] auto destage(vma::Allocator allocator, vk::CommandBuffer commandBuffer, const Buffer &srcBuffer, vk::BufferUsageFlags dstBufferUsage = {}) -> PersistentMappedBuffer {
        PersistentMappedBuffer buffer { allocator, vk::BufferCreateInfo {
            {},
            srcBuffer.size,
            vk::BufferUsageFlagBits::eTransferDst | dstBufferUsage,
        }, vma::AllocationCreateInfo {
            vma::AllocationCreateFlagBits::eHostAccessRandom | vma::AllocationCreateFlagBits::eMapped,
            vma::MemoryUsage::eAutoPreferHost,
        } };
        copy(commandBuffer, srcBuffer, buffer);
        return buffer;
    }

    // Device-visible image to host-visible buffer. Host-visible buffer will be created. Device-visible image usage must includes eTransferSrc.
    [[nodiscard]] auto destage(vma::Allocator allocator, vk::CommandBuffer commandBuffer, const Image &srcImage, vk::BufferUsageFlags dstBufferUsage = {}, const std::optional<vk::ImageSubresourceLayers> &subresourceLayers = {}) -> PersistentMappedBuffer {
        PersistentMappedBuffer buffer { allocator, vk::BufferCreateInfo {
            {},
            srcImage.extent.width * srcImage.extent.height * srcImage.extent.depth * blockSize(srcImage.format),
            vk::BufferUsageFlagBits::eTransferDst | dstBufferUsage,
        }, vma::AllocationCreateInfo {
            vma::AllocationCreateFlagBits::eHostAccessRandom | vma::AllocationCreateFlagBits::eMapped,
            vma::MemoryUsage::eAutoPreferHost,
        } };
        copy(commandBuffer, srcImage, buffer, subresourceLayers);
        return buffer;
    }
}