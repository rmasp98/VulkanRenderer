#ifndef VULKAN_RENDERER_MEMORY_HPP
#define VULKAN_RENDERER_MEMORY_HPP

#include <atomic>
#include <iostream>
#include <map>
#include <unordered_map>

#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

struct MemoryMetaData {
  vk::UniqueDeviceMemory Memory;
  uint32_t Offset;
  uint64_t Size;
};

class Allocation {
 public:
  Allocation(uint32_t const id, std::function<void(uint32_t)> const deallocator)
      : id_(id), deallocator_(deallocator) {}

  ~Allocation() {
    if (deallocator_) deallocator_(id_);
  }

  Allocation(Allocation const&) = delete;
  Allocation& operator=(Allocation const&) = delete;
  Allocation(Allocation&&) = default;
  Allocation& operator=(Allocation&&) = default;

  uint32_t Get() const { return id_; }

 private:
  uint32_t id_;
  std::function<void(uint32_t)> deallocator_;
};

// TODO: move this to get memory from an allocator
// (https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
class MemoryAllocator {
 public:
  // TODO: this should take device to create memory
  MemoryAllocator(vk::PhysicalDevice const& device)
      : memoryProperties_(device.getMemoryProperties()) {}

  Allocation Allocate(vk::Buffer const& buffer,
                      vk::MemoryPropertyFlags const flags,
                      vk::Device const& device) {
    auto memoryRequirements = device.getBufferMemoryRequirements(buffer);
    auto allocation = Allocate(memoryRequirements, flags, device);

    // TODO: add offset to this
    device.bindBufferMemory(buffer,
                            allocations_.at(allocation.Get()).Memory.get(), 0);
    return allocation;
  }

  Allocation Allocate(vk::Image const& image,
                      vk::MemoryPropertyFlags const flags,
                      vk::Device const& device) {
    auto memoryRequirements = device.getImageMemoryRequirements(image);
    auto allocation = Allocate(memoryRequirements, flags, device);

    // TODO: add offset to this
    device.bindImageMemory(image,
                           allocations_.at(allocation.Get()).Memory.get(), 0);
    return allocation;
  }

  // TODO: probably should make this thread safe
  void Deallocate(Allocation const& allocation) {
    Deallocate(allocation.Get());
  }

  void* MapMemory(Allocation const& allocation,
                  vk::Device const& device) const {
    // TODO: assert id exists
    auto& mmd = allocations_.at(allocation.Get());
    return device.mapMemory(mmd.Memory.get(), mmd.Offset, mmd.Size);
  }

  void UnmapMemory(Allocation const& allocation,
                   vk::Device const& device) const {
    // TODO: assert id exists
    auto& mmd = allocations_.at(allocation.Get());
    device.unmapMemory(mmd.Memory.get());
  }

  uint64_t GetOffset(Allocation const& allocation) const {
    auto& mmd = allocations_.at(allocation.Get());
    // Get offset from table
    return mmd.Offset;
  }

 protected:
  Allocation Allocate(vk::MemoryRequirements const& memoryRequirements,
                      vk::MemoryPropertyFlags const flags,
                      vk::Device const& device) {
    auto typeBits = memoryRequirements.memoryTypeBits;

    uint32_t typeIndex = uint32_t(~0);
    for (uint32_t i = 0; i < memoryProperties_.memoryTypeCount; i++) {
      if ((typeBits & 1) &&
          ((memoryProperties_.memoryTypes[i].propertyFlags & flags) == flags)) {
        typeIndex = i;
        break;
      }
      typeBits >>= 1;
    }
    assert(typeIndex != uint32_t(~0));

    auto memory =
        device.allocateMemoryUnique({memoryRequirements.size, typeIndex});

    static std::atomic<uint32_t> currentId = 0;
    Allocation allocation{currentId++,
                          [&](uint32_t const id) { Deallocate(id); }};
    auto a = MemoryMetaData{std::move(memory), 0, memoryRequirements.size};
    // TODO: probably should make this thread safe
    allocations_.insert({allocation.Get(), std::move(a)});

    return allocation;
  }

  // TODO: probably should make this thread safe
  void Deallocate(uint32_t const allocationId) {
    allocations_.erase(allocationId);
  }

 private:
  vk::PhysicalDeviceMemoryProperties memoryProperties_;
  std::map<uint32_t, MemoryMetaData> allocations_;
};

}  // namespace vulkan_renderer

#endif