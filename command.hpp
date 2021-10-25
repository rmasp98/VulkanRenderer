#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "buffer.hpp"
#include "device.hpp"
#include "utils.hpp"
#include "vulkan/vulkan.hpp"

class Command {
 public:
  Command() = default;
  ~Command() = default;

  Command(Command const& other) = delete;
  Command& operator=(Command const& other) = delete;
  Command(Command&& other) = default;
  Command& operator=(Command&& other) = default;

  void AddVertexBuffer(std::unique_ptr<Buffer>&& vertBuffer) {
    vertBuffers_.emplace_back(std::move(vertBuffer));
  }

  // TODO: Give ability to set a relative viewport and scissor

  void Allocate(std::shared_ptr<Device> const&, uint32_t const numFramebuffers);

  void Record(std::shared_ptr<Device>& device, vk::UniquePipeline const&,
              vk::UniqueRenderPass const&, std::vector<Framebuffer> const&,
              vk::Extent2D const&);

  void Draw(uint32_t const imageIndex, std::shared_ptr<Device> const& device) {
    device->SubmitToGraphics(cmdBuffers_[imageIndex]);
  }

 private:
  std::vector<vk::UniqueCommandBuffer> cmdBuffers_;
  std::vector<std::unique_ptr<Buffer>> vertBuffers_;
};
