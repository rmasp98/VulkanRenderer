#ifndef VULKAN_RENDERER_FRAMEBUFFER_HPP
#define VULKAN_RENDERER_FRAMEBUFFER_HPP

#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

class Framebuffer {
 public:
  Framebuffer(vk::UniqueImageView&& imageView,
              vk::UniqueFramebuffer&& framebuffer)
      : imageView_(std::move(imageView)),
        framebuffer_(std::move(framebuffer)) {}
  ~Framebuffer() = default;

  Framebuffer(Framebuffer const& other) = delete;
  Framebuffer& operator=(Framebuffer const& other) = delete;
  Framebuffer(Framebuffer&& other) = default;
  Framebuffer& operator=(Framebuffer&& other) = default;

  vk::Framebuffer GetFramebuffer() const { return framebuffer_.get(); }

 private:
  vk::UniqueImageView imageView_;
  vk::UniqueFramebuffer framebuffer_;
};

}  // namespace vulkan_renderer

#endif