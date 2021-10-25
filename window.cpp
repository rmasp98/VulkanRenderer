
#include "window.hpp"

static void framebufferResizeCallback(GLFWwindow* window, int width,
                                      int height) {
  auto win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
  win->ResizeCallback(width, height);
}

Window::Window(std::string title, int width, int height)
    : width_(width), height_(height) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  glfwSetWindowUserPointer(window_, this);
  glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
}

Window::~Window() {
  glfwDestroyWindow(window_);
  glfwTerminate();
}

std::vector<char const*> Window::GetVulkanExtensions() const {
  uint32_t count;
  char const** arrayExtensions = glfwGetRequiredInstanceExtensions(&count);
  std::vector<char const*> vectorExtensions;
  for (size_t i = 0; i < count; ++i) {
    vectorExtensions.push_back(arrayExtensions[i]);
  }
  return vectorExtensions;
}

vk::UniqueSurfaceKHR Window::GetVulkanSurface(
    vk::UniqueInstance const& instance) const {
  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface(VkInstance(instance.get()), window_, nullptr,
                              &surface) != VK_SUCCESS) {
    throw std::runtime_error("failed to create window surface!");
  }
  vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE> deleter(
      instance.get());
  return vk::UniqueSurfaceKHR(vk::SurfaceKHR(surface), deleter);
}
