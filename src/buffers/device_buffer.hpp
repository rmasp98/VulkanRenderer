#pragma once

#include "device_api.hpp"
#include "memory.hpp"
#include "queues.hpp"

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

  void Upload(void const* data, Queues const& queues, DeviceApi& device) {
    if (optimise_) {
      TransferToOptimisedMemory(data, queues, device);
    } else {
      auto memoryLocation = device.MapMemory(id_);
      memcpy(memoryLocation, data, size_);
      device.UnmapMemory(id_);
    }
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

  void TransferToOptimisedMemory(void const* data, Queues const& queues,
                                 DeviceApi& device) const {
    auto transferBuffer =
        device.CreateBuffer(size_, vk::BufferUsageFlagBits::eTransferSrc);
    auto transferId = device.AllocateMemory(
        transferBuffer, vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent);
    auto memoryLocation = device.MapMemory(transferId);
    memcpy(memoryLocation, data, size_);
    device.UnmapMemory(transferId);

    auto cmdBuffer =
        device.AllocateCommandBuffers(vk::CommandBufferLevel::ePrimary, 1);
    cmdBuffer[0]->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    vk::BufferCopy copyRegion{0, 0, size_};
    cmdBuffer[0]->copyBuffer(transferBuffer.get(), buffer_.get(), copyRegion);
    cmdBuffer[0]->end();

    queues.SubmitToGraphics(cmdBuffer[0]);
    // TODO: remove if we can deallocate in response to completion in callback
    queues.GraphicsWaitIdle();
    device.DeallocateMemory(transferId);
  }
};