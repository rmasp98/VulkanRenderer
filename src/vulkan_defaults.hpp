#pragma once

#include "vulkan/vulkan.hpp"

namespace defaults {

static inline uint32_t const MaxFramesInFlight = 10;

namespace pipeline {  // Graphics Pipeline

static inline vk::PipelineVertexInputStateCreateInfo const VertexInput{
    {}, nullptr, nullptr};

static inline vk::PipelineInputAssemblyStateCreateInfo const InputAssembly{
    {}, vk::PrimitiveTopology::eTriangleList};

static inline vk::PipelineViewportStateCreateInfo const ViewportState{
    {}, 1, nullptr, 1, nullptr};

static inline vk::PipelineRasterizationStateCreateInfo const Rasterizer{
    {},
    false,
    false,
    vk::PolygonMode::eFill,
    vk::CullModeFlagBits::eBack,
    vk::FrontFace::eClockwise,
    false,
    0.0f,
    0.0f,
    0.0f,
    1.0f};

static inline vk::PipelineMultisampleStateCreateInfo const Multisampling{
    {}, vk::SampleCountFlagBits::e1};

static inline vk::PipelineDepthStencilStateCreateInfo const DepthStencil{
    {},
    true,
    true,
    vk::CompareOp::eLessOrEqual,
    false,
    false,
    {vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep,
     vk::CompareOp::eAlways},
    {vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep,
     vk::CompareOp::eAlways}};

static inline vk::PipelineColorBlendAttachmentState const ColourBlendAttachment{
    false,
    vk::BlendFactor::eZero,
    vk::BlendFactor::eZero,
    vk::BlendOp::eAdd,
    vk::BlendFactor::eZero,
    vk::BlendFactor::eZero,
    vk::BlendOp::eAdd,
    {vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
     vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA}};

static inline vk::PipelineColorBlendStateCreateInfo const ColourBlending{
    {},
    false,  // enable
    vk::LogicOp::eNoOp,
    const_cast<vk::PipelineColorBlendAttachmentState&>(ColourBlendAttachment),
    {{1.0f, 1.0f, 1.0f, 1.0f}}};

static inline std::array<vk::DynamicState, 2> const DynamicStates{
    vk::DynamicState::eViewport, vk::DynamicState::eScissor};

static inline vk::PipelineDynamicStateCreateInfo const DynamicState{
    {}, DynamicStates};

// Layout
static inline vk::PipelineLayoutCreateInfo const LayoutCreateInfo{{}, {}};

// RenderPass
static inline vk::AttachmentDescription const ColourAttachment{
    {},
    vk::Format::eUndefined,
    vk::SampleCountFlagBits::e1,
    vk::AttachmentLoadOp::eClear,
    vk::AttachmentStoreOp::eStore,
    vk::AttachmentLoadOp::eDontCare,
    vk::AttachmentStoreOp::eDontCare,
    vk::ImageLayout::eUndefined,
    vk::ImageLayout::ePresentSrcKHR};

static inline vk::AttachmentReference const ColourAttachmentRef{
    0, vk::ImageLayout::eColorAttachmentOptimal};

static inline vk::SubpassDescription const Subpass{
    {}, vk::PipelineBindPoint::eGraphics,
    {}, const_cast<vk::AttachmentReference&>(ColourAttachmentRef),
    {}, nullptr};

// TODO: find out what these values are supposed to be
static inline vk::SubpassDependency const Dependency{
    VK_SUBPASS_EXTERNAL,
    0,
    vk::PipelineStageFlagBits::eColorAttachmentOutput,
    vk::PipelineStageFlagBits::eColorAttachmentOutput,
    {},
    vk::AccessFlagBits::eColorAttachmentWrite};

}  // namespace pipeline

namespace framebuffer {

static inline vk::ComponentMapping ComponentMapping(vk::ComponentSwizzle::eR,
                                                    vk::ComponentSwizzle::eG,
                                                    vk::ComponentSwizzle::eB,
                                                    vk::ComponentSwizzle::eA);

static inline vk::ImageSubresourceRange SubResourceRange(
    vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

}  // namespace framebuffer

}  // namespace defaults