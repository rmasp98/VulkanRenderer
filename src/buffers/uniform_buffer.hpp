#ifndef VULKAN_RENDERER_UNIFORM_BUFFER_HPP
#define VULKAN_RENDERER_UNIFORM_BUFFER_HPP

#include "descriptor_sets.hpp"
#include "device_api.hpp"
#include "queues.hpp"

namespace vulkan_renderer {

class Uniform {
 public:
  virtual ~Uniform() = default;

  virtual void Allocate(Queues const&, DeviceApi&) = 0;
  virtual void Deallocate() = 0;

  virtual bool IsOutdated(ImageIndex const) const = 0;
  virtual void Upload(ImageIndex const, Queues const&, DeviceApi&) = 0;

  virtual void AddDescriptorSetUpdate(DescriptorSets&) const = 0;
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
  void Allocate(Queues const& queues, DeviceApi& device) override {
    for (uint32_t i = 0; i < device.GetNumSwapchainImages(); ++i) {
      deviceBuffers_.emplace_back(
          sizeof(T), vk::BufferUsageFlagBits::eUniformBuffer, device);
    }
  }

  void Deallocate() override { deviceBuffers_.clear(); }

  bool IsOutdated(ImageIndex const imageIndex) const override {
    assert(imageIndex < deviceBuffers_.size());
    return deviceBuffers_[imageIndex].IsOutdated();
  }

  void Upload(ImageIndex const imageIndex, Queues const& queues,
              DeviceApi& device) override {
    if (deviceBuffers_.size() == 0) {
      Allocate(queues, device);
    }

    assert(imageIndex < deviceBuffers_.size());
    deviceBuffers_[imageIndex].Upload(&data_, queues, device);
  }

  void AddDescriptorSetUpdate(DescriptorSets& descriptorSets) const override {
    for (uint32_t i = 0; i < deviceBuffers_.size(); ++i) {
      vk::WriteDescriptorSet writeSet{
          {},      binding_, 0,      1, vk::DescriptorType::eUniformBuffer,
          nullptr, nullptr,  nullptr};
      deviceBuffers_[i].AddDescriptorSetUpdate(set_, i, writeSet,
                                               descriptorSets);
    }
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
  void Allocate(Queues const& queues, DeviceApi& device) override {
    imageBuffer_ =
        std::make_unique<SamplerImageBuffer>(properties_, queues, device);
  }

  void Deallocate() override { imageBuffer_.reset(); }

  bool IsOutdated(ImageIndex const) const override {
    assert(imageBuffer_);
    return imageBuffer_->IsOutdated();
  }

  void Upload(ImageIndex const, Queues const& queues,
              DeviceApi& device) override {
    if (!imageBuffer_) {
      Allocate(queues, device);
    }
    imageBuffer_->Upload(data_, queues, device);
  }

  void AddDescriptorSetUpdate(DescriptorSets& descriptorSets) const override {
    assert(imageBuffer_);
    vk::WriteDescriptorSet writeSet{
        {},      binding_, 0,      1, vk::DescriptorType::eCombinedImageSampler,
        nullptr, nullptr,  nullptr};
    imageBuffer_->AddDescriptorSetUpdate(set_, writeSet, descriptorSets);
  }

 private:
  std::vector<unsigned char> data_;
  ImageProperties properties_;
  uint32_t binding_;
  uint32_t set_;
  std::unique_ptr<SamplerImageBuffer> imageBuffer_;
};

class PushConstant {
 public:
  virtual ~PushConstant() = default;
  virtual void SubscribeUpdates(std::function<void()> const& callback) = 0;
  virtual void Upload(vk::PipelineLayout const&,
                      vk::CommandBuffer const&) const = 0;
};

template <class T>
class PushConstantData : public PushConstant {
 public:
  PushConstantData(T const& data, vk::ShaderStageFlags const stages,
                   uint32_t const offset)
      : data_(data), stages_(stages), offset_(offset) {}

  void Update(T const& data) {
    data_ = data;
    if (callback_) {
      callback_();
    }
  }

  void SubscribeUpdates(std::function<void()> const& callback) override {
    callback_ = callback;
  }

  void Upload(vk::PipelineLayout const& layout,
              vk::CommandBuffer const& cmdBuffer) const override {
    cmdBuffer.pushConstants<T>(layout, stages_, offset_, {data_});
  }

 private:
  T data_;
  vk::ShaderStageFlags stages_;
  uint32_t offset_;
  std::function<void()> callback_;
};

}  // namespace vulkan_renderer

#endif