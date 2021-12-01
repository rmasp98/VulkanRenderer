
#include <memory>

#include "device.hpp"
#include "vulkan/vulkan.hpp"

struct ShaderDetails {
  std::vector<char> File;
  std::vector<vk::DescriptorSetLayoutBinding> Bindings;
};

class Shader {
 public:
  Shader(vk::ShaderStageFlagBits type, ShaderDetails details,
         std::shared_ptr<Device> const& device)
      : type_(type),
        details_(details),
        module_(device->CreateShaderModule(details_.File)),
        layout_(device->CreateDescriptorSetLayout({{}, details_.Bindings})) {}
  ~Shader() = default;

  Shader(Shader const& other) = delete;
  Shader& operator=(Shader const& other) = delete;
  Shader(Shader&& other) = default;
  Shader& operator=(Shader&& other) = default;

  bool operator<(Shader const& r) const { return this->type_ < r.type_; }

  vk::ShaderStageFlagBits GetType() { return type_; }

  vk::PipelineShaderStageCreateInfo GetStage() const {
    return {{}, type_, module_.get(), "main"};
  }

  vk::DescriptorSetLayout GetLayout() const { return layout_.get(); }

 private:
  vk::ShaderStageFlagBits type_;
  ShaderDetails details_;
  vk::UniqueShaderModule module_;
  vk::UniqueDescriptorSetLayout layout_;
};
