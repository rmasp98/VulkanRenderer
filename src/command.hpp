#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "device_api.hpp"
#include "queues.hpp"
#include "vertex_buffer.hpp"
#include "vulkan/vulkan.hpp"

class Command {
 public:
  void AddVertexBuffer(std::unique_ptr<Buffer>&& vertBuffer) {
    vertBuffers_.emplace_back(std::move(vertBuffer));
  }

  // TODO: Give ability to set a relative viewport and scissor

  void Allocate(DeviceApi const& device, uint32_t const numFramebuffers) {
    cmdBuffers_ = device.AllocateCommandBuffer(vk::CommandBufferLevel::ePrimary,
                                               numFramebuffers);
  }

  void Record(
      DeviceApi& device, vk::UniquePipeline const& pipeline,
      vk::UniqueRenderPass const& renderPass,
      std::vector<Framebuffer> const& framebuffers,
      std::unordered_map<vk::ShaderStageFlagBits,
                         std::shared_ptr<UniformBuffer>>& uniformBuffers,
      vk::Extent2D const& extent,
      vk::UniquePipelineLayout const& pipelineLayout) {
    assert(cmdBuffers_.size() == framebuffers.size());
    for (uint32_t i = 0; i < cmdBuffers_.size(); ++i) {
      auto& cmdBuffer = cmdBuffers_[i];
      cmdBuffer->begin(
          vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlags()));

      std::array<vk::ClearValue, 1> clearValues;
      clearValues[0].color =
          vk::ClearColorValue(std::array<float, 4>({{0.2f, 0.2f, 0.2f, 0.2f}}));

      cmdBuffer->beginRenderPass(
          {renderPass.get(), framebuffers[i].GetFramebuffer(),
           vk::Rect2D(vk::Offset2D(0, 0), extent), clearValues},
          vk::SubpassContents::eInline);

      cmdBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());

      cmdBuffer->setViewport(
          0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
                          static_cast<float>(extent.height), 0.0f, 1.0f));
      cmdBuffer->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));

      for (auto& uniformBuffer : uniformBuffers) {
        uniformBuffer.second->Bind(0, cmdBuffer, pipelineLayout);
      }

      for (auto& vertBuffer : vertBuffers_) {
        if (!vertBuffer->IsUploaded()) {
          vertBuffer->Upload(device);
        }
        vertBuffer->UpdateUniformBuffer(uniformBuffers, 0, device);
        vertBuffer->Record(cmdBuffer, pipelineLayout);
      }

      cmdBuffer->endRenderPass();
      cmdBuffer->end();
    }
    isRegistered_ = true;
  }

  bool IsRegistered() { return isRegistered_; }

  void Unregister() { isRegistered_ = false; }

  void Draw(uint32_t const imageIndex, Queues const& queues) const {
    queues.SubmitToGraphics(cmdBuffers_[imageIndex]);
  }

 private:
  std::vector<vk::UniqueCommandBuffer> cmdBuffers_;
  std::vector<std::unique_ptr<Buffer>> vertBuffers_;
  bool isRegistered_ = false;
};
