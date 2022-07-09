#pragma once

#include "descriptor_sets.hpp"
#include "device_api.hpp"
#include "queues.hpp"
#include "vulkan/vulkan.hpp"

vk::AccessFlags GetSourceAccessMask(vk::ImageLayout const sourceLayout);
vk::PipelineStageFlags GetSourceStage(vk::ImageLayout const sourceLayout);
vk::AccessFlags GetDestinationAccessMask(
    vk::ImageLayout const destinationLayout);
vk::PipelineStageFlags GetDestinationStage(
    vk::ImageLayout const destinationLayout);

struct ImageProperties {
  vk::Extent3D Extent;
  vk::ImageType Type = vk::ImageType::e2D;
  vk::Format Format = vk::Format::eR8G8B8A8Srgb;
  vk::ImageLayout Layout = vk::ImageLayout::eShaderReadOnlyOptimal;
  uint32_t MipLevels = 1;
  uint32_t ArrayLayers = 1;
  vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1;
  vk::ImageTiling Tiling = vk::ImageTiling::eOptimal;
  vk::SamplerCreateInfo test;
  vk::Filter MagFilter = vk::Filter::eLinear;
  vk::Filter MinFilter = vk::Filter::eLinear;
  vk::SamplerAddressMode UAddressMode = vk::SamplerAddressMode::eRepeat;
  vk::SamplerAddressMode VAddressMode = vk::SamplerAddressMode::eRepeat;
  vk::SamplerAddressMode WAddressMode = vk::SamplerAddressMode::eRepeat;
  float MaxAnisotropy = -1.0f;  // negative disables
  vk::BorderColor BorderColour = vk::BorderColor::eIntOpaqueBlack;
  bool UnnormaliseCoordinates = false;
  vk::CompareOp CompareOp = vk::CompareOp::eAlways;  // eNever is disabled
  float MipLodBias = -1.0f;                          // negative disables?
  float MinLod = -1.0f;                              // negative disables?
  float MaxLod = -1.0f;                              // negative disables?

  auto operator<=>(ImageProperties const& rhs) const = default;
};

class ImageBuffer {
 public:
  ImageBuffer(ImageProperties const& properties,
              std::vector<uint32_t> const& queueFamilyIndices,
              DeviceApi& device)
      : size_{properties.Extent.width * properties.Extent.height *
              properties.Extent.depth * 4},
        image_(device.CreateImage({{},
                                   properties.Type,
                                   properties.Format,
                                   properties.Extent,
                                   properties.MipLevels,
                                   properties.ArrayLayers,
                                   properties.SampleCount,
                                   properties.Tiling,
                                   vk::ImageUsageFlagBits::eTransferDst |
                                       vk::ImageUsageFlagBits::eSampled,
                                   vk::SharingMode::eExclusive,
                                   queueFamilyIndices})),
        id_(device.AllocateMemory(image_,
                                  vk::MemoryPropertyFlagBits::eDeviceLocal)),
        imageView_(device.CreateImageView(
            image_.get(), vk::ImageViewType::e2D, properties.Format, {},
            {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1})),
        sampler_(
            device.CreateSampler({{},
                                  properties.MagFilter,
                                  properties.MinFilter,
                                  vk::SamplerMipmapMode::eNearest,
                                  properties.UAddressMode,
                                  properties.VAddressMode,
                                  properties.WAddressMode,
                                  properties.MipLodBias,
                                  properties.MaxAnisotropy >= 0.0f,
                                  properties.MaxAnisotropy,
                                  properties.CompareOp != vk::CompareOp::eNever,
                                  properties.CompareOp,
                                  properties.MinLod,
                                  properties.MaxLod,
                                  properties.BorderColour,
                                  properties.UnnormaliseCoordinates})),

        imageInfo_(sampler_.get(), imageView_.get(), properties.Layout) {}

  void Upload(std::vector<unsigned char> data, vk::Extent3D const extent,
              Queues const& queues, DeviceApi& device) {
    auto transferBuffer =
        device.CreateBuffer(size_, vk::BufferUsageFlagBits::eTransferSrc);
    auto transferId = device.AllocateMemory(
        transferBuffer, vk::MemoryPropertyFlagBits::eHostVisible |
                            vk::MemoryPropertyFlagBits::eHostCoherent);
    auto memoryLocation = device.MapMemory(transferId);
    memcpy(memoryLocation, data.data(), size_);
    device.UnmapMemory(transferId);

    auto cmdBuffer =
        device.AllocateCommandBuffers(vk::CommandBufferLevel::ePrimary, 1);
    cmdBuffer[0]->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    // TODO: could remove this using inital layer in create
    transitionImageLayout(image_, vk::Format::eR8G8B8A8Srgb,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal, cmdBuffer[0]);

    vk::BufferImageCopy region{
        0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0, 0, 0}, extent};
    cmdBuffer[0]->copyBufferToImage(transferBuffer.get(), image_.get(),
                                    vk::ImageLayout::eTransferDstOptimal, 1,
                                    &region);

    transitionImageLayout(
        image_, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal, cmdBuffer[0]);

    cmdBuffer[0]->end();

    queues.SubmitToGraphics(cmdBuffer[0]);
    // TODO: remove if we can deallocate in response to completion in callback
    queues.GraphicsWaitIdle();
    device.DeallocateMemory(transferId);
    isOutdated_ = false;
  }

  void AddDescriptorSetUpdate(uint32_t const set, ImageIndex const imageIndex,
                              vk::WriteDescriptorSet& writeSet,
                              std::unique_ptr<DescriptorSets>& descriptorSets) {
    writeSet.setImageInfo(imageInfo_);
    descriptorSets->AddUpdate(set, imageIndex, writeSet);
  }

  void SetOutdated() { isOutdated_ = true; }
  bool IsOutdated() { return isOutdated_; }

 private:
  uint32_t size_;
  vk::UniqueImage image_;
  AllocationId id_;
  vk::UniqueImageView imageView_;
  vk::UniqueSampler sampler_;
  bool isOutdated_ = true;
  vk::DescriptorImageInfo imageInfo_;

  void transitionImageLayout(vk::UniqueImage const& image,
                             vk::Format const format,
                             vk::ImageLayout const sourceLayout,
                             vk::ImageLayout const destinationLayout,
                             vk::UniqueCommandBuffer const& cmdBuffer) {
    vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
    if (destinationLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
      aspectMask = vk::ImageAspectFlagBits::eDepth;
      if (format == vk::Format::eD32SfloatS8Uint ||
          format == vk::Format::eD24UnormS8Uint) {
        aspectMask |= vk::ImageAspectFlagBits::eStencil;
      }
    }

    vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, 1, 0, 1);
    vk::ImageMemoryBarrier imageMemoryBarrier(
        GetSourceAccessMask(sourceLayout),
        GetDestinationAccessMask(destinationLayout), sourceLayout,
        destinationLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        image.get(), imageSubresourceRange);
    cmdBuffer->pipelineBarrier(GetSourceStage(sourceLayout),
                               GetDestinationStage(destinationLayout), {},
                               nullptr, nullptr, imageMemoryBarrier);
  }
};

inline vk::AccessFlags GetSourceAccessMask(vk::ImageLayout const sourceLayout) {
  switch (sourceLayout) {
    case vk::ImageLayout::eTransferDstOptimal:
      return vk::AccessFlagBits::eTransferWrite;
    case vk::ImageLayout::ePreinitialized:
      return vk::AccessFlagBits::eHostWrite;
    case vk::ImageLayout::eGeneral:  // sourceAccessMask is empty
    case vk::ImageLayout::eUndefined:
      break;
    default:
      assert(false);
  }
  return {};
}

inline vk::PipelineStageFlags GetSourceStage(
    vk::ImageLayout const sourceLayout) {
  switch (sourceLayout) {
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::ePreinitialized:
      return vk::PipelineStageFlagBits::eHost;
    case vk::ImageLayout::eTransferDstOptimal:
      return vk::PipelineStageFlagBits::eTransfer;
    case vk::ImageLayout::eUndefined:
      return vk::PipelineStageFlagBits::eTopOfPipe;
    default:
      assert(false);
  }
  return {};
}

inline vk::AccessFlags GetDestinationAccessMask(
    vk::ImageLayout const destinationLayout) {
  switch (destinationLayout) {
    case vk::ImageLayout::eColorAttachmentOptimal:
      return vk::AccessFlagBits::eColorAttachmentWrite;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
      return vk::AccessFlagBits::eDepthStencilAttachmentRead |
             vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
      return vk::AccessFlagBits::eShaderRead;
    case vk::ImageLayout::eTransferSrcOptimal:
      return vk::AccessFlagBits::eTransferRead;
    case vk::ImageLayout::eTransferDstOptimal:
      return vk::AccessFlagBits::eTransferWrite;
    case vk::ImageLayout::eGeneral:  // empty destinationAccessMask
    case vk::ImageLayout::ePresentSrcKHR:
      break;
    default:
      assert(false);
  }
  return {};
}

inline vk::PipelineStageFlags GetDestinationStage(
    vk::ImageLayout const destinationLayout) {
  switch (destinationLayout) {
    case vk::ImageLayout::eColorAttachmentOptimal:
      return vk::PipelineStageFlagBits::eColorAttachmentOutput;
    case vk::ImageLayout::eDepthStencilAttachmentOptimal:
      return vk::PipelineStageFlagBits::eEarlyFragmentTests;
    case vk::ImageLayout::eGeneral:
      return vk::PipelineStageFlagBits::eHost;
    case vk::ImageLayout::ePresentSrcKHR:
      return vk::PipelineStageFlagBits::eBottomOfPipe;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
      return vk::PipelineStageFlagBits::eFragmentShader;
    case vk::ImageLayout::eTransferDstOptimal:
    case vk::ImageLayout::eTransferSrcOptimal:
      return vk::PipelineStageFlagBits::eTransfer;
    default:
      assert(false);
  }
  return {};
}