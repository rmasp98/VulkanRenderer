#pragma once

#include "descriptor_sets.hpp"
#include "device_api.hpp"
#include "queues.hpp"

class Uniform {
 public:
  virtual ~Uniform() = default;

  virtual void Allocate(Queues const&, DeviceApi&,
                        bool const force = false) = 0;
  virtual void Deallocate() = 0;

  virtual void Upload(ImageIndex const, Queues const&, DeviceApi&) = 0;

  virtual void AddDescriptorSetUpdate(
      ImageIndex const, std::unique_ptr<DescriptorSets>&) const = 0;
};

struct MVP {
  float data[16];
  auto operator<=>(MVP const& rhs) const = default;
};

template <typename T>
class UniformBuffer : public Uniform {
 public:
  UniformBuffer(T const& data, uint32_t const binding, uint32_t const set = 0)
      : data_(data), binding_(binding), set_(set) {}

  void Update(T const& data) {
    if (data_ != data) {
      data_ = data;
      for (auto& buffer : deviceBuffers_) {
        buffer.SetOutdated();
      }
    }
  }

 protected:
  void Allocate(Queues const& queues, DeviceApi& device,
                bool const force) override {
    if (force || deviceBuffers_.size() == 0) {
      for (uint32_t i = 0; i < device.GetNumSwapchainImages(); ++i) {
        deviceBuffers_.push_back({sizeof(T),
                                  vk::BufferUsageFlagBits::eUniformBuffer,
                                  false, device});
      }
    }
  }

  void Deallocate() override { deviceBuffers_.clear(); }

  void Upload(ImageIndex const imageIndex, Queues const& queues,
              DeviceApi& device) override {
    assert(imageIndex < deviceBuffers_.size());
    if (deviceBuffers_[imageIndex].IsOutdated()) {
      deviceBuffers_[imageIndex].Upload(&data_, queues, device);
    }
  }

  void AddDescriptorSetUpdate(
      ImageIndex const imageIndex,
      std::unique_ptr<DescriptorSets>& descriptorSets) const override {
    assert(imageIndex < deviceBuffers_.size());
    vk::WriteDescriptorSet writeSet{
        {},      binding_, 0,      1, vk::DescriptorType::eUniformBuffer,
        nullptr, nullptr,  nullptr};
    deviceBuffers_[imageIndex].AddDescriptorSetUpdate(set_, imageIndex,
                                                      writeSet, descriptorSets);
  }

 private:
  T data_;
  uint32_t binding_;
  uint32_t set_;
  std::vector<DeviceBuffer> deviceBuffers_;
};

class UniformImage : public Uniform {
 public:
  UniformImage(std::vector<unsigned char> const& data,
               ImageProperties const& properties, uint32_t const binding,
               uint32_t const set = 0)
      : data_(data), properties_(properties), binding_(binding), set_(set) {
    assert(data_.size() == properties_.Extent.width *
                               properties_.Extent.height *
                               properties_.Extent.depth * 4);
  }

 protected:
  void Allocate(Queues const& queues, DeviceApi& device,
                bool const force) override {
    if (force || imageBuffer_ == nullptr) {
      imageBuffer_ = std::make_unique<ImageBuffer>(
          properties_, queues.GetQueueFamilies().UniqueIndices(), device);
    }
  }

  void Deallocate() override { imageBuffer_.reset(); }

  void Upload(ImageIndex const, Queues const& queues,
              DeviceApi& device) override {
    assert(imageBuffer_);
    if (imageBuffer_->IsOutdated()) {
      imageBuffer_->Upload(data_, properties_.Extent, queues, device);
    }
  }

  void AddDescriptorSetUpdate(
      ImageIndex const imageIndex,
      std::unique_ptr<DescriptorSets>& descriptorSets) const override {
    assert(imageBuffer_);
    vk::WriteDescriptorSet writeSet{
        {},      binding_, 0,      1, vk::DescriptorType::eCombinedImageSampler,
        nullptr, nullptr,  nullptr};
    imageBuffer_->AddDescriptorSetUpdate(set_, imageIndex, writeSet,
                                         descriptorSets);
  }

 private:
  std::vector<unsigned char> data_;
  ImageProperties properties_;
  uint32_t binding_;
  uint32_t set_;
  std::unique_ptr<ImageBuffer> imageBuffer_;
};