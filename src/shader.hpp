#pragma once

#include <fstream>
#include <vector>

// #include "buffers/uniform_buffer.hpp"
#include "device_api.hpp"
#include "spirv_reflect.h"
#include "vulkan/vulkan.hpp"

using SetBindingsMap =
    std::unordered_map<uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>;

struct ShaderDetails {
  std::vector<char> FileContents;
};

inline SetBindingsMap GetBindingsFromShader(
    vk::ShaderStageFlagBits const type, std::vector<char> const& fileContents);

class Shader {
 public:
  Shader(vk::ShaderStageFlagBits const type,
         std::vector<char> const& fileContents, DeviceApi& device)
      : type_(type),
        module_(device.CreateShaderModule(fileContents)),
        setBindings_(GetBindingsFromShader(type, fileContents)) {}

  vk::PipelineShaderStageCreateInfo GetStage() const {
    return {{}, type_, module_.get(), "main"};
  }

  SetBindingsMap const& GetBindings() const { return setBindings_; }

  vk::ShaderStageFlagBits GetType() const { return type_; }

 private:
  vk::ShaderStageFlagBits type_;
  vk::UniqueShaderModule module_;
  std::unordered_map<uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>
      setBindings_;
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

inline SetBindingsMap GetBindingsFromShader(
    vk::ShaderStageFlagBits const type, std::vector<char> const& fileContents) {
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

  SetBindingsMap setBindings;
  for (uint32_t i = 0; i < numSets; ++i) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    auto set = descriptorSets[i];
    for (uint32_t j = 0; j < descriptorSets[i]->binding_count; j++) {
      auto& binding = set->bindings[j];
      bindings.push_back(
          {binding->binding,
           static_cast<vk::DescriptorType>(binding->descriptor_type),
           binding->count, type, nullptr});
    }
    if (bindings.size()) {
      setBindings.insert({set->set, bindings});
    }
  }

  return setBindings;
}
