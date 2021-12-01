#include "utils.hpp"

#include <fstream>
#include <iostream>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* /*pUserData*/) {
  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
  }

  return VK_FALSE;
}

vk::DebugUtilsMessengerCreateInfoEXT CreateDebugMessengerInfo() {
  vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

  vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

  return {{}, severityFlags, messageTypeFlags, DebugCallback};
}

vk::UniqueInstance CreateInstance(std::string const& /*appName*/,
                                  std::vector<char const*> const& /*layers*/,
                                  std::vector<char const*>& /*extensions*/,
                                  uint32_t const apiVersion) {
  std::cout << "Test1" << std::endl;
  // vk::DynamicLoader loader;
  // VULKAN_HPP_DEFAULT_DISPATCHER.init(
  //     loader.getProcAddress<PFN_vkGetInstanceProcAddr>(
  //         "vkGetInstanceProcAddr"));

  // vk::ApplicationInfo appInfo(appName.c_str(), 1, "Crystalline", 1,
  // apiVersion);
  vk::ApplicationInfo appInfo("Test", 1, "Crystalline", 1, apiVersion);
  std::cout << "Test2" << std::endl;
  // vk::InstanceCreateInfo createInfo({}, &appInfo, layers, extensions);
  vk::InstanceCreateInfo createInfo({}, &appInfo, {}, {});
  std::cout << "Test3" << std::endl;

  // vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
  // if (layers.size() > 0) {
  //   extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  //   createInfo.setPEnabledExtensionNames(extensions);

  //  debugCreateInfo = CreateDebugMessengerInfo();
  //  createInfo.pNext = &debugCreateInfo;
  //}
  // std::cout << "Test4" << std::endl;
  try {
    auto instance = vk::createInstanceUnique(createInfo);
    std::cout << "Test5" << std::endl;
    // VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    std::cout << "Test6" << std::endl;

    return instance;
  } catch (vk::SystemError& err) {
    std::cout << "vk::SystemError: " << err.what() << std::endl;
    exit(-1);
  } catch (std::exception& err) {
    std::cout << "std::exception: " << err.what() << std::endl;
    exit(-1);
  } catch (...) {
    std::cout << "unknown error\n";
    exit(-1);
  }
}

vk::UniqueDebugUtilsMessengerEXT CreateDebugUtilsMessenger(
    vk::UniqueInstance const& instance) {
  return instance->createDebugUtilsMessengerEXTUnique(
      CreateDebugMessengerInfo());
}

std::vector<vk::PipelineShaderStageCreateInfo> CreateShaderStages(
    vk::UniqueDevice const& device,
    std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> const&
        files) {
  // TODO: Assert that must contain vertex and fragment shaders?
  std::vector<vk::PipelineShaderStageCreateInfo> createInfos;
  for (auto file : files) {
    auto shaderModule = device->createShaderModule(
        {{},
         file.second.size(),
         reinterpret_cast<const uint32_t*>(file.second.data())});
    createInfos.push_back({{}, file.first, shaderModule, "main"});
  }
  return createInfos;
}

std::vector<char> LoadShader(char const* filename) {
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

std::vector<uint32_t> LoadShader2(char const* filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  auto conv = reinterpret_cast<const uint32_t*>(buffer.data());
  std::vector<uint32_t> uintBuffer(conv, conv + sizeof(buffer));

  return uintBuffer;
}

