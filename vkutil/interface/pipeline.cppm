export module vkutil:pipeline;

import std;
import vulkan_hpp;
import :ref_holder;

template <typename... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template <typename... Ts> overload(Ts...) -> overload<Ts...>;

export namespace vkutil {
     [[nodiscard]] auto getShaderModuleCreateInfo(const std::filesystem::path &path) -> RefHolder<vk::ShaderModuleCreateInfo, std::vector<std::byte>> {
         std::ifstream file { path, std::ios::ate | std::ios::binary };
         if (!file.is_open()) {
             throw std::runtime_error { "Failed to open file." };
         }

         const std::size_t fileSize = file.tellg();
         std::vector<std::byte> buffer(fileSize);
         file.seekg(0);
         file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

         return { [](std::span<const std::byte> code) {
             return vk::ShaderModuleCreateInfo {
                 {},
                 code.size_bytes(),
                 reinterpret_cast<const std::uint32_t*>(code.data()),
             };
         }, std::move(buffer) };
     }

     [[nodiscard]] auto getDefaultGraphicsPipelineCreateInfo(std::span<const vk::PipelineShaderStageCreateInfo> stages, vk::PipelineLayout layout, const std::variant<std::pair<vk::RenderPass, std::uint32_t>, vk::PipelineRenderingCreateInfo> &renderingInfo) noexcept -> vk::GraphicsPipelineCreateInfo {
     static constexpr vk::PipelineVertexInputStateCreateInfo vertexInputState{};

     static constexpr vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{
         {},
         vk::PrimitiveTopology::eTriangleList,
     };

     static constexpr vk::PipelineViewportStateCreateInfo viewportState{
         {},
         1, nullptr,
         1, nullptr
     };

     static constexpr vk::PipelineRasterizationStateCreateInfo rasterizationState{
         {},
         {}, {},
         vk::PolygonMode::eFill,
         vk::CullModeFlagBits::eBack, {},
         {}, {}, {}, {},
         1.f
     };

     static constexpr vk::PipelineMultisampleStateCreateInfo multisampleState{};

     static constexpr vk::PipelineColorBlendAttachmentState attachment {
         {},
         {}, {}, {},
         {}, {}, {},
         vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
     };
     static const vk::PipelineColorBlendStateCreateInfo colorBlendState{
         {},
         {}, {}, attachment
     };

     static constexpr std::array dynamicStates {
         vk::DynamicState::eViewport,
         vk::DynamicState::eScissor,
     };
     static const vk::PipelineDynamicStateCreateInfo dynamicState {
         {}, dynamicStates
     };

     vk::GraphicsPipelineCreateInfo createInfo {
         {},
         stages,
         &vertexInputState,
         &inputAssemblyState,
         nullptr,
         &viewportState,
         &rasterizationState,
         &multisampleState,
         nullptr,
         &colorBlendState,
         &dynamicState,
         layout,
     };
     std::visit(overload {
         [&createInfo](const std::pair<vk::RenderPass, std::uint32_t> &renderPassInfo) {
             const auto [renderPass, subpass] = renderPassInfo;
             createInfo.renderPass = renderPass;
             createInfo.subpass = subpass;
         },
         [&createInfo](const vk::PipelineRenderingCreateInfo &pipelineRenderingCreateInfo) {
             createInfo.pNext = &pipelineRenderingCreateInfo;
         },
     }, renderingInfo);

     return createInfo;
     }
}