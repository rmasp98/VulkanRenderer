#pragma once

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
               bool const optimise, DeviceApi const& device,
               std::shared_ptr<MemoryAllocator>& allocator)
      : buffer_(
            device.CreateBuffer(size, GetBufferFlags(bufferUsage, optimise))),
        id_(allocator->Allocate(device, buffer_, GetMemoryFlags(optimise))),
        size_(size),
        offset_(allocator->GetOffset(id_)),
        optimise_(optimise),
        allocator_(allocator) {}

  void Upload(void const* data, DeviceApi const& device) {
    if (auto allocator = allocator_.lock()) {
      auto memoryLocation = allocator->MapMemory(device, id_);
      memcpy(memoryLocation, data, size_);
      allocator->UnMapMemory(device, id_);
    }
  }

  void Bind(vk::UniqueCommandBuffer const& cmdBuffer) {
    cmdBuffer->bindVertexBuffers(0, buffer_.get(), {offset_});
  }

  void BindIndex(vk::UniqueCommandBuffer const& cmdBuffer) {
    cmdBuffer->bindIndexBuffer(buffer_.get(), offset_, vk::IndexType::eUint16);
  }

 private:
  vk::UniqueBuffer buffer_;
  uint32_t id_;
  uint32_t size_;
  uint32_t offset_;
  bool optimise_;
  std::weak_ptr<MemoryAllocator> allocator_;
};

class Buffer {
 public:
  virtual ~Buffer() = default;
  virtual void Upload(DeviceApi&, std::shared_ptr<MemoryAllocator>&) = 0;
  virtual bool IsUploaded() const = 0;
  virtual void Bind(vk::UniqueCommandBuffer const&) const = 0;
  virtual void Record(vk::UniqueCommandBuffer const&) const = 0;
};

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

template <class T>
class VertexBuffer : public Buffer {
 public:
  VertexBuffer(std::vector<T> const& data) : data_(data) {}

  virtual void Upload(DeviceApi& device,
                      std::shared_ptr<MemoryAllocator>& allocator) override {
    // TODO: maybe check to see if already created and right size
    deviceBuffer_ = std::make_unique<DeviceBuffer>(
        data_.size() * sizeof(T), vk::BufferUsageFlagBits::eVertexBuffer, false,
        device, allocator);
    deviceBuffer_->Upload(data_.data(), device);
  }

  virtual bool IsUploaded() const override { return deviceBuffer_ != nullptr; }

  virtual void Bind(vk::UniqueCommandBuffer const& cmdBuffer) const override {
    if (deviceBuffer_) {
      deviceBuffer_->Bind(cmdBuffer);
    }
  }

  virtual void Record(vk::UniqueCommandBuffer const& cmdBuffer) const override {
    Bind(cmdBuffer);
    cmdBuffer->draw(data_.size(), 1, 0, 0);
  }

 private:
  std::vector<T> const data_;
  std::unique_ptr<DeviceBuffer> deviceBuffer_;
};

template <class T>
class IndexBuffer : public Buffer {
 public:
  IndexBuffer(std::vector<T> const& data, std::vector<uint16_t> const& indices)
      : vertexBuffer_(data), indices_(indices) {}

  virtual void Upload(DeviceApi& device,
                      std::shared_ptr<MemoryAllocator>& allocator) override {
    // TODO: maybe check to see if already created and right size
    vertexBuffer_.Upload(device, allocator);
    deviceBuffer_ = std::make_unique<DeviceBuffer>(
        indices_.size() * sizeof(uint16_t),
        vk::BufferUsageFlagBits::eIndexBuffer, false, device, allocator);
    deviceBuffer_->Upload(indices_.data(), device);
  }

  virtual bool IsUploaded() const override {
    return vertexBuffer_.IsUploaded() && deviceBuffer_ != nullptr;
  }

  virtual void Bind(vk::UniqueCommandBuffer const& cmdBuffer) const override {
    vertexBuffer_.Bind(cmdBuffer);
    deviceBuffer_->BindIndex(cmdBuffer);
  }

  virtual void Record(vk::UniqueCommandBuffer const& cmdBuffer) const override {
    Bind(cmdBuffer);
    cmdBuffer->drawIndexed(indices_.size(), 1, 0, 0, 0);
  }

 private:
  VertexBuffer<T> vertexBuffer_;
  std::vector<uint16_t> indices_;
  std::unique_ptr<DeviceBuffer> deviceBuffer_;
};