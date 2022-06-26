#pragma once

#include "device_buffer.hpp"
#include "queues.hpp"
#include "uniform_buffer.hpp"

class Buffer {
 public:
  virtual ~Buffer() = default;
  virtual void SetUniformData(vk::ShaderStageFlagBits const,
                              std::shared_ptr<UniformData> const&) = 0;
  virtual void Upload(Queues const&, DeviceApi&) = 0;

  virtual void UpdateUniformBuffer(
      std::unordered_map<vk::ShaderStageFlagBits,
                         std::shared_ptr<UniformBuffer>>&,
      uint32_t const bufferId, Queues const&, DeviceApi&) = 0;

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

  virtual void SetUniformData(
      vk::ShaderStageFlagBits const shader,
      std::shared_ptr<UniformData> const& data) override {
    uniformData_[shader] = data;
  }

  virtual void Upload(Queues const& queues, DeviceApi& device) override {
    // TODO: maybe check to see if already created and right size
    deviceBuffer_ = std::make_unique<DeviceBuffer>(
        data_.size() * sizeof(T), vk::BufferUsageFlagBits::eVertexBuffer, true,
        device);
    deviceBuffer_->Upload(data_.data(), queues, device);
  }

  virtual void UpdateUniformBuffer(
      std::unordered_map<vk::ShaderStageFlagBits,
                         std::shared_ptr<UniformBuffer>>& buffers,
      uint32_t const bufferId, Queues const& queues, DeviceApi& device) {
    for (auto& data : uniformData_) {
      if (buffers.contains(data.first)) {
        data.second->UpdateBuffer(buffers[data.first]);
        buffers[data.first]->Upload(bufferId, queues, device);
      }
    }
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
  std::unordered_map<vk::ShaderStageFlagBits, std::shared_ptr<UniformData>>
      uniformData_;
};

template <class T>
class IndexBuffer : public Buffer {
 public:
  IndexBuffer(std::vector<T> const& data, std::vector<uint16_t> const& indices)
      : vertexBuffer_(data), indices_(indices) {}

  virtual void SetUniformData(
      vk::ShaderStageFlagBits const shader,
      std::shared_ptr<UniformData> const& data) override {
    uniformData_[shader] = data;
  }

  virtual void Upload(Queues const& queues, DeviceApi& device) override {
    // TODO: maybe check to see if already created and right size
    vertexBuffer_.Upload(queues, device);
    deviceBuffer_ = std::make_unique<DeviceBuffer>(
        indices_.size() * sizeof(uint16_t),
        vk::BufferUsageFlagBits::eIndexBuffer, true, device);
    deviceBuffer_->Upload(indices_.data(), queues, device);
  }

  virtual void UpdateUniformBuffer(
      std::unordered_map<vk::ShaderStageFlagBits,
                         std::shared_ptr<UniformBuffer>>& buffers,
      uint32_t const bufferId, Queues const& queues, DeviceApi& device) {
    for (auto& data : uniformData_) {
      if (buffers.contains(data.first)) {
        data.second->UpdateBuffer(buffers[data.first]);
        buffers[data.first]->Upload(bufferId, queues, device);
      }
    }
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
  std::unordered_map<vk::ShaderStageFlagBits, std::shared_ptr<UniformData>>
      uniformData_;
};