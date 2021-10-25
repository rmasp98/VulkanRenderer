#pragma once

#include <functional>
#include <memory>

#include "memory.hpp"
#include "queues.hpp"
#include "utils.hpp"
#include "vulcan_defaults.hpp"
#include "vulkan/vulkan.hpp"

using TransferFunction = std::function<void(vk::UniqueBuffer const&,
                                            vk::UniqueBuffer const&, uint32_t)>;

class Pipeline;

inline vk::BufferUsageFlags GetBufferFlags(bool const optimise) {
  if (optimise) {
    return vk::BufferUsageFlagBits::eTransferDst;
  }
  return {};
}

inline vk::MemoryPropertyFlags GetMemoryFlags(bool const optimise) {
  if (optimise) {
    return vk::MemoryPropertyFlagBits::eDeviceLocal;
  }
  return vk::MemoryPropertyFlagBits::eHostVisible |
         vk::MemoryPropertyFlagBits::eHostCoherent;
}

class BufferRef {
 public:
  BufferRef(vk::UniqueDevice const& device,
            std::shared_ptr<MemoryAllocator>& allocator, uint32_t const size,
            vk::BufferUsageFlags const bufferUsage, bool const optimise)
      : buffer_(device->createBufferUnique(
            {{}, size, bufferUsage | GetBufferFlags(optimise)})),
        id_(allocator->Allocate(device, buffer_, GetMemoryFlags(optimise))),
        optimise_(optimise),
        size_(size),
        offset_(allocator->GetOffset(id_)),
        allocator_(allocator) {}

  ~BufferRef() {
    if (auto allocator = allocator_.lock()) {
      allocator->Deallocate(id_);
    }
  }

  void Upload(vk::UniqueDevice const& device, void const* data,
              TransferFunction transferFuncion) {
    if (auto allocator = allocator_.lock()) {
      if (optimise_) {
        auto [transferBuffer, transferId] =
            UploadForTransfer(device, data, size_);
        transferFuncion(transferBuffer, buffer_, size_);
        allocator->Deallocate(transferId);
      } else {
        auto memoryLocation = allocator->MapMemory(device, id_);
        memcpy(memoryLocation, data, size_);
        allocator->UnMapMemory(device, id_);
      }
    }
  }

  void Bind(vk::UniqueCommandBuffer const& cmdBuffer) {
    cmdBuffer->bindVertexBuffers(0, buffer_.get(), {offset_});
  }

  void BindIndex(vk::UniqueCommandBuffer const& cmdBuffer) {
    cmdBuffer->bindIndexBuffer(buffer_.get(), {offset_},
                               vk::IndexType::eUint16);
  }

 private:
  vk::UniqueBuffer const buffer_;
  uint32_t const id_;
  bool const optimise_;
  uint32_t const size_;
  uint32_t const offset_;
  std::weak_ptr<MemoryAllocator> allocator_;

  std::pair<vk::UniqueBuffer, uint32_t> UploadForTransfer(
      vk::UniqueDevice const& device, void const* data, uint32_t const size) {
    if (auto allocator = allocator_.lock()) {
      auto transferBuffer = device->createBufferUnique(
          {{}, size, vk::BufferUsageFlagBits::eTransferSrc});

      auto transferId =
          allocator->Allocate(device, transferBuffer,
                              vk::MemoryPropertyFlagBits::eHostVisible |
                                  vk::MemoryPropertyFlagBits::eHostCoherent);

      auto memoryLocation = allocator->MapMemory(device, transferId);
      // TODO: do this safely
      memcpy(memoryLocation, data, size);
      allocator->UnMapMemory(device, transferId);

      return {std::move(transferBuffer), transferId};
    }
    return {};
  }
};

class Device {
 public:
  Device(vk::PhysicalDevice const&, vk::SurfaceFormatKHR const&,
         QueueFamilies const&, vk::UniqueSurfaceKHR const&, vk::Extent2D&);
  ~Device() = default;

  Device(Device const&) = delete;
  Device& operator=(Device const&) = delete;
  Device(Device&& other) = default;
  Device& operator=(Device&& other) = default;

  void RecreateSwapchain(vk::UniqueSurfaceKHR const&, vk::Extent2D&);

  vk::UniquePipelineLayout CreatePipelineLayout(
      vk::PipelineLayoutCreateInfo const&) const;
  vk::UniqueRenderPass CreateRenderpass(vk::RenderPassCreateInfo const&) const;
  vk::UniquePipelineCache CreatePipelineCache(
      vk::PipelineCacheCreateInfo const&) const;
  vk::UniqueDescriptorSetLayout CreateDescriptorSetLayout(
      vk::DescriptorSetLayoutCreateInfo const&) const;

  vk::UniquePipeline CreatePipeline(
      vk::UniquePipelineCache const& cache,
      vk::GraphicsPipelineCreateInfo const& settings) const;

  std::vector<Framebuffer> CreateFramebuffers(
      vk::UniqueRenderPass const& renderPass, vk::Extent2D const& extent,
      vk::ComponentMapping const& componentMapping =
          defaults::framebuffer::ComponentMapping,
      vk::ImageSubresourceRange const& subResourceRange =
          defaults::framebuffer::SubResourceRange) const;

  std::unordered_map<vk::ShaderStageFlagBits, vk::UniqueShaderModule>
  CreateShaderModules(
      std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> const&
          shaderFiles) const;

  uint32_t GetNextImage();

  void SubmitToGraphics(vk::UniqueCommandBuffer& cmdBuffer) const;
  void SubmitToPresent(uint32_t const imageIndex);

  std::vector<vk::UniqueCommandBuffer> AllocateCommandBuffer(
      vk::CommandBufferLevel bufferLevel, uint32_t numBuffers) const;

  std::vector<vk::UniqueDescriptorSet> AllocateDescriptorSet(
      vk::DescriptorSetLayout const& layout) const;

  std::unique_ptr<BufferRef> UploadBuffer(
      void const* data, uint32_t const size,
      vk::BufferUsageFlags const bufferUsage, bool const optimise = false);
  void UploadBuffer(std::unique_ptr<BufferRef>&, void const* data);

  void WaitIdle() { device_->waitIdle(); }

 private:
  vk::PhysicalDevice physicalDevice_;
  vk::SurfaceFormatKHR surfaceFormat_;
  vk::UniqueDevice device_;
  Queues queues_;
  vk::UniqueCommandPool commandPool_;
  vk::UniqueSwapchainKHR swapchain_;
  uint32_t numSwapchainImages_;
  vk::UniqueDescriptorPool descriptorPool_;
  Semaphores semaphores_;
  std::shared_ptr<MemoryAllocator> allocator_;

  vk::UniqueSwapchainKHR CreateSwapchain(
      vk::UniqueSurfaceKHR const&, vk::Extent2D&,
      vk::UniqueSwapchainKHR const& oldSwapchain = vk::UniqueSwapchainKHR());

  vk::UniqueDeviceMemory AllocateMemory(
      vk::UniqueBuffer const& buffer,
      vk::MemoryPropertyFlags const flags) const;

  void TransferBuffer(vk::UniqueBuffer const& sourceBuffer,
                      vk::UniqueBuffer const& destBuffer, uint32_t size);

  vk::UniqueDescriptorPool CreateDescriptorPool() {
    auto descriptorSizes = vk::DescriptorPoolSize(
        vk::DescriptorType::eUniformBuffer, numSwapchainImages_);
    return device_->createDescriptorPoolUnique(
        {{}, numSwapchainImages_, descriptorSizes});
  }
};

DeviceFeatures GetDeviceFeatures(vk::PhysicalDevice const&);

int CalculateScore(vk::PhysicalDevice const&, vk::SurfaceFormatKHR const&,
                   QueueFamilies const&);

vk::SurfaceFormatKHR SelectSurfaceFormat(vk::PhysicalDevice const&,
                                         vk::UniqueSurfaceKHR const&);

class DeviceDetails {
 public:
  DeviceDetails(vk::PhysicalDevice const& device,
                vk::UniqueSurfaceKHR const& surface)
      : device_(device),
        surfaceFormat_(SelectSurfaceFormat(device, surface)),
        queueFamilies_(device, surface),
        features_(GetDeviceFeatures(device)),
        score_(CalculateScore(device, surfaceFormat_, queueFamilies_)) {}

  // TODO: get all possible formats and allow user to select one

  DeviceFeatures GetFeatures() const { return features_; }
  int GetScore() const { return score_; }

  std::shared_ptr<Device> CreateDevice(vk::UniqueSurfaceKHR const& surface,
                                       vk::Extent2D& extent) const {
    return std::make_shared<Device>(device_, surfaceFormat_, queueFamilies_,
                                    surface, extent);
  }

 private:
  vk::PhysicalDevice device_;
  vk::SurfaceFormatKHR surfaceFormat_;
  QueueFamilies queueFamilies_;
  DeviceFeatures features_;
  int score_;
};

std::vector<DeviceDetails> GetSuitableDevices(vk::UniqueInstance const&,
                                              vk::UniqueSurfaceKHR const&,
                                              DeviceFeatures const);

vk::UniqueDevice CreateDevice(vk::PhysicalDevice const&,
                              std::vector<uint32_t> const& queueFamilyIndices,
                              std::vector<char const*> const& extensions,
                              vk::PhysicalDeviceFeatures const*);

vk::SurfaceTransformFlagBitsKHR SelectPreTransform(
    vk::SurfaceCapabilitiesKHR const&);

vk::CompositeAlphaFlagBitsKHR SelectCompositeAlpha(
    vk::SurfaceCapabilitiesKHR const&);

uint32_t SelectImageCount(vk::SurfaceCapabilitiesKHR const&);

vk::Extent2D SetExtent(vk::Extent2D const& windowExtent,
                       vk::SurfaceCapabilitiesKHR const&);

vk::PresentModeKHR SelectPresentMode(vk::PhysicalDevice const&,
                                     vk::UniqueSurfaceKHR const&);
