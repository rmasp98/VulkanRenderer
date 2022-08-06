#ifndef VULKAN_RENDERER_IMAGE_BUFFER_HPP
#define VULKAN_RENDERER_IMAGE_BUFFER_HPP

#include "descriptor_sets.hpp"
#include "device_api.hpp"
#include "device_buffer.hpp"
#include "image_properties.hpp"
#include "queues.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

class ImageBuffer {
 public:
  ImageBuffer(ImageProperties const& properties, Queues const& queues,
              DeviceApi& device)
      : properties_(ValidateImageProperties(properties, device)),
        image_(device.CreateImage(
            {
                {},
                properties_.Type,
                properties_.Format,
                properties_.Extent,
                properties_.MipLevels,
                properties_.ArrayLayers,
                properties_.SampleCount,
                properties_.Tiling,
                properties_.Usage,
                vk::SharingMode::eExclusive,
            },
            queues.GetQueueFamilies().UniqueIndices())),
        allocation_(device.AllocateMemory(
            image_.get(), vk::MemoryPropertyFlagBits::eDeviceLocal)),
        imageView_(device.CreateImageView(
            image_.get(), vk::ImageViewType::e2D, properties_.Format, {},
            {properties_.Aspect, 0, properties_.MipLevels, 0, 1})) {}

  void Upload(std::vector<unsigned char> data, Queues const& queues,
              DeviceApi& device) {
    TransferDeviceBuffer transferBuffer{properties_.Extent.width *
                                            properties_.Extent.height *
                                            properties_.Extent.depth * 4,
                                        device};
    transferBuffer.Upload(data.data(), queues, device);
    transferBuffer.CopyToImage(image_.get(), properties_, queues, device);

    isOutdated_ = false;
  }

  void SetOutdated() { isOutdated_ = true; }
  bool IsOutdated() const { return isOutdated_; }

  vk::ImageView const& GetImageView() const { return imageView_.get(); }

 protected:
  vk::Image const& GetImage() const { return image_.get(); }
  ImageProperties const& GetProperties() const { return properties_; }

 private:
  ImageProperties properties_;
  vk::UniqueImage image_;
  Allocation allocation_;
  vk::UniqueImageView imageView_;
  bool isOutdated_ = true;
};

class SamplerImageBuffer : public ImageBuffer {
 public:
  SamplerImageBuffer(ImageProperties const& properties, Queues const& queues,
                     DeviceApi& device)
      : ImageBuffer(properties, queues, device),
        sampler_(device.CreateSampler(
            {{},
             GetProperties().MagFilter,
             GetProperties().MinFilter,
             GetProperties().MipMapMode,
             GetProperties().UAddressMode,
             GetProperties().VAddressMode,
             GetProperties().WAddressMode,
             GetProperties().MipLodBias,
             GetProperties().MaxAnisotropy >= 0.0f,
             GetProperties().MaxAnisotropy,
             GetProperties().CompareOp != vk::CompareOp::eNever,
             GetProperties().CompareOp,
             GetProperties().MinLod,
             GetProperties().MaxLod,
             GetProperties().BorderColour,
             GetProperties().UnnormaliseCoordinates})),
        imageInfo_(sampler_.get(), GetImageView(), GetProperties().Layout) {}

  void AddDescriptorSetUpdate(uint32_t const set, ImageIndex const imageIndex,
                              vk::WriteDescriptorSet& writeSet,
                              DescriptorSets& descriptorSets) {
    writeSet.setImageInfo(imageInfo_);
    descriptorSets.AddUpdate(set, imageIndex, writeSet);
  }

  void AddDescriptorSetUpdate(uint32_t const set,
                              vk::WriteDescriptorSet& writeSet,
                              DescriptorSets& descriptorSets) {
    writeSet.setImageInfo(imageInfo_);
    descriptorSets.AddUpdate(set, writeSet);
  }

 private:
  vk::UniqueSampler sampler_;
  vk::DescriptorImageInfo imageInfo_;
};

class DepthBuffer : public ImageBuffer {
 public:
  DepthBuffer(vk::Extent2D const& windowExtent,
              vk::SampleCountFlagBits const multiSampleCount,
              Queues const& queues, DeviceApi& device)
      : ImageBuffer({.Extent = {windowExtent.width, windowExtent.height, 1},
                     .Usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                     .Format = device.GetDepthBufferFormat(),
                     .Aspect = vk::ImageAspectFlagBits::eDepth,
                     .SampleCount = multiSampleCount},
                    queues, device) {
    auto cmdBuffer = device.AllocateCommandBuffer();
    cmdBuffer->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    auto properties = GetProperties();
    TransitionImageLayout(GetImage(), 0, properties.MipLevels,
                          properties.Format, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthStencilAttachmentOptimal,
                          cmdBuffer.get());
    cmdBuffer->end();

    queues.SubmitToGraphics(cmdBuffer.get());
    queues.GraphicsWaitIdle();
  }

  vk::Format GetFormat() { return GetProperties().Format; }
};

}  // namespace vulkan_renderer

#endif