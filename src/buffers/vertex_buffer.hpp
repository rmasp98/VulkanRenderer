#ifndef VULKAN_RENDERER_VERTEX_BUFFER_HPP
#define VULKAN_RENDERER_VERTEX_BUFFER_HPP

#include "device_api.hpp"
#include "device_buffer.hpp"
#include "queues.hpp"
#include "render_pass.hpp"
#include "uniform_buffer.hpp"
#include "vulkan/vulkan.hpp"

namespace vulkan_renderer {

class Buffer {
 public:
  virtual ~Buffer() = default;
  virtual void AddUniform(std::shared_ptr<Uniform> const&) = 0;
  virtual void AddPushConstant(std::shared_ptr<PushConstant>) = 0;

  virtual void Allocate(Queues const&, DeviceApi&, bool force = false) = 0;
  virtual void UpdateDescriptorSets(Pipeline const&, DeviceApi const&) = 0;
  virtual void ClearDescriptorSets() = 0;

  virtual bool IsOutdated() const = 0;

  virtual void Upload(Queues const&, DeviceApi&) = 0;
  virtual void UploadUniforms(ImageIndex const, Queues const&, DeviceApi&) = 0;
  virtual void UploadPushConstants(Pipeline const&,
                                   vk::CommandBuffer const&) = 0;

  virtual void Bind(ImageIndex const, Pipeline const&,
                    vk::CommandBuffer const&) const = 0;
  virtual void Draw(vk::CommandBuffer const&) const = 0;
};

template <class T>
class VertexBuffer : public Buffer {
 public:
  VertexBuffer(std::vector<T> const& data) : data_(data) {}

  void AddUniform(std::shared_ptr<Uniform> const& uniform) override {
    // TODO: Check to see if set/binding already exists
    uniforms_.push_back(uniform);
  }

  void AddPushConstant(std::shared_ptr<PushConstant> pushConstant) override {
    // TODO: Check to see if set/binding already exists
    pushConstant->SubscribeUpdates([&]() { isOutdated_ = true; });
    pushConstants_.push_back(pushConstant);
  }

  void Allocate(Queues const& queues, DeviceApi& device, bool force) {
    if (force || deviceBuffer_ == nullptr) {
      deviceBuffer_ = std::make_unique<OptimisedDeviceBuffer>(
          data_.size() * sizeof(T), vk::BufferUsageFlagBits::eVertexBuffer,
          device);
      Upload(queues, device);

      for (auto& uniform : uniforms_) {
        uniform->Allocate(queues, device);
      }
    }
  }

  void UpdateDescriptorSets(Pipeline const& pipeline, DeviceApi const& device) {
    descriptorSets_.erase(pipeline.GetId());

    auto descriptorSet = pipeline.CreateDescriptorSets(device);
    for (auto& uniform : uniforms_) {
      uniform->AddDescriptorSetUpdate(descriptorSet);
    }
    descriptorSet.SubmitUpdates(device);

    descriptorSets_.insert({pipeline.GetId(), std::move(descriptorSet)});
  }

  void ClearDescriptorSets() { descriptorSets_.clear(); }

  bool IsOutdated() const override { return isOutdated_; };

  void Upload(Queues const& queues, DeviceApi& device) override {
    // TODO: maybe check to see if already created and right size
    if (deviceBuffer_->IsOutdated()) {
      deviceBuffer_->Upload(data_.data(), queues, device);
    }
  }

  void UploadUniforms(ImageIndex const imageIndex, Queues const& queues,
                      DeviceApi& device) override {
    for (auto& uniform : uniforms_) {
      if (uniform && uniform->IsOutdated(imageIndex)) {
        uniform->Upload(imageIndex, queues, device);
      }
    }
  }

  void UploadPushConstants(Pipeline const& pipeline,
                           vk::CommandBuffer const& cmdBuffer) override {
    for (auto& pushConstant : pushConstants_) {
      pipeline.UploadPushConstants(pushConstant, cmdBuffer);
    }
    isOutdated_ = false;
  }

  void Bind(ImageIndex const imageIndex, Pipeline const& pipeline,
            vk::CommandBuffer const& cmdBuffer) const override {
    if (deviceBuffer_ && descriptorSets_.contains(pipeline.GetId())) {
      deviceBuffer_->BindVertex(cmdBuffer);
      pipeline.BindDescriptorSet(
          imageIndex, descriptorSets_.at(pipeline.GetId()), cmdBuffer);
    }
  }

  virtual void Draw(vk::CommandBuffer const& cmdBuffer) const override {
    cmdBuffer.draw(data_.size(), 1, 0, 0);
  }

 private:
  std::vector<T> const data_;
  std::unique_ptr<OptimisedDeviceBuffer> deviceBuffer_;
  std::vector<std::shared_ptr<Uniform>> uniforms_;
  std::vector<std::shared_ptr<PushConstant>> pushConstants_;
  std::unordered_map<PipelineId, DescriptorSets> descriptorSets_;
  bool isOutdated_ = true;
};

template <class T>
class IndexBuffer : public Buffer {
 public:
  IndexBuffer(std::vector<T> const& data, std::vector<uint32_t> const& indices)
      : vertexBuffer_(data), indices_(indices) {}

  virtual void AddUniform(std::shared_ptr<Uniform> const& uniform) override {
    vertexBuffer_.AddUniform(uniform);
  }

  void AddPushConstant(std::shared_ptr<PushConstant> pushConstant) override {
    vertexBuffer_.AddPushConstant(pushConstant);
  }

  void Allocate(Queues const& queues, DeviceApi& device, bool force) {
    vertexBuffer_.Allocate(queues, device, force);
    if (force || deviceBuffer_ == nullptr) {
      deviceBuffer_ = std::make_unique<OptimisedDeviceBuffer>(
          indices_.size() * sizeof(uint32_t),
          vk::BufferUsageFlagBits::eIndexBuffer, device);
      Upload(queues, device);
    }
  }

  void UpdateDescriptorSets(Pipeline const& pipeline, DeviceApi const& device) {
    vertexBuffer_.UpdateDescriptorSets(pipeline, device);
  }

  void ClearDescriptorSets() { vertexBuffer_.ClearDescriptorSets(); }

  bool IsOutdated() const override { return vertexBuffer_.IsOutdated(); };

  virtual void Upload(Queues const& queues, DeviceApi& device) override {
    vertexBuffer_.Upload(queues, device);
    if (deviceBuffer_->IsOutdated()) {
      deviceBuffer_->Upload(indices_.data(), queues, device);
    }
  }

  void UploadUniforms(ImageIndex const imageIndex, Queues const& queues,
                      DeviceApi& device) override {
    vertexBuffer_.UploadUniforms(imageIndex, queues, device);
  }

  void UploadPushConstants(Pipeline const& pipeline,
                           vk::CommandBuffer const& cmdBuffer) override {
    vertexBuffer_.UploadPushConstants(pipeline, cmdBuffer);
  }

  virtual void Bind(ImageIndex const imageIndex, Pipeline const& pipeline,
                    vk::CommandBuffer const& cmdBuffer) const override {
    vertexBuffer_.Bind(imageIndex, pipeline, cmdBuffer);
    if (deviceBuffer_) {
      deviceBuffer_->BindIndex(cmdBuffer);
    }
  }

  virtual void Draw(vk::CommandBuffer const& cmdBuffer) const override {
    cmdBuffer.drawIndexed(indices_.size(), 1, 0, 0, 0);
  }

 private:
  VertexBuffer<T> vertexBuffer_;
  std::vector<uint32_t> indices_;
  std::unique_ptr<OptimisedDeviceBuffer> deviceBuffer_;
};

}  // namespace vulkan_renderer

#endif