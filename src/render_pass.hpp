#ifndef VULKAN_RENDERER_RENDER_PASS_HPP
#define VULKAN_RENDERER_RENDER_PASS_HPP

#include <unordered_map>

#include "defaults.hpp"
#include "device_api.hpp"
#include "pipeline.hpp"
#include "queues.hpp"
#include "render_settings.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

class RenderPass {
 public:
  RenderPass(RenderPassSettings const& settings, vk::Extent2D const& extent,
             Queues const& queues, DeviceApi& device)
      : settings_(settings),
        frameBufferAttachments_(
            settings_.CreateAttachmentBuffers(extent, queues, device)),
        renderPass_(
            device.CreateRenderpass(settings_.GetRenderPassCreateInfo())),
        framebuffers_(device.CreateFramebuffers(
            device.CreateSwapchainImageViews(), GetAttachmentImageViews(),
            renderPass_, extent)) {}

  PipelineHandle CreatePipeline(PipelineSettings settings, DeviceApi& device) {
    settings.Multisampling.rasterizationSamples =
        settings_.GetMultisampleCount();

    static std::atomic<uint32_t> currentPipelineId = 0;
    auto pipelineId = currentPipelineId++;

    auto imageViews = device.CreateSwapchainImageViews();
    pipelines_.emplace(
        pipelineId, Pipeline{pipelineId, settings, renderPass_.get(), device});

    return {pipelineId, [&](PipelineId const id) { RemovePipeline(id); }};
  }

  // TOOD: probably need locks for this and emplace above
  void RemovePipeline(PipelineId const id) { pipelines_.erase(id); }

  Pipeline const& GetPipeline(PipelineId pipeline) const {
    assert(pipelines_.contains(pipeline));
    return pipelines_.at(pipeline);
  }

  std::vector<PipelineId> GetPipelineIds() const {
    std::vector<PipelineId> pipelines;
    for (auto const& [id, _] : pipelines_) {
      pipelines.push_back(id);
    }
    return pipelines;
  }

  void Bind(ImageIndex const imageIndex, vk::Extent2D const& extent,
            PipelineId const& pipeline,
            vk::CommandBuffer const& cmdBuffer) const {
    assert(imageIndex < framebuffers_.size() && pipelines_.contains(pipeline));

    cmdBuffer.beginRenderPass(
        {renderPass_.get(), framebuffers_[imageIndex].GetFramebuffer(),
         vk::Rect2D(vk::Offset2D(0, 0), extent), settings_.GetClearValues()},
        vk::SubpassContents::eInline);
    pipelines_.at(pipeline).Bind(cmdBuffer);
  }

  void Recreate(vk::Extent2D const& extent, Queues const& queues,
                DeviceApi& device) {
    frameBufferAttachments_ =
        settings_.CreateAttachmentBuffers(extent, queues, device);
    renderPass_ = device.CreateRenderpass(settings_.GetRenderPassCreateInfo());
    framebuffers_ = device.CreateFramebuffers(
        device.CreateSwapchainImageViews(), GetAttachmentImageViews(),
        renderPass_, extent);

    for (auto& [_, pipeline] : pipelines_) {
      pipeline.Recreate(renderPass_.get(), device);
    }
  }

 protected:
  std::vector<vk::ImageView> GetAttachmentImageViews() {
    std::vector<vk::ImageView> imageViews;
    for (auto const& buffer : frameBufferAttachments_) {
      imageViews.push_back(buffer.GetImageView());
    }
    return imageViews;
  }

 private:
  RenderPassSettings settings_;
  std::vector<ImageBuffer> frameBufferAttachments_;
  vk::UniqueRenderPass renderPass_;
  std::vector<Framebuffer> framebuffers_;
  std::unordered_map<PipelineId, Pipeline> pipelines_;
};

}  // namespace vulkan_renderer

#endif