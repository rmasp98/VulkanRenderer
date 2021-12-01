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
  virtual void AllocateBuffers(
      std::shared_ptr<Device> const&,
      std::unordered_map<vk::ShaderStageFlagBits,
                         vk::DescriptorSetLayout> const&,
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
  UniformBufferMVP(MVP const& data, vk::ShaderStageFlagBits const stage)
      : data_(data), stage_(stage) {}
  ~UniformBufferMVP() = default;

  void Update(MVP const& data) {
    data_ = data;
    isUploaded_.assign(isUploaded_.size(), false);
  }

  // TODO: figure out how to map T to layout
  virtual void AllocateBuffers(
      std::shared_ptr<Device> const& device,
      std::unordered_map<vk::ShaderStageFlagBits,
                         vk::DescriptorSetLayout> const& descriptorSetLayouts,
      uint32_t const numBuffers) final {
    if (descriptorSetLayouts.count(stage_) > 0) {
      buffers_.clear();
      for (uint32_t i = 0; i < numBuffers; ++i) {
        buffers_.emplace_back(device->UploadBuffer(
            &data_, sizeof(MVP), vk::BufferUsageFlagBits::eUniformBuffer,
            false));
      }
      isUploaded_.resize(numBuffers, true);
      descriptorSets_ =
          device->AllocateDescriptorSet(descriptorSetLayouts.at(stage_));
    }
  }

  virtual void Upload(std::shared_ptr<Device> const& device,
                      uint32_t const bufferId) final {
    if (!IsUploaded(bufferId)) {
      device->UploadBuffer(buffers_[bufferId], &data_);
      isUploaded_[bufferId] = true;
    }
  }

  virtual void Remove() final {
    for (auto& buffer : buffers_) {
      buffer.reset();
    }
  }

  virtual bool IsUploaded(uint32_t const bufferId) const final {
    return isUploaded_[bufferId];
  }

  virtual void Bind(vk::UniqueCommandBuffer const& cmdBuffer,
                    vk::UniquePipelineLayout const& layout,
                    uint32_t const bufferId) const final {
    cmdBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                  layout.get(), 0,
                                  {descriptorSets_[bufferId].get()}, {});
  }

 private:
  MVP data_;
  vk::ShaderStageFlagBits stage_;
  std::vector<std::unique_ptr<BufferRef>> buffers_;
  std::vector<bool> isUploaded_;
  std::vector<vk::UniqueDescriptorSet> descriptorSets_;
};

;
