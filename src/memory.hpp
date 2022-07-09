#pragma once

#include <atomic>
#include <iostream>
#include <map>
#include <unordered_map>

#include "vulkan/vulkan.hpp"

using AllocationId = uint32_t;

struct MemoryMetaData {
  vk::UniqueDeviceMemory Memory;
  uint32_t Offset;
  uint64_t Size;
};

// TODO: move this to get memory from an allocator
// (https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
class MemoryAllocator {
 public:
  // TODO: this should take device to create memory
  MemoryAllocator(vk::PhysicalDevice const& device)
      : memoryProperties_(device.getMemoryProperties()) {}

  AllocationId Allocate(vk::UniqueBuffer const& buffer,
                        vk::MemoryPropertyFlags const flags,
                        vk::UniqueDevice const& device) {
    auto memoryRequirements = device->getBufferMemoryRequirements(buffer.get());
    auto id = Allocate(memoryRequirements, flags, device);

    // TODO: add offset to this
    device->bindBufferMemory(buffer.get(), allocations_.at(id).Memory.get(), 0);
    return id;
  }

  AllocationId Allocate(vk::UniqueImage const& image,
                        vk::MemoryPropertyFlags const flags,
                        vk::UniqueDevice const& device) {
    auto memoryRequirements = device->getImageMemoryRequirements(image.get());
    auto id = Allocate(memoryRequirements, flags, device);

    // TODO: add offset to this
    device->bindImageMemory(image.get(), allocations_.at(id).Memory.get(), 0);
    return id;
  }

  // TODO: probably should make this thread safe
  void Deallocate(AllocationId const id) { allocations_.erase(id); }

  void* MapMemory(AllocationId const id, vk::UniqueDevice const& device) const {
    // TODO: assert id exists
    auto& mmd = allocations_.at(id);
    return device->mapMemory(mmd.Memory.get(), mmd.Offset, mmd.Size);
  }

  void UnmapMemory(AllocationId const id,
                   vk::UniqueDevice const& device) const {
    // TODO: assert id exists
    auto& mmd = allocations_.at(id);
    device->unmapMemory(mmd.Memory.get());
  }

  uint64_t GetOffset(AllocationId const id) const {
    auto& mmd = allocations_.at(id);
    // Get offset from table
    return mmd.Offset;
  }

 protected:
  AllocationId Allocate(vk::MemoryRequirements const& memoryRequirements,
                        vk::MemoryPropertyFlags const flags,
                        vk::UniqueDevice const& device) {
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
        device->allocateMemoryUnique({memoryRequirements.size, typeIndex});

    auto id = currentId_++;
    auto a = MemoryMetaData{std::move(memory), 0, memoryRequirements.size};
    // TODO: probably should make this thread safe
    allocations_.insert({id, std::move(a)});

    return id;
  }

 private:
  vk::PhysicalDeviceMemoryProperties memoryProperties_;
  std::map<AllocationId, MemoryMetaData> allocations_;
  std::atomic<uint32_t> currentId_;
};
