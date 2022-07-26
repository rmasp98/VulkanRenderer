#pragma once

#include "device_api.hpp"
#include "vulkan/vulkan.hpp"

struct ImageProperties {
  // Main properties
  vk::Extent3D Extent;
  vk::ImageUsageFlags Usage = vk::ImageUsageFlagBits::eTransferSrc |
                              vk::ImageUsageFlagBits::eTransferDst |
                              vk::ImageUsageFlagBits::eSampled;
  vk::Format Format = vk::Format::eR8G8B8A8Srgb;
  vk::ImageType Type = vk::ImageType::e2D;
  vk::ImageAspectFlagBits Aspect = vk::ImageAspectFlagBits::eColor;
  vk::ImageLayout Layout = vk::ImageLayout::eShaderReadOnlyOptimal;

  // Mip properties?
  uint32_t MipLevels = 1;
  uint32_t ArrayLayers = 1;
  float MipLodBias = 0.0f;
  float MinLod = 0.0f;
  float MaxLod = 0.0f;
  vk::Filter MagFilter = vk::Filter::eLinear;
  vk::Filter MinFilter = vk::Filter::eLinear;
  vk::SamplerMipmapMode MipMapMode = vk::SamplerMipmapMode::eNearest;

  // Sampling properties?
  vk::SampleCountFlagBits SampleCount = vk::SampleCountFlagBits::e1;
  vk::ImageTiling Tiling = vk::ImageTiling::eOptimal;
  vk::SamplerAddressMode UAddressMode = vk::SamplerAddressMode::eRepeat;
  vk::SamplerAddressMode VAddressMode = vk::SamplerAddressMode::eRepeat;
  vk::SamplerAddressMode WAddressMode = vk::SamplerAddressMode::eRepeat;
  vk::BorderColor BorderColour = vk::BorderColor::eIntOpaqueBlack;

  // Other
  float MaxAnisotropy = -1.0f;  // negative disables
  bool UnnormaliseCoordinates = false;
  vk::CompareOp CompareOp = vk::CompareOp::eAlways;  // eNever is disabled

  auto operator<=>(ImageProperties const& rhs) const = default;
};

// TODO: to some more checks to make sure provided properties are valid
inline ImageProperties ValidateImageProperties(
    ImageProperties const& properties, DeviceApi const& device) {
  auto validProperties = properties;
  if (validProperties.MipLevels > 1 &&
      !device.IsLinearFilteringSupported(properties.Format)) {
    validProperties.MipLevels = 1;
    // TODO: log a warning that mipmaps have been disabled for this image
  }
  return validProperties;
}