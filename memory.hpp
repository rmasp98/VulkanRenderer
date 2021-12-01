#pragma once

#include <atomic>
#include <iostream>
#include <map>
#include <unordered_map>

#include "vulkan/vulkan.hpp"

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
  MemoryAllocator(vk::PhysicalDeviceMemoryProperties const& memoryProperties)
      : memoryProperties_(memoryProperties) {}

  uint32_t Allocate(vk::UniqueDevice const& device,
                    vk::UniqueBuffer const& buffer,
                    vk::MemoryPropertyFlags const flags) {
    uint32_t typeIndex = uint32_t(~0);
    auto memoryRequirements = device->getBufferMemoryRequirements(buffer.get());
    auto typeBits = memoryRequirements.memoryTypeBits;

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
  void Deallocate(uint32_t id) { allocations_.erase(id); }

  void* MapMemory(vk::UniqueDevice const& device, uint32_t id) {
    // TODO: assert id exists
    auto& mmd = allocations_[id];
    return device->mapMemory(mmd.Memory.get(), mmd.Offset, mmd.Size);
  }

  void UnMapMemory(vk::UniqueDevice const& device, uint32_t id) {
    // TODO: assert id exists
    auto& mmd = allocations_[id];
    device->unmapMemory(mmd.Memory.get());
  }

  uint64_t GetOffset(uint32_t id) {
    auto& mmd = allocations_[id];
    // Get offset from table
    return mmd.Offset;
  }

 private:
  vk::PhysicalDeviceMemoryProperties const memoryProperties_;
  std::map<uint32_t, MemoryMetaData> allocations_;
  std::atomic<uint32_t> currentId_;
};
