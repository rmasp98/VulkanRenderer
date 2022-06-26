#pragma once

#include "device_api.hpp"
#include "device_buffer.hpp"
#include "queues.hpp"

struct MVP {
  float data[16];
  auto operator<=>(MVP const& rhs) const = default;
};

class UniformBuffer {
 public:
  virtual ~UniformBuffer() = default;

  virtual void AllocateBuffer(DeviceApi&) = 0;
  virtual void DeallocateBuffer() = 0;

  template <class T>
  void Update(T const& data) {
    assert(typeid(T) == GetTypeId());
    UpdateInternal(static_cast<void const*>(&data));
  }

  virtual void Upload(ImageIndex const, Queues const&, DeviceApi&) = 0;

  virtual void Bind(ImageIndex const, vk::UniqueCommandBuffer const&,
                    vk::UniquePipelineLayout const&) const = 0;

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
    if (data_ != data) {
      data_ = data;
      for (auto& buffer : deviceBuffers_) {
        buffer.SetOutdated();
      }
    }
  }

  virtual void Upload(ImageIndex const imageIndex, Queues const& queues,
                      DeviceApi& device) override {
    assert(imageIndex < deviceBuffers_.size());
    if (deviceBuffers_[imageIndex].IsOutdated()) {
      deviceBuffers_[imageIndex].Upload(&data_, queues, device);
    }
  }

  virtual void Bind(ImageIndex const imageIndex,
                    vk::UniqueCommandBuffer const& cmdBuffer,
                    vk::UniquePipelineLayout const& layout) const override {
    assert(imageIndex < descriptorSets_.size());
    cmdBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                  layout.get(), 0,
                                  {descriptorSets_[imageIndex].get()}, {});
  }

  virtual vk::DescriptorSetLayout GetLayout() { return layout_.get(); };

 protected:
  // The caller of this function is responsible for checking data is of valid
  // type T
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