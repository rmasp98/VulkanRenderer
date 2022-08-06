#ifndef VULKAN_RENDERER_DEVICE_BUFFER_HPP
#define VULKAN_RENDERER_DEVICE_BUFFER_HPP

#include "descriptor_sets.hpp"
#include "device_api.hpp"
#include "image_properties.hpp"
#include "memory.hpp"
#include "queues.hpp"

namespace vulkan_renderer {

class DeviceBuffer {
 public:
  DeviceBuffer(uint32_t const size, vk::BufferUsageFlags const bufferUsage,
               DeviceApi& device,
               vk::MemoryPropertyFlags const memoryFlags =
                   vk::MemoryPropertyFlagBits::eHostVisible |
                   vk::MemoryPropertyFlagBits::eHostCoherent)
      : buffer_(device.CreateBuffer(size, bufferUsage)),
        isOutdated_(true),
        allocation_(device.AllocateMemory(buffer_.get(), memoryFlags)),
        size_(size),
        offset_(device.GetMemoryOffset(allocation_)),
        bufferInfo_(GetBuffer(), 0, size_) {}

  virtual ~DeviceBuffer() = default;
  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer(DeviceBuffer&&) = default;

  virtual void Upload(void const* data, Queues const&, DeviceApi&);

  void AddDescriptorSetUpdate(uint32_t const set, ImageIndex const,
                              vk::WriteDescriptorSet&, DescriptorSets&) const;

  void SetOutdated() { isOutdated_ = true; }
  bool IsOutdated() const { return isOutdated_; }

  void BindVertex(vk::CommandBuffer const& cmdBuffer) const {
    cmdBuffer.bindVertexBuffers(0, buffer_.get(), {offset_});
  }

  void BindIndex(vk::CommandBuffer const& cmdBuffer) const {
    cmdBuffer.bindIndexBuffer(buffer_.get(), offset_, vk::IndexType::eUint32);
  }

 protected:
  vk::Buffer const& GetBuffer() { return buffer_.get(); }
  uint32_t const& GetSize() { return size_; }

 private:
  vk::UniqueBuffer buffer_;
  bool isOutdated_;
  Allocation allocation_;
  uint32_t size_;
  uint32_t offset_;
  bool optimise_;
  vk::DescriptorBufferInfo bufferInfo_;
};

class TransferDeviceBuffer : protected DeviceBuffer {
 public:
  TransferDeviceBuffer(uint32_t const size, DeviceApi& device)
      : DeviceBuffer(size, vk::BufferUsageFlagBits::eTransferSrc, device) {}

  // Expose it publicly
  virtual void Upload(void const* data, Queues const& queues,
                      DeviceApi& device) override {
    DeviceBuffer::Upload(data, queues, device);
  }

  void CopyToBuffer(vk::Buffer const& targetBuffer, Queues const&, DeviceApi&);

  void CopyToImage(vk::Image const& targetImage, ImageProperties&,
                   Queues const&, DeviceApi&);
};

class OptimisedDeviceBuffer : public DeviceBuffer {
 public:
  OptimisedDeviceBuffer(uint32_t const size,
                        vk::BufferUsageFlags const bufferUsage,
                        DeviceApi& device)
      : DeviceBuffer(size, bufferUsage | vk::BufferUsageFlagBits::eTransferDst,
                     device, vk::MemoryPropertyFlagBits::eDeviceLocal) {}

  virtual void Upload(void const* data, Queues const&, DeviceApi&) override;
};

void TransitionImageLayout(vk::Image const&, uint32_t const mipLevel,
                           uint32_t const numMipLevels, vk::Format const,
                           vk::ImageLayout const sourceLayout,
                           vk::ImageLayout const destinationLayout,
                           vk::CommandBuffer const&);

void GenerateMipMaps(vk::Image const&, ImageProperties const&,
                     vk::CommandBuffer const&);

}  // namespace vulkan_renderer

#endif