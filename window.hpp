#pragma once

#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "vulkan/vulkan.hpp"

class Window {
 public:
  Window(std::string title, int width, int height);
  ~Window();

  bool ShouldClose() const { return glfwWindowShouldClose(window_); }
  void Update() const {
    glfwPollEvents();
    glfwSwapBuffers(window_);
  }
  std::vector<char const*> GetVulkanExtensions() const;
  vk::UniqueSurfaceKHR GetVulkanSurface(vk::UniqueInstance const&) const;

  std::pair<uint32_t, uint32_t> Size() const { return {width_, height_}; }

  void BindResizeCallback(
      std::function<void(uint32_t, uint32_t)> const& callback) {
    resizeCallback_ = callback;
  }

  void ResizeCallback(uint32_t width, uint32_t height) {
    if (resizeCallback_) {
      resizeCallback_(width, height);
    }
  }

 protected:
  GLFWwindow* window_;
  uint32_t width_;
  uint32_t height_;
  std::function<void(int, int)> resizeCallback_;
};