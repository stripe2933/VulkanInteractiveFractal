export module vkutil:buffer;

import std;
import vk_mem_alloc_hpp;
import vulkan_hpp;

export namespace vkutil {
    struct Buffer {
        vk::Buffer buffer;
        vk::DeviceSize size;

        operator vk::Buffer() const noexcept {
            return buffer;
        }
    };

    struct AllocatedBuffer : Buffer {
        vma::Allocator allocator;
        vma::Allocation allocation;

        AllocatedBuffer(vma::Allocator allocator, const vk::BufferCreateInfo &createInfo, const vma::AllocationCreateInfo &allocationCreateInfo)
            : allocator { allocator }{
            std::tie(buffer, allocation) = allocator.createBuffer(createInfo, allocationCreateInfo);
            size = createInfo.size;
        }
        AllocatedBuffer(const AllocatedBuffer &) = delete;
        AllocatedBuffer(AllocatedBuffer &&src) noexcept
            : Buffer { static_cast<Buffer>(src) },
              allocator { src.allocator },
              allocation { std::exchange(src.allocation, nullptr) } {

        }
        auto operator=(const AllocatedBuffer &) -> AllocatedBuffer& = delete;
        auto operator=(AllocatedBuffer &&src) noexcept -> AllocatedBuffer& {
            if (allocation) {
                allocator.destroyBuffer(buffer, allocation);
            }

            static_cast<Buffer&>(*this) = static_cast<Buffer>(src);
            allocator = src.allocator;
            allocation = std::exchange(src.allocation, nullptr);
            return *this;
        }

        ~AllocatedBuffer() {
            if (allocation) {
                allocator.destroyBuffer(buffer, allocation);
            }
        }
    };

    struct PersistentMappedBuffer : AllocatedBuffer {
        std::byte *data;

        PersistentMappedBuffer(vma::Allocator allocator, const vk::BufferCreateInfo &createInfo, const vma::AllocationCreateInfo &allocationCreateInfo)
            : AllocatedBuffer { allocator, createInfo, allocationCreateInfo } {
            data = static_cast<std::byte*>(allocator.mapMemory(allocation));
        }
        PersistentMappedBuffer(const PersistentMappedBuffer &) = delete;
        PersistentMappedBuffer(PersistentMappedBuffer &&src) noexcept
            : AllocatedBuffer { static_cast<AllocatedBuffer>(std::move(src)) },
              data { src.data } {

        }
        auto operator=(const PersistentMappedBuffer &) -> PersistentMappedBuffer& = delete;
        auto operator=(PersistentMappedBuffer &&src) noexcept -> PersistentMappedBuffer& {
            if (allocation) {
                allocator.unmapMemory(allocation);
            }

            static_cast<AllocatedBuffer&>(*this) = static_cast<AllocatedBuffer>(std::move(src));
            data = src.data;
            return *this;
        }

        ~PersistentMappedBuffer() {
            if (allocation) {
                allocator.unmapMemory(allocation);
            }
        }
    };
}