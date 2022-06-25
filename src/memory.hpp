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
  MemoryAllocator() = default;

  AllocationId Allocate(
      vk::UniqueBuffer const& buffer, vk::MemoryPropertyFlags const flags,
      vk::PhysicalDeviceMemoryProperties const& memoryProperties,
      vk::UniqueDevice const& device) {
    uint32_t typeIndex = uint32_t(~0);
    auto memoryRequirements = device->getBufferMemoryRequirements(buffer.get());
    auto typeBits = memoryRequirements.memoryTypeBits;

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
      if ((typeBits & 1) &&
          ((memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)) {
        typeIndex = i;
        break;
      }
      typeBits >>= 1;
    }
    assert(typeIndex != uint32_t(~0));

    auto memory =
        device->allocateMemoryUnique({memoryRequirements.size, typeIndex});

    // TODO: add offset to this
    device->bindBufferMemory(buffer.get(), memory.get(), 0);

    auto id = currentId_++;

    // TODO: figure out how to construct this in the insert
    auto a = MemoryMetaData{std::move(memory), 0, memoryRequirements.size};
    // TODO: probably should make this thread safe
    allocations_.insert({id, std::move(a)});

    // Return the id for allocation
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

 private:
  vk::PhysicalDeviceMemoryProperties const memoryProperties_;
  std::map<AllocationId, MemoryMetaData> allocations_;
  std::atomic<uint32_t> currentId_;
};
