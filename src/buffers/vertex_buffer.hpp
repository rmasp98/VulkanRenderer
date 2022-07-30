#ifndef VULKAN_RENDERER_VERTEX_BUFFER_HPP
#define VULKAN_RENDERER_VERTEX_BUFFER_HPP

#include "device_api.hpp"
#include "device_buffer.hpp"
#include "queues.hpp"
#include "uniform_buffer.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

struct ColouredVertex2D {
  float Vertex[2];  // location 0
  float Colour[3];  // location 1

  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDetails(
      uint32_t binding) {
    return {{0, binding, vk::Format::eR32G32Sfloat,
             offsetof(ColouredVertex2D, Vertex)},
            {1, binding, vk::Format::eR32G32B32Sfloat,
             offsetof(ColouredVertex2D, Colour)}};
  }
};

struct UVVertex2D {
  float Vertex[2];  // location 0
  float UV[2];      // location 1

  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDetails(
      uint32_t binding) {
    return {
        {0, binding, vk::Format::eR32G32Sfloat, offsetof(UVVertex2D, Vertex)},
        {1, binding, vk::Format::eR32G32Sfloat, offsetof(UVVertex2D, UV)}};
  }
};

struct UVColouredVertex3D {
  std::array<float, 3> Vertex;  // location 0
  std::array<float, 3> Colour;  // location 1
  std::array<float, 2> UV;      // location 2

  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDetails(
      uint32_t binding) {
    return {{0, binding, vk::Format::eR32G32B32Sfloat,
             offsetof(UVColouredVertex3D, Vertex)},
            {1, binding, vk::Format::eR32G32B32Sfloat,
             offsetof(UVColouredVertex3D, Colour)},
            {2, binding, vk::Format::eR32G32Sfloat,
             offsetof(UVColouredVertex3D, UV)}};
  }

  auto operator<=>(UVColouredVertex3D const& rhs) const = default;
};

class Buffer {
 public:
  virtual ~Buffer() = default;
  virtual void AddUniform(std::shared_ptr<Uniform> const&) = 0;
  virtual void AddPushConstant(std::shared_ptr<PushConstant> const&) = 0;

  virtual void Initialise(DescriptorSetLayouts&, Queues const&, DeviceApi&,
                          bool force = false) = 0;

  virtual void Upload(Queues const&, DeviceApi&) = 0;
  virtual void UploadUniforms(ImageIndex const, Queues const&, DeviceApi&) = 0;
  virtual void UploadPushConstants(vk::UniquePipelineLayout const&,
                                   vk::UniqueCommandBuffer const&) = 0;

  virtual void Bind(ImageIndex const, vk::UniquePipelineLayout const&,
                    vk::UniqueCommandBuffer const&) const = 0;
  virtual void Draw(vk::UniqueCommandBuffer const&) const = 0;
};

template <class T>
class VertexBuffer : public Buffer {
 public:
  VertexBuffer(std::vector<T> const& data) : data_(data) {}

  void AddUniform(std::shared_ptr<Uniform> const& uniform) override {
    // TODO: Check to see if set/binding already exists
    uniforms_.push_back(uniform);
  }

  void AddPushConstant(
      std::shared_ptr<PushConstant> const& pushConstant) override {
    // TODO: Check to see if set/binding already exists
    pushConstants_.push_back(pushConstant);
  }

  void Initialise(DescriptorSetLayouts& descriptorSetLayouts,
                  Queues const& queues, DeviceApi& device, bool force) {
    if (force || deviceBuffer_ == nullptr) {
      deviceBuffer_ = std::make_unique<OptimisedDeviceBuffer>(
          data_.size() * sizeof(T), vk::BufferUsageFlagBits::eVertexBuffer,
          device);
      Upload(queues, device);

      descriptorSets_ = descriptorSetLayouts.CreateDescriptorSets(device);

      for (auto& uniform : uniforms_) {
        uniform->Allocate(queues, device);
        for (uint32_t i = 0; i < device.GetNumSwapchainImages(); ++i) {
          uniform->AddDescriptorSetUpdate(i, descriptorSets_);
        }
      }
      descriptorSets_->SubmitUpdates(device);
    }
  }

  void Upload(Queues const& queues, DeviceApi& device) override {
    // TODO: maybe check to see if already created and right size
    if (deviceBuffer_->IsOutdated()) {
      deviceBuffer_->Upload(data_.data(), queues, device);
    }
  }

  void UploadUniforms(ImageIndex const imageIndex, Queues const& queues,
                      DeviceApi& device) override {
    for (auto& uniform : uniforms_) {
      // Only allocates and uploads if needed
      uniform->Allocate(queues, device);
      uniform->Upload(imageIndex, queues, device);
    }
  }

  void UploadPushConstants(vk::UniquePipelineLayout const& layout,
                           vk::UniqueCommandBuffer const& cmdBuffer) override {
    for (auto& pushConstant : pushConstants_) {
      pushConstant->Upload(layout, cmdBuffer);
    }
  }

  void Bind(ImageIndex const imageIndex,
            vk::UniquePipelineLayout const& pipelineLayout,
            vk::UniqueCommandBuffer const& cmdBuffer) const override {
    if (deviceBuffer_ && descriptorSets_) {
      deviceBuffer_->BindVertex(cmdBuffer);
      descriptorSets_->Bind(imageIndex, cmdBuffer, pipelineLayout);
    }
  }

  virtual void Draw(vk::UniqueCommandBuffer const& cmdBuffer) const override {
    cmdBuffer->draw(data_.size(), 1, 0, 0);
  }

 private:
  std::vector<T> const data_;
  std::unique_ptr<OptimisedDeviceBuffer> deviceBuffer_;
  std::vector<std::shared_ptr<Uniform>> uniforms_;
  std::vector<std::shared_ptr<PushConstant>> pushConstants_;
  std::unique_ptr<DescriptorSets> descriptorSets_;
};

template <class T>
class IndexBuffer : public Buffer {
 public:
  IndexBuffer(std::vector<T> const& data, std::vector<uint32_t> const& indices)
      : vertexBuffer_(data), indices_(indices) {}

  virtual void AddUniform(std::shared_ptr<Uniform> const& uniform) override {
    vertexBuffer_.AddUniform(uniform);
  }

  void AddPushConstant(
      std::shared_ptr<PushConstant> const& pushConstant) override {
    vertexBuffer_.AddPushConstant(pushConstant);
  }

  void Initialise(DescriptorSetLayouts& descriptorSetLayouts,
                  Queues const& queues, DeviceApi& device, bool force) {
    vertexBuffer_.Initialise(descriptorSetLayouts, queues, device, force);
    if (force || deviceBuffer_ == nullptr) {
      deviceBuffer_ = std::make_unique<OptimisedDeviceBuffer>(
          indices_.size() * sizeof(uint32_t),
          vk::BufferUsageFlagBits::eIndexBuffer, device);
      Upload(queues, device);
    }
  }

  virtual void Upload(Queues const& queues, DeviceApi& device) override {
    vertexBuffer_.Upload(queues, device);
    if (deviceBuffer_->IsOutdated()) {
      deviceBuffer_->Upload(indices_.data(), queues, device);
    }
  }

  void UploadUniforms(ImageIndex const imageIndex, Queues const& queues,
                      DeviceApi& device) override {
    vertexBuffer_.UploadUniforms(imageIndex, queues, device);
  }

  void UploadPushConstants(vk::UniquePipelineLayout const& layout,
                           vk::UniqueCommandBuffer const& cmdBuffer) override {
    vertexBuffer_.UploadPushConstants(layout, cmdBuffer);
  }

  virtual void Bind(ImageIndex const imageIndex,
                    vk::UniquePipelineLayout const& pipelineLayout,
                    vk::UniqueCommandBuffer const& cmdBuffer) const override {
    vertexBuffer_.Bind(imageIndex, pipelineLayout, cmdBuffer);
    if (deviceBuffer_) {
      deviceBuffer_->BindIndex(cmdBuffer);
    }
  }

  virtual void Draw(vk::UniqueCommandBuffer const& cmdBuffer) const override {
    cmdBuffer->drawIndexed(indices_.size(), 1, 0, 0, 0);
  }

 private:
  VertexBuffer<T> vertexBuffer_;
  std::vector<uint32_t> indices_;
  std::unique_ptr<OptimisedDeviceBuffer> deviceBuffer_;
};

}  // namespace vulkan_renderer

#endif