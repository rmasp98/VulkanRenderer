#ifndef VULKAN_RENDERER_COMMAND_HPP
#define VULKAN_RENDERER_COMMAND_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "buffers/image_buffer.hpp"
#include "buffers/vertex_buffer.hpp"
#include "device_api.hpp"
#include "queues.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

class Command {
 public:
  // TODO: need to add some kind of priority
  void AddVertexBuffer(std::unique_ptr<Buffer>&& vertBuffer) {
    vertBuffers_.emplace_back(std::move(vertBuffer));
  }

  // TODO: Give ability to set a relative viewport and scissor
  void Initialise(DescriptorSetLayouts& descriptorSetLayouts,
                  Queues const& queues, DeviceApi& device) {
    if (!IsInitialised()) {
      cmdBuffers_ = device.AllocateCommandBuffers(
          vk::CommandBufferLevel::ePrimary, device.GetNumSwapchainImages());

      for (auto& vertBuffer : vertBuffers_) {
        vertBuffer->Initialise(descriptorSetLayouts, queues, device);
      }
    }
  }

  void Record(ImageIndex const imageIndex, vk::UniquePipeline const& pipeline,
              vk::UniquePipelineLayout const& pipelineLayout,
              vk::UniqueRenderPass const& renderPass,
              std::vector<vk::ClearValue> clearValues,
              Framebuffer const& framebuffer, vk::Extent2D const& extent,
              Queues const& queues, DeviceApi& device) {
    assert(imageIndex < cmdBuffers_.size());

    auto& cmdBuffer = cmdBuffers_[imageIndex];
    cmdBuffer->reset();
    cmdBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

    cmdBuffer->beginRenderPass(
        {renderPass.get(), framebuffer.GetFramebuffer(),
         vk::Rect2D(vk::Offset2D(0, 0), extent), clearValues},
        vk::SubpassContents::eInline);

    cmdBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());

    // TODO: this should probably be set per vertBuffer
    cmdBuffer->setViewport(
        0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
                        static_cast<float>(extent.height), 0.0f, 1.0f));
    cmdBuffer->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

    for (auto& vertBuffer : vertBuffers_) {
      vertBuffer->UploadUniforms(imageIndex, queues, device);
      vertBuffer->UploadPushConstants(pipelineLayout, cmdBuffer);
      vertBuffer->Bind(imageIndex, pipelineLayout, cmdBuffer);
      vertBuffer->Draw(cmdBuffer);
    }

    cmdBuffer->endRenderPass();
    cmdBuffer->end();
  }

  bool IsInitialised() { return !cmdBuffers_.empty(); }

  void Unregister() { cmdBuffers_.clear(); }

  void Draw(uint32_t const imageIndex, Queues const& queues) const {
    queues.SubmitToGraphics(cmdBuffers_[imageIndex]);
  }

 private:
  std::vector<vk::UniqueCommandBuffer> cmdBuffers_;
  std::vector<std::unique_ptr<Buffer>> vertBuffers_;
};

}  // namespace vulkan_renderer

#endif