#ifndef VULKAN_RENDERER_COMMAND_HPP
#define VULKAN_RENDERER_COMMAND_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "buffers/image_buffer.hpp"
#include "buffers/vertex_buffer.hpp"
#include "device_api.hpp"
#include "pipeline.hpp"
#include "queues.hpp"
#include "render_pass.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

class Command {
 public:
  void AddVertexBuffer(std::shared_ptr<Buffer> const& vertBuffer) {
    vertBuffers_.push_back(vertBuffer);
  }

  void Allocate(vk::CommandPool const& pool, Queues const& queues,
                DeviceApi& device) {
    cmdBuffers_ = device.AllocateCommandBuffers(
        vk::CommandBufferLevel::ePrimary, device.GetNumSwapchainImages(), pool);
    isOutdated_.resize(cmdBuffers_.size(), true);

    // Hopefully should only ever allocate once
    for (auto& vertBuffer : vertBuffers_) {
      vertBuffer->Allocate(queues, device, false);
      vertBuffer->ClearDescriptorSets();
    }
  }

  void UpdateDescriptorSets(Pipeline const& pipeline, DeviceApi const& device) {
    for (auto& vertBuffer : vertBuffers_) {
      vertBuffer->UpdateDescriptorSets(pipeline, device);
    }
  }

  bool IsInitialised() const { return !cmdBuffers_.empty(); }

  bool IsOutdated(ImageIndex const imageIndex) {
    assert(imageIndex < isOutdated_.size());
    // TODO: Check any reasons to rerecord
    for (auto const& vertBuffer : vertBuffers_) {
      if (vertBuffer->IsOutdated()) {
        std::fill(isOutdated_.begin(), isOutdated_.end(), true);
      }
    }
    return isOutdated_[imageIndex];
  }

  void UploadUniforms(ImageIndex const imageIndex, Queues const& queues,
                      DeviceApi& device) {
    for (auto& vertBuffer : vertBuffers_) {
      vertBuffer->UploadUniforms(imageIndex, queues, device);
    }
  }

  void Record(ImageIndex const imageIndex, RenderPass const& renderPass,
              PipelineId const pipeline, vk::Extent2D const& extent) {
    assert(imageIndex < cmdBuffers_.size());

    auto& cmdBuffer = cmdBuffers_[imageIndex];
    cmdBuffer.reset();
    cmdBuffer.begin(vk::CommandBufferBeginInfo{});

    renderPass.Bind(imageIndex, extent, pipeline, cmdBuffer);

    // TODO: this should probably be set per vertBuffer
    cmdBuffer.setViewport(
        0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
                        static_cast<float>(extent.height), 0.0f, 1.0f));
    cmdBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

    for (auto& vertBuffer : vertBuffers_) {
      vertBuffer->UploadPushConstants(renderPass.GetPipeline(pipeline),
                                      cmdBuffer);
      vertBuffer->Bind(imageIndex, renderPass.GetPipeline(pipeline), cmdBuffer);
      vertBuffer->Draw(cmdBuffer);
    }

    cmdBuffer.endRenderPass();
    cmdBuffer.end();

    isOutdated_[imageIndex] = false;
  }

  void Draw(uint32_t const imageIndex, Semaphores const& renderSemaphores,
            Queues const& queues) const {
    vk::PipelineStageFlags flags{
        vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo submitInfo{renderSemaphores.WaitSemaphore.get(), flags,
                              cmdBuffers_[imageIndex],
                              renderSemaphores.CompleteSemaphore.get()};
    queues.SubmitToGraphics(submitInfo, renderSemaphores.CompleteFence.get());
  }

  void Clear() { cmdBuffers_.clear(); }

 private:
  std::vector<vk::CommandBuffer> cmdBuffers_;
  std::vector<bool> isOutdated_;
  std::vector<std::shared_ptr<Buffer>> vertBuffers_;
};

}  // namespace vulkan_renderer

#endif