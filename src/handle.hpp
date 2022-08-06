#ifndef VULKAN_RENDERER_HANDLE_HPP
#define VULKAN_RENDERER_HANDLE_HPP

#include <functional>

namespace vulkan_renderer {

template <class T>
class Handle {
 public:
  Handle(T id, std::function<void(T const)> const& destructor)
      : id_(id), destructor_(destructor) {}

  ~Handle() {
    if (destructor_) {
      destructor_(id_);
    }
  }

  Handle(Handle const& other) = delete;
  Handle& operator=(Handle const& other) = delete;
  Handle(Handle&& other) = default;
  Handle& operator=(Handle&& other) = default;

  T Get() const { return id_; }

  void Reset() {
    if (destructor_) {
      destructor_(id_);
    }
    destructor_ = nullptr;
  }

 private:
  T id_;
  std::function<void(T const)> destructor_;
};

}  // namespace vulkan_renderer

#endif