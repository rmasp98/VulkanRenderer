#ifndef VULKAN_RENDERER_DEBUG_HPP
#define VULKAN_RENDERER_DEBUG_HPP

#include "vulkan/vulkan.hpp"

#if !defined(NDEBUG)
#define DEBUG_DEFAULT_ENABLE true
#else
#define DEBUG_DEFAULT_ENABLE false
#endif

namespace vulkan_renderer {

class DebugMessenger {
 public:
  DebugMessenger(bool enable = DEBUG_DEFAULT_ENABLE) : enabled_(enable) {
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

    createInfo_ = {
        {}, severityFlags, messageTypeFlags, DebugMessenger::DebugCallback};
  }

  void Initialise(vk::Instance& instance) {
    if (enabled_) {
      debugUtilsMessenger_ =
          instance.createDebugUtilsMessengerEXTUnique(createInfo_);
    }
  }

  vk::DebugUtilsMessengerCreateInfoEXT const* GetCreateInfo() const {
    return enabled_ ? &createInfo_ : nullptr;
  }

  void AddDebugConfig(std::vector<char const*>& layers,
                      std::vector<char const*>& extensions) const {
    if (enabled_) {
      layers.push_back("VK_LAYER_KHRONOS_validation");
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
  }

 private:
  bool enabled_;
  vk::DebugUtilsMessengerCreateInfoEXT createInfo_;
  vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger_;

  static VKAPI_ATTR VkBool32 VKAPI_CALL
  DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* /*pUserData*/) {
    // TODO: Do this properly
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
      std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    }

    return VK_FALSE;
  }
};

}  // namespace vulkan_renderer

#endif