module;

#define FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

export module vkutil:lifetime_holder;

import std;
import vulkan_hpp;

export namespace vkutil{
    class LifetimeHolder {
        std::mutex mutex;
        std::multimap<vk::CommandBuffer, std::any> storage;

    public:
        template <std::movable... Ts>
        void hold(vk::CommandBuffer commandBuffer, Ts &&...data) {
            const std::scoped_lock lock { mutex };
            (storage.emplace(commandBuffer, std::make_shared<Ts>(FWD(data))), ...);
        }

        auto drop(vk::CommandBuffer commandBuffer) -> std::size_t {
            const std::scoped_lock lock { mutex };
            return storage.erase(commandBuffer);
        }
    };
}