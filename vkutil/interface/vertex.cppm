module;

#define INDEX_SEQ(Is, N, ...) [&]<std::size_t... Is>(std::index_sequence<Is...>) __VA_ARGS__ (std::make_index_sequence<N>{})

export module vkutil:vertex;

import std;
import vulkan_hpp;

export namespace vkutil {
    // TODO.CXX23: Self template parameter can be omitted using explicit object parameter.
    template <typename Self, std::uint32_t AttributeCount>
    struct VertexAttributeInfoBase {
        using AttributeArrayType = std::array<vk::VertexInputAttributeDescription, AttributeCount>;

        template <std::uint32_t... AttributeIndex>
        [[nodiscard]] static constexpr auto getAttributeDescriptions(std::uint32_t binding, std::array<std::uint32_t, sizeof...(AttributeIndex)> locations) noexcept {
            const auto setLocation = [binding](vk::VertexInputAttributeDescription attributeDescription, std::uint32_t location) noexcept {
                attributeDescription.binding = binding;
                attributeDescription.location = location;
                return attributeDescription;
            };
            return std::array { setLocation(get<AttributeIndex>(Self::attributes), get<AttributeIndex>(locations))... };
        }

        [[nodiscard]] static constexpr auto getAttributeDescriptions(std::uint32_t binding, std::array<std::uint32_t, AttributeCount> locations) noexcept {
            return INDEX_SEQ(Is, AttributeCount, {
                return getAttributeDescriptions<Is...>(binding, locations);
            });
        }
    };

    template <typename>
    struct VertexAttributeInfo;
}