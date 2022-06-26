#pragma once

#include <fstream>
#include <vector>

#include "buffers/uniform_buffer.hpp"
#include "device_api.hpp"
#include "spirv_reflect.h"
#include "vulkan/vulkan.hpp"

struct ShaderDetails {
  std::vector<char> FileContents;
  std::vector<vk::DescriptorSetLayoutBinding> Bindings;
};

std::shared_ptr<UniformBuffer> GetUniformBufferFromShader(
    vk::ShaderStageFlagBits const type, std::vector<char> const& fileContents,
    DeviceApi& device);

class Shader {
 public:
  Shader(vk::ShaderStageFlagBits const type, ShaderDetails const& details,
         DeviceApi& device)
      : type_(type),
        module_(device.CreateShaderModule(details.FileContents)),
        uniformBuffer_(std::move(
            GetUniformBufferFromShader(type, details.FileContents, device))) {}

  vk::PipelineShaderStageCreateInfo GetStage() const {
    return {{}, type_, module_.get(), "main"};
  }

  vk::DescriptorSetLayout GetLayout() const {
    if (uniformBuffer_) {
      return uniformBuffer_->GetLayout();
    }
    return {};
  }

  std::shared_ptr<UniformBuffer>& GetUniformBuffer() { return uniformBuffer_; }

  vk::ShaderStageFlagBits GetType() { return type_; }

 private:
  vk::ShaderStageFlagBits type_;
  vk::UniqueShaderModule module_;
  // TODO: This needs to be an array for different sets
  std::shared_ptr<UniformBuffer> uniformBuffer_;
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

inline std::shared_ptr<UniformBuffer> GetUniformBufferFromShader(
    vk::ShaderStageFlagBits const type, std::vector<char> const& fileContents,
    DeviceApi& device) {
  // Generate reflection data for a shader
  SpvReflectShaderModule module;
  SpvReflectResult result = spvReflectCreateShaderModule(
      fileContents.size(), fileContents.data(), &module);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  uint32_t numSets = 0;
  result = spvReflectEnumerateDescriptorSets(&module, &numSets, nullptr);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  SpvReflectDescriptorSet** descriptorSets = (SpvReflectDescriptorSet**)malloc(
      numSets * sizeof(SpvReflectDescriptorSet*));
  result = spvReflectEnumerateDescriptorSets(&module, &numSets, descriptorSets);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<vk::DescriptorSetLayoutBinding> bindings;
  for (uint32_t i = 0; i < numSets; ++i) {
    for (uint32_t j = 0; j < descriptorSets[i]->binding_count; j++) {
      auto& binding = descriptorSets[i]->bindings[j];
      bindings.push_back(
          {binding->binding,
           static_cast<vk::DescriptorType>(binding->descriptor_type),
           binding->count, type, nullptr});
    }
  }

  if (!bindings.empty()) {
    auto layout = device.CreateDescriptorSetLayout({{}, bindings});

    // TODO: create a function that finds the correct template from reflection
    auto uniformBuffer =
        std::make_shared<UniformBufferImpl<MVP>>(std::move(layout));
    uniformBuffer->AllocateBuffer(device);
    return uniformBuffer;
  }
  return {};
}
