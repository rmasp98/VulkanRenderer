#include "device_buffer.hpp"

void DeviceBuffer::Upload(void const* data, Queues const&, DeviceApi& device) {
  auto memoryLocation = device.MapMemory(allocation_);
  memcpy(memoryLocation, data, size_);
  device.UnmapMemory(allocation_);
  isOutdated_ = false;
}

void DeviceBuffer::AddDescriptorSetUpdate(
    uint32_t const set, ImageIndex const imageIndex,
    vk::WriteDescriptorSet& writeSet,
    std::unique_ptr<DescriptorSets>& descriptorSets) const {
  writeSet.setBufferInfo(bufferInfo_);
  descriptorSets->AddUpdate(set, imageIndex, writeSet);
}

void TransferDeviceBuffer::CopyToBuffer(vk::UniqueBuffer const& targetBuffer,
                                        Queues const& queues,
                                        DeviceApi& device) {
  auto cmdBuffer =
      device.AllocateCommandBuffers(vk::CommandBufferLevel::ePrimary, 1);
  cmdBuffer[0]->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  vk::BufferCopy copyRegion{0, 0, GetSize()};
  cmdBuffer[0]->copyBuffer(GetBuffer().get(), targetBuffer.get(), copyRegion);
  cmdBuffer[0]->end();

  queues.SubmitToGraphics(cmdBuffer[0]);
  // TODO: remove if we can deallocate in response to completion in callback
  queues.GraphicsWaitIdle();
}

void TransferDeviceBuffer::CopyToImage(vk::UniqueImage const& targetImage,
                                       ImageProperties& properties,
                                       Queues const& queues,
                                       DeviceApi& device) {
  auto cmdBuffer =
      device.AllocateCommandBuffers(vk::CommandBufferLevel::ePrimary, 1);
  cmdBuffer[0]->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  TransitionImageLayout(targetImage, 0, properties.MipLevels, properties.Format,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal, cmdBuffer[0]);

  vk::BufferImageCopy region{
      0, 0, 0, {properties.Aspect, 0, 0, 1}, {0, 0, 0}, properties.Extent};
  cmdBuffer[0]->copyBufferToImage(GetBuffer().get(), targetImage.get(),
                                  vk::ImageLayout::eTransferDstOptimal, 1,
                                  &region);

  GenerateMipMaps(targetImage, properties, cmdBuffer[0]);

  cmdBuffer[0]->end();

  queues.SubmitToGraphics(cmdBuffer[0]);
  // TODO: remove if we can deallocate in response to completion in callback
  queues.GraphicsWaitIdle();
}

void OptimisedDeviceBuffer::Upload(void const* data, Queues const& queues,
                                   DeviceApi& device) {
  TransferDeviceBuffer transferBuffer{GetSize(), device};
  transferBuffer.Upload(data, queues, device);
  transferBuffer.CopyToBuffer(GetBuffer(), queues, device);

  SetOutdated();
}

vk::AccessFlags GetSourceAccessMask(vk::ImageLayout const sourceLayout) {
  switch (sourceLayout) {
    case vk::ImageLayout::eTransferDstOptimal:
      return vk::AccessFlagBits::eTransferWrite;
    case vk::ImageLayout::eTransferSrcOptimal:
      return vk::AccessFlagBits::eTransferRead;
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

vk::PipelineStageFlags GetSourceStage(vk::ImageLayout const sourceLayout) {
  switch (sourceLayout) {
    case vk::ImageLayout::eGeneral:
    case vk::ImageLayout::ePreinitialized:
      return vk::PipelineStageFlagBits::eHost;
    case vk::ImageLayout::eTransferSrcOptimal:
    case vk::ImageLayout::eTransferDstOptimal:
      return vk::PipelineStageFlagBits::eTransfer;
    case vk::ImageLayout::eUndefined:
      return vk::PipelineStageFlagBits::eTopOfPipe;
    default:
      assert(false);
  }
  return {};
}

vk::AccessFlags GetDestinationAccessMask(
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

vk::PipelineStageFlags GetDestinationStage(
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

void TransitionImageLayout(vk::UniqueImage const& image,
                           uint32_t const mipLevel, uint32_t const numMipLevels,
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

  vk::ImageSubresourceRange imageSubresourceRange(aspectMask, mipLevel,
                                                  numMipLevels, 0, 1);
  vk::ImageMemoryBarrier imageMemoryBarrier(
      GetSourceAccessMask(sourceLayout),
      GetDestinationAccessMask(destinationLayout), sourceLayout,
      destinationLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
      image.get(), imageSubresourceRange);
  cmdBuffer->pipelineBarrier(GetSourceStage(sourceLayout),
                             GetDestinationStage(destinationLayout), {},
                             nullptr, nullptr, imageMemoryBarrier);
}

void GenerateMipMaps(vk::UniqueImage const& image,
                     ImageProperties const& properties,
                     vk::UniqueCommandBuffer const& cmdBuffer) {
  vk::Offset3D baseOffset{static_cast<int>(properties.Extent.width),
                          static_cast<int>(properties.Extent.height), 1};
  vk::Offset3D mipOffset{baseOffset.x / 2, baseOffset.y / 2, 1};
  for (uint32_t i = 1;
       i < properties.MipLevels && baseOffset.x > 1 && baseOffset.y > 1; ++i) {
    TransitionImageLayout(image, i - 1, 1, properties.Format,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eTransferSrcOptimal, cmdBuffer);

    vk::ImageBlit blit{{properties.Aspect, i - 1, 0, 1},
                       {vk::Offset3D{0, 0, 0}, baseOffset},
                       {properties.Aspect, i, 0, 1},
                       {vk::Offset3D{0, 0, 0}, mipOffset}};
    cmdBuffer->blitImage(image.get(), vk::ImageLayout::eTransferSrcOptimal,
                         image.get(), vk::ImageLayout::eTransferDstOptimal,
                         blit, vk::Filter::eLinear);

    TransitionImageLayout(image, i - 1, 1, properties.Format,
                          vk::ImageLayout::eTransferSrcOptimal,
                          properties.Layout, cmdBuffer);

    baseOffset = mipOffset;
    mipOffset = vk::Offset3D{baseOffset.x / 2, baseOffset.y / 2, 1};
  }

  TransitionImageLayout(image, properties.MipLevels - 1, 1, properties.Format,
                        vk::ImageLayout::eTransferDstOptimal, properties.Layout,
                        cmdBuffer);
}