#ifndef VULKAN_RENDERER_SHADER_HPP
#define VULKAN_RENDERER_SHADER_HPP

#include <fstream>
#include <vector>

// #include "buffers/uniform_buffer.hpp"
#include "device_api.hpp"
#include "spirv_reflect.h"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

// TODO: change this to a char const*?
inline std::vector<char> LoadShader(char const* filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize / sizeof(char));

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  return buffer;
}

using SetBindingsMap =
    std::unordered_map<uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>;

SetBindingsMap GetBindingsFromShader(vk::ShaderStageFlagBits const type,
                                     SpvReflectShaderModule const& module);
std::vector<vk::PushConstantRange> GetPushConstantsFromShader(
    vk::ShaderStageFlagBits const type, SpvReflectShaderModule const& module);

class Shader {
 public:
  Shader(vk::ShaderStageFlagBits const type,
         std::vector<char> const& fileContents, DeviceApi& device)
      : type_(type), module_(device.CreateShaderModule(fileContents)) {
    // Generate reflection data for a shader
    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(
        fileContents.size(), fileContents.data(), &module);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    setBindings_ = GetBindingsFromShader(type, module);
    pushConstants_ = GetPushConstantsFromShader(type, module);
  }

  vk::PipelineShaderStageCreateInfo GetStage() const {
    return {{}, type_, module_.get(), "main"};
  }

  SetBindingsMap const& GetBindings() const { return setBindings_; }
  std::vector<vk::PushConstantRange> const& GetPushConstants() const {
    return pushConstants_;
  }

  vk::ShaderStageFlagBits GetType() const { return type_; }

 private:
  vk::ShaderStageFlagBits type_;
  vk::UniqueShaderModule module_;
  SetBindingsMap setBindings_;
  std::vector<vk::PushConstantRange> pushConstants_;
};

inline SetBindingsMap GetBindingsFromShader(
    vk::ShaderStageFlagBits const type, SpvReflectShaderModule const& module) {
  uint32_t numSets = 0;
  auto result = spvReflectEnumerateDescriptorSets(&module, &numSets, nullptr);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<SpvReflectDescriptorSet*> descriptorSets(numSets);
  result = spvReflectEnumerateDescriptorSets(&module, &numSets,
                                             descriptorSets.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  SetBindingsMap setBindings;
  for (auto const& set : descriptorSets) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    for (uint32_t j = 0; j < set->binding_count; j++) {
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

inline std::vector<vk::PushConstantRange> GetPushConstantsFromShader(
    vk::ShaderStageFlagBits const type, SpvReflectShaderModule const& module) {
  uint32_t numPushConstants = 0;
  auto result = spvReflectEnumeratePushConstantBlocks(
      &module, &numPushConstants, nullptr);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<SpvReflectBlockVariable*> pushConstants(numPushConstants);
  result = spvReflectEnumeratePushConstantBlocks(&module, &numPushConstants,
                                                 pushConstants.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<vk::PushConstantRange> pushConstantRanges;
  for (auto const& pushConstant : pushConstants) {
    if (pushConstant) {
      pushConstantRanges.emplace_back(type, pushConstant->offset,
                                      pushConstant->size);
    }
  }
  return pushConstantRanges;
}

}  // namespace vulkan_renderer

#endif