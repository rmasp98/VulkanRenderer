#pragma once

#include "device_api.hpp"
#include "memory.hpp"

inline vk::BufferUsageFlags GetBufferFlags(
    vk::BufferUsageFlags const bufferUsage, bool const optimise) {
  if (optimise) {
    return bufferUsage | vk::BufferUsageFlagBits::eTransferDst;
  }
  return bufferUsage;
}

inline vk::MemoryPropertyFlags GetMemoryFlags(bool const optimise) {
  if (optimise) {
    return vk::MemoryPropertyFlagBits::eDeviceLocal;
  }
  return vk::MemoryPropertyFlagBits::eHostVisible |
         vk::MemoryPropertyFlagBits::eHostCoherent;
}

class DeviceBuffer {
 public:
  DeviceBuffer(uint32_t const size, vk::BufferUsageFlags const bufferUsage,
               bool const optimise, DeviceApi& device)
      : buffer_(
            device.CreateBuffer(size, GetBufferFlags(bufferUsage, optimise))),
        isOutdated_(true),
        id_(device.AllocateMemory(buffer_, GetMemoryFlags(optimise))),
        size_(size),
        offset_(device.GetMemoryOffset(id_)),
        optimise_(optimise) {}

  void Upload(void const* data, DeviceApi const& device) {
    auto memoryLocation = device.MapMemory(id_);
    memcpy(memoryLocation, data, size_);
    device.UnmapMemory(id_);
    isOutdated_ = false;
  }

  void UpdateDescriptorSet(vk::UniqueDescriptorSet const& descriptorSet,
                           vk::DeviceSize const size, DeviceApi const& device) {
    vk::DescriptorBufferInfo bufferInfo{buffer_.get(), 0, size};
    vk::WriteDescriptorSet writeSet{descriptorSet.get(),
                                    0,
                                    0,
                                    1,
                                    vk::DescriptorType::eUniformBuffer,
                                    nullptr,
                                    &bufferInfo,
                                    nullptr};
    device.UpdateDescriptorSet(writeSet);
  }

  void SetOutdated() { isOutdated_ = true; }
  bool IsOutdated() { return isOutdated_; }

  void Bind(vk::UniqueCommandBuffer const& cmdBuffer) {
    cmdBuffer->bindVertexBuffers(0, buffer_.get(), {offset_});
  }

  void BindIndex(vk::UniqueCommandBuffer const& cmdBuffer) {
    cmdBuffer->bindIndexBuffer(buffer_.get(), offset_, vk::IndexType::eUint16);
  }

 private:
  vk::UniqueBuffer buffer_;
  bool isOutdated_;
  uint32_t id_;
  uint32_t size_;
  uint32_t offset_;
  bool optimise_;
};

struct MVP {
  float data[16];
};

class UniformBuffer {
 public:
  virtual ~UniformBuffer() = default;

  virtual void AllocateBuffer(DeviceApi& device) = 0;
  virtual void DeallocateBuffer() = 0;

  template <class T>
  void Update(T const& data) {
    if (typeid(T) == GetTypeId()) {
      UpdateInternal(static_cast<void const*>(&data));
    }
  }

  virtual void Upload(uint32_t const bufferId, DeviceApi const& device) = 0;

  virtual void Bind(uint32_t const bufferId,
                    vk::UniqueCommandBuffer const& cmdBuffer,
                    vk::UniquePipelineLayout const& layout) const = 0;

  virtual vk::DescriptorSetLayout GetLayout() = 0;

 protected:
  virtual void UpdateInternal(void const* data) = 0;
  virtual const std::type_info& GetTypeId() const = 0;
};

template <class T>
class UniformBufferImpl : public UniformBuffer {
 public:
  UniformBufferImpl(vk::UniqueDescriptorSetLayout&& layout)
      : layout_(std::move(layout)) {}

  virtual void AllocateBuffer(DeviceApi& device) override {
    auto numBuffers = device.GetNumSwapchainImages();
    descriptorSets_ = device.AllocateDescriptorSet(layout_.get());

    for (uint32_t i = 0; i < numBuffers; ++i) {
      DeviceBuffer buffer{sizeof(T), vk::BufferUsageFlagBits::eUniformBuffer,
                          false, device};
      buffer.UpdateDescriptorSet(descriptorSets_[i], sizeof(T), device);
      deviceBuffers_.push_back(std::move(buffer));
    }
  }

  virtual void DeallocateBuffer() override {
    deviceBuffers_.clear();
    descriptorSets_.clear();
  }

  void Update(T const& data) {
    data_ = data;
    for (auto& buffer : deviceBuffers_) {
      buffer.SetOutdated();
    }
  }

  virtual void Upload(uint32_t const bufferId,
                      DeviceApi const& device) override {
    if (bufferId < deviceBuffers_.size() &&
        deviceBuffers_[bufferId].IsOutdated()) {
      deviceBuffers_[bufferId].Upload(&data_, device);
    }
  }

  virtual void Bind(uint32_t const bufferId,
                    vk::UniqueCommandBuffer const& cmdBuffer,
                    vk::UniquePipelineLayout const& layout) const override {
    if (bufferId < descriptorSets_.size()) {
      cmdBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                    layout.get(), 0,
                                    {descriptorSets_[bufferId].get()}, {});
    }
  }

  virtual vk::DescriptorSetLayout GetLayout() { return layout_.get(); };

 protected:
  virtual void UpdateInternal(void const* data) override {
    Update(*static_cast<T const*>(data));
  }

  virtual const std::type_info& GetTypeId() const override { return typeid(T); }

 private:
  T data_;
  std::vector<vk::UniqueDescriptorSet> descriptorSets_;
  std::vector<DeviceBuffer> deviceBuffers_;
  vk::UniqueDescriptorSetLayout layout_;
};