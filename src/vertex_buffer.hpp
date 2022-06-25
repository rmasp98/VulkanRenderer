#pragma once

#include "uniform_buffer.hpp"

class UniformData {
 public:
  virtual void UpdateBuffer(std::shared_ptr<UniformBuffer>&) const = 0;
};

template <class T>
class UniformDataImpl : public UniformData {
 public:
  UniformDataImpl(T const& data) : data_(data) {}

  void Set(T const& data) { data_ = data; }

  virtual void UpdateBuffer(
      std::shared_ptr<UniformBuffer>& buffer) const override {
    buffer->Update(data_);
  }

 private:
  T data_;
};

class Buffer {
 public:
  virtual ~Buffer() = default;
  virtual void SetUniformData(vk::ShaderStageFlagBits const,
                              std::shared_ptr<UniformData> const&) = 0;
  virtual void Upload(DeviceApi&) = 0;

  virtual void UpdateUniformBuffer(
      std::unordered_map<vk::ShaderStageFlagBits,
                         std::shared_ptr<UniformBuffer>>&,
      uint32_t const bufferId, DeviceApi const&) = 0;

  virtual bool IsUploaded() const = 0;
  virtual void Bind(vk::UniqueCommandBuffer const&,
                    vk::UniquePipelineLayout const&) const = 0;
  virtual void Record(vk::UniqueCommandBuffer const&,
                      vk::UniquePipelineLayout const&) const = 0;
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

  virtual void Upload(DeviceApi& device) override {
    // TODO: maybe check to see if already created and right size
    deviceBuffer_ = std::make_unique<DeviceBuffer>(
        data_.size() * sizeof(T), vk::BufferUsageFlagBits::eVertexBuffer, false,
        device);
    deviceBuffer_->Upload(data_.data(), device);
  }

  virtual void UpdateUniformBuffer(
      std::unordered_map<vk::ShaderStageFlagBits,
                         std::shared_ptr<UniformBuffer>>& buffers,
      uint32_t const bufferId, DeviceApi const& device) {
    for (auto& data : uniformData_) {
      if (buffers.contains(data.first)) {
        data.second->UpdateBuffer(buffers[data.first]);
        buffers[data.first]->Upload(bufferId, device);
      }
    }
  }

  virtual bool IsUploaded() const override { return deviceBuffer_ != nullptr; }

  virtual void Bind(
      vk::UniqueCommandBuffer const& cmdBuffer,
      vk::UniquePipelineLayout const& pipelineLayout) const override {
    if (deviceBuffer_) {
      deviceBuffer_->Bind(cmdBuffer);
    }
  }

  virtual void Record(
      vk::UniqueCommandBuffer const& cmdBuffer,
      vk::UniquePipelineLayout const& pipelineLayout) const override {
    Bind(cmdBuffer, pipelineLayout);
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

  virtual void Upload(DeviceApi& device) override {
    // TODO: maybe check to see if already created and right size
    vertexBuffer_.Upload(device);
    deviceBuffer_ = std::make_unique<DeviceBuffer>(
        indices_.size() * sizeof(uint16_t),
        vk::BufferUsageFlagBits::eIndexBuffer, false, device);
    deviceBuffer_->Upload(indices_.data(), device);
  }

  virtual void UpdateUniformBuffer(
      std::unordered_map<vk::ShaderStageFlagBits,
                         std::shared_ptr<UniformBuffer>>& buffers,
      uint32_t const bufferId, DeviceApi const& device) {
    for (auto& data : uniformData_) {
      if (buffers.contains(data.first)) {
        data.second->UpdateBuffer(buffers[data.first]);
        buffers[data.first]->Upload(bufferId, device);
      }
    }
  }

  virtual bool IsUploaded() const override {
    return vertexBuffer_.IsUploaded() && deviceBuffer_ != nullptr;
  }

  virtual void Bind(
      vk::UniqueCommandBuffer const& cmdBuffer,
      vk::UniquePipelineLayout const& pipelineLayout) const override {
    vertexBuffer_.Bind(cmdBuffer, pipelineLayout);
    deviceBuffer_->BindIndex(cmdBuffer);
  }

  virtual void Record(
      vk::UniqueCommandBuffer const& cmdBuffer,
      vk::UniquePipelineLayout const& pipelineLayout) const override {
    Bind(cmdBuffer, pipelineLayout);
    cmdBuffer->drawIndexed(indices_.size(), 1, 0, 0, 0);
  }

 private:
  VertexBuffer<T> vertexBuffer_;
  std::vector<uint16_t> indices_;
  std::unique_ptr<DeviceBuffer> deviceBuffer_;
  std::unordered_map<vk::ShaderStageFlagBits, std::shared_ptr<UniformData>>
      uniformData_;
};