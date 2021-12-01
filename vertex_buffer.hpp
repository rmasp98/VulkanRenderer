#pragma once

#include <memory>
#include <unordered_map>

#include "device.hpp"
#include "uniform_buffer.hpp"
#include "vulkan/vulkan.hpp"

class Buffer {
 public:
  Buffer() = default;
  virtual ~Buffer() = default;

  virtual void Upload(std::shared_ptr<Device>&,
                      std::unordered_map<vk::ShaderStageFlagBits,
                                         vk::DescriptorSetLayout> const&) = 0;
  virtual void Remove() = 0;
  virtual bool IsUploaded() const = 0;

  virtual void AssignUniformBuffer(std::shared_ptr<UniformBuffer>) = 0;

  virtual void Bind(vk::UniqueCommandBuffer const&) const = 0;
  virtual void Record(vk::UniqueCommandBuffer const&) const = 0;
};

// TODO: Might use this later to abstract
// struct AttributeDetails {
//  uint32_t Location;
//  vk::Format Format;
//  uint32_t Offset;
//};

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

inline vk::PipelineVertexInputStateCreateInfo GetPipelineVertexInput() {
  static std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
  static std::vector<vk::VertexInputBindingDescription> bindingDescriptons;

  // TODO: this is a crap way of doing this
  if (attributeDescriptions.size() == 0) {
    // ColourVertex2D
    uint32_t cv2DBinding = 0;
    auto cv2DAttrDesc = ColouredVertex2D::GetAttributeDetails(cv2DBinding);
    attributeDescriptions.insert(attributeDescriptions.end(),
                                 cv2DAttrDesc.begin(), cv2DAttrDesc.end());
    bindingDescriptons.push_back({cv2DBinding, sizeof(ColouredVertex2D)});
  }

  return {{}, bindingDescriptons, attributeDescriptions};
}

// TODO: can we make a Buffer with multiple datasets
template <class T>
class VertexBuffer : public Buffer {
 public:
  VertexBuffer(std::vector<T> const& data) : data_(data) {}
  virtual ~VertexBuffer() = default;

  virtual void Upload(
      std::shared_ptr<Device>& device,
      std::unordered_map<vk::ShaderStageFlagBits,
                         vk::DescriptorSetLayout> const& descriptorSetLayouts) {
    buffer_ =
        device->UploadBuffer(data_.data(), data_.size() * sizeof(T),
                             vk::BufferUsageFlagBits::eVertexBuffer, true);
    for (auto& uniformBufferPtr : uniformBuffers_) {
      if (auto uniformBuffer = uniformBufferPtr.lock()) {
        // TODO: get number of framebuffers
        uniformBuffer->AllocateBuffers(device, descriptorSetLayouts, 1);
      }
    }
  }

  virtual void Remove() { buffer_.reset(); }

  virtual bool IsUploaded() const { return buffer_ != nullptr; }

  virtual void AssignUniformBuffer(
      std::shared_ptr<UniformBuffer> uniformBuffer) {
    uniformBuffers_.push_back(uniformBuffer);
  }

  virtual void Record(vk::UniqueCommandBuffer const& cmdBuffer) const {
    Bind(cmdBuffer);
    cmdBuffer->draw(data_.size(), 1, 0, 0);
  }

  virtual void Bind(vk::UniqueCommandBuffer const& cmdBuffer) const {
    buffer_->Bind(cmdBuffer);
  }

 protected:
  std::vector<T> const data_;
  std::unique_ptr<BufferRef> buffer_;
  std::vector<std::weak_ptr<UniformBuffer>> uniformBuffers_;
};

// TODO: see if we can figure out putting both in same buffer
template <class T>
class IndexBuffer : public Buffer {
 public:
  IndexBuffer(std::vector<T> const& data, std::vector<uint16_t> indicies)
      : vertexBuffer_(data), indicies_(indicies) {}
  virtual ~IndexBuffer() = default;

  virtual void Upload(
      std::shared_ptr<Device>& device,
      std::unordered_map<vk::ShaderStageFlagBits,
                         vk::DescriptorSetLayout> const& descriptorSetLayouts) {
    vertexBuffer_.Upload(device, descriptorSetLayouts);
    indexBuffer_ = device->UploadBuffer(
        indicies_.data(), indicies_.size() * sizeof(uint16_t),
        vk::BufferUsageFlagBits::eIndexBuffer, true);
  }

  virtual void Remove() {
    vertexBuffer_.Remove();
    indexBuffer_.reset();
  }

  virtual bool IsUploaded() const {
    return vertexBuffer_.IsUploaded() && indexBuffer_ != nullptr;
  }

  virtual void AssignUniformBuffer(
      std::shared_ptr<UniformBuffer> uniformBuffer) {
    uniformBuffers_.push_back(uniformBuffer);
  }

  virtual void Record(vk::UniqueCommandBuffer const& cmdBuffer) const {
    Bind(cmdBuffer);
    cmdBuffer->drawIndexed(indicies_.size(), 1, 0, 0, 0);
  }

  virtual void Bind(vk::UniqueCommandBuffer const& cmdBuffer) const {
    vertexBuffer_.Bind(cmdBuffer);
    indexBuffer_->BindIndex(cmdBuffer);
  }

 private:
  VertexBuffer<T> vertexBuffer_;
  std::vector<uint16_t> const indicies_;
  std::unique_ptr<BufferRef> indexBuffer_;
  std::vector<std::weak_ptr<UniformBuffer>> uniformBuffers_;
};
