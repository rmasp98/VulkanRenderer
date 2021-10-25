#pragma once

#include <iostream>
#include <memory>
#include <utility>

#include "device.hpp"
#include "utils.hpp"
#include "vulkan/vulkan.hpp"

class UniformBuffer {
 public:
  UniformBuffer() = default;
  ~UniformBuffer() = default;

  // TODO: figure out how to map T to layout
  virtual void AllocateBuffers(std::shared_ptr<Device> const& device,
                               uint32_t const numBuffers) = 0;

  virtual void Upload(std::shared_ptr<Device> const& device,
                      uint32_t const bufferId) = 0;
  virtual void Remove() = 0;

  virtual bool IsUploaded(uint32_t const bufferId) const = 0;

  virtual void Bind(vk::UniqueCommandBuffer const& cmdBuffer,
                    vk::UniquePipelineLayout const& layout,
                    uint32_t const bufferId) const = 0;
};

struct MVP {
  float data[16];
};

class UniformBufferMVP : public UniformBuffer {
 public:
  UniformBufferMVP(MVP const& data) : data_(data) {}
  ~UniformBufferMVP() = default;

  void Update(MVP const& data) {
    data_ = data;
    isUploaded_.assign(isUploaded_.size(), false);
  }

  // TODO: figure out how to map T to layout
  void AllocateBuffers(std::shared_ptr<Device> const& device,
                       uint32_t const numBuffers) {
    buffers_.clear();
    for (uint32_t i = 0; i < numBuffers; ++i) {
      buffers_.emplace_back(device->UploadBuffer(
          &data_, sizeof(MVP), vk::BufferUsageFlagBits::eUniformBuffer, false));
    }
    isUploaded_.resize(numBuffers, true);
    descriptorSets_ = device->AllocateDescriptorSet(descriptorSetLayout_.get());
  }

  void Upload(std::shared_ptr<Device> const& device, uint32_t const bufferId) {
    if (!IsUploaded(bufferId)) {
      device->UploadBuffer(buffers_[bufferId], &data_);
      isUploaded_[bufferId] = true;
    }
  }

  void Remove() {
    for (auto& buffer : buffers_) {
      buffer.reset();
    }
  }

  bool IsUploaded(uint32_t const bufferId) const {
    return isUploaded_[bufferId];
  }

  void Bind(vk::UniqueCommandBuffer const& cmdBuffer,
            vk::UniquePipelineLayout const& layout,
            uint32_t const bufferId) const {
    cmdBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                  layout.get(), 0,
                                  {descriptorSets_[bufferId].get()}, {});
  }

  static vk::DescriptorSetLayout& GetLayout(
      std::shared_ptr<Device> const& device) {
    descriptorSetLayout_ = device->CreateDescriptorSetLayout({{}, bindings_});
    return descriptorSetLayout_.get();
  }

  static void DeleteLayout() { descriptorSetLayout_.reset(); }

  static void SetBindings(
      std::vector<vk::DescriptorSetLayoutBinding> bindings) {
    bindings_ = bindings;
  }

 private:
  MVP data_;
  std::vector<std::unique_ptr<BufferRef>> buffers_;
  std::vector<bool> isUploaded_;
  inline static std::vector<vk::DescriptorSetLayoutBinding> bindings_{
      {0, vk::DescriptorType::eUniformBuffer, 1,
       vk::ShaderStageFlagBits::eVertex}};
  inline static vk::UniqueDescriptorSetLayout descriptorSetLayout_;
  std::vector<vk::UniqueDescriptorSet> descriptorSets_;
};

inline static std::vector<vk::DescriptorSetLayout> GetDescriptorLayouts(
    std::shared_ptr<Device> const& device) {
  return {UniformBufferMVP::GetLayout(device)};
}

inline static void RemoveDescriptorLayouts() {
  UniformBufferMVP::DeleteLayout();
}

class Buffer {
 public:
  Buffer() = default;
  virtual ~Buffer() = default;

  virtual void Upload(std::shared_ptr<Device>&) = 0;
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

static inline vk::PipelineVertexInputStateCreateInfo GetPipelineVertexInput() {
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

  virtual void Upload(std::shared_ptr<Device>& device) {
    buffer_ =
        device->UploadBuffer(data_.data(), data_.size() * sizeof(T),
                             vk::BufferUsageFlagBits::eVertexBuffer, true);
    for (auto& uniformBufferPtr : uniformBuffers_) {
      if (auto uniformBuffer = uniformBufferPtr.lock()) {
        // TODO: get number of framebuffers
        uniformBuffer->AllocateBuffers(device, 1);
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

  virtual void Upload(std::shared_ptr<Device>& device) {
    vertexBuffer_.Upload(device);
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
