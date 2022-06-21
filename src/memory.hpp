#pragma once

#include <atomic>
#include <iostream>
#include <map>
#include <unordered_map>

#include "device_api.hpp"
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

  uint32_t Allocate(DeviceApi const& device, vk::UniqueBuffer const& buffer,
                    vk::MemoryPropertyFlags const flags) {
    uint32_t typeIndex = uint32_t(~0);
    auto memoryRequirements = device.GetBufferMemoryRequirements(buffer);
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

    auto memory = device.AllocateMemory(memoryRequirements.size, typeIndex);

    // TODO: add offset to this
    device.BindBufferMemory(buffer, memory, 0);

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

  void* MapMemory(DeviceApi const& device, uint32_t id) {
    // TODO: assert id exists
    auto& mmd = allocations_[id];
    return device.MapMemory(mmd.Memory, mmd.Offset, mmd.Size);
  }

  void UnMapMemory(DeviceApi const& device, uint32_t id) {
    // TODO: assert id exists
    auto& mmd = allocations_[id];
    device.UnmapMemory(mmd.Memory);
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
