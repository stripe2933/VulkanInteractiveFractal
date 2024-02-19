#pragma once

#define VKUTIL_VERTEX_ATTRIBUTES_BEGIN(VertexType, N) \
    template <> \
    struct vkutil::VertexAttributeInfo<VertexType> : vkutil::VertexAttributeInfoBase<vkutil::VertexAttributeInfo<VertexType>, N> { \
        static constexpr AttributeArrayType attributes {
#define VKUTIL_VERTEX_ATTRIBUTE(Format, Offset) \
            vk::VertexInputAttributeDescription { {}, {}, Format, Offset }
#define VKUTIL_VERTEX_ATTRIBUTES_END \
        }; \
    };