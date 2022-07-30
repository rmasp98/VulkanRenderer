#ifndef VULKAN_RENDERER_DESCRIPTOR_SETS_HPP
#define VULKAN_RENDERER_DESCRIPTOR_SETS_HPP

#include "device_api.hpp"
#include "queues.hpp"
#include "shader.hpp"

namespace vulkan_renderer {

inline SetBindingsMap GetCombinedBindingsFromShaders(
    std::vector<Shader> const& shaders) {
  SetBindingsMap combinedSetBindings;
  for (auto& shader : shaders) {
    auto setBindings = shader.GetBindings();
    for (auto& [set, bindings] : setBindings) {
      if (combinedSetBindings.contains(set)) {
        combinedSetBindings[set].insert(combinedSetBindings[set].end(),
                                        bindings.begin(), bindings.end());
      } else {
        combinedSetBindings.insert({set, bindings});
      }
    }
  }
  return combinedSetBindings;
}

struct DescriptorSetLayout {
  DescriptorSetLayout(
      std::vector<vk::DescriptorSetLayoutBinding> const& bindings,
      DeviceApi const& device)
      : Bindings(bindings),
        Layout(device.CreateDescriptorSetLayout({{}, bindings})) {}

  std::vector<vk::DescriptorSetLayoutBinding> Bindings;
  vk::UniqueDescriptorSetLayout Layout;
};

class DescriptorSets {
 public:
  DescriptorSets(
      std::unordered_map<uint32_t, DescriptorSetLayout> const& layouts,
      DeviceApi const& device) {
    for (auto& [setIndex, layout] : layouts) {
      descriptorSets_.emplace(
          setIndex, DescriptorSet{layout.Bindings, layout.Layout, device});
    }
  }

  void AddUpdate(uint32_t const set, ImageIndex const imageIndex,
                 vk::WriteDescriptorSet& writeSet) {
    assert(descriptorSets_.contains(set) &&
           imageIndex < descriptorSets_.at(set).DescriptorSets.size());
    writeSet.setDstSet(
        descriptorSets_.at(set).DescriptorSets[imageIndex].get());
    updates_.push_back(writeSet);
  }

  void SubmitUpdates(DeviceApi const& device) {
    if (updates_.size()) {
      device.UpdateDescriptorSet(updates_);
      updates_.clear();
    }
  }

  void Bind(ImageIndex const imageIndex,
            vk::UniqueCommandBuffer const& cmdBuffer,
            vk::UniquePipelineLayout const& layout) {
    for (auto& [_, set] : descriptorSets_) {
      assert(imageIndex < set.DescriptorSets.size());
      cmdBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                    layout.get(), 0,
                                    {set.DescriptorSets[imageIndex].get()}, {});
    }
  }

 private:
  struct DescriptorSet {
    DescriptorSet(std::vector<vk::DescriptorSetLayoutBinding> const& bindings,
                  vk::UniqueDescriptorSetLayout const& layout,
                  DeviceApi const& device)
        : Bindings(bindings),
          DescriptorSets(device.AllocateDescriptorSet(layout.get())) {}

    std::vector<vk::DescriptorSetLayoutBinding> Bindings;
    std::vector<vk::UniqueDescriptorSet> DescriptorSets;
  };

  std::unordered_map<uint32_t, DescriptorSet> descriptorSets_;
  std::vector<vk::WriteDescriptorSet> updates_;
};

class DescriptorSetLayouts {
 public:
  DescriptorSetLayouts(std::vector<Shader> const& shaders,
                       DeviceApi const& device) {
    auto combinedSetBindings = GetCombinedBindingsFromShaders(shaders);
    for (auto& [set, bindings] : combinedSetBindings) {
      setLayouts_.emplace(set, DescriptorSetLayout{bindings, device});
    }
  }

  std::vector<vk::DescriptorSetLayout> GetLayouts() const {
    std::vector<vk::DescriptorSetLayout> layouts;
    for (auto& [_, set] : setLayouts_) {
      layouts.push_back(set.Layout.get());
    }
    return layouts;
  }

  std::unique_ptr<DescriptorSets> CreateDescriptorSets(
      DeviceApi const& device) {
    return std::make_unique<DescriptorSets>(setLayouts_, device);
  }

 private:
  std::unordered_map<uint32_t, DescriptorSetLayout> setLayouts_;
};

}  // namespace vulkan_renderer

#endif