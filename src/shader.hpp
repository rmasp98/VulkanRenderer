#pragma once

#include <fstream>
#include <vector>

#include "device_api.hpp"
#include "vulkan/vulkan.hpp"

struct ShaderDetails {
  std::vector<char> FileContents;
  std::vector<vk::DescriptorSetLayoutBinding> Bindings;
};

class Shader {
 public:
  Shader(vk::ShaderStageFlagBits type, ShaderDetails details,
         DeviceApi const& device)
      : type_(type),
        details_(details),
        module_(device.CreateShaderModule(details_.FileContents)),
        layout_(device.CreateDescriptorSetLayout({{}, details_.Bindings})) {}

  vk::PipelineShaderStageCreateInfo GetStage() const {
    return {{}, type_, module_.get(), "main"};
  }

  vk::DescriptorSetLayout GetLayout() const { return layout_.get(); }

  vk::ShaderStageFlagBits GetType() { return type_; }

 private:
  vk::ShaderStageFlagBits type_;
  ShaderDetails details_;
  vk::UniqueShaderModule module_;
  vk::UniqueDescriptorSetLayout layout_;
};

// TODO: change this to a char const*?
inline std::vector<char> LoadShader(char const* filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  return buffer;
}
