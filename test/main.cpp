
#include <iostream>

#include "instance.hpp"
#include "vulkan/vulkan.hpp"
#include "window.hpp"

int main() {
  auto window = Window("test", 800, 500);
  // RenderApi renderer{window};

  Instance instance(window);
  auto device = instance.GetDevice();

  std::unordered_map<vk::ShaderStageFlagBits, ShaderDetails> shaders{
      {vk::ShaderStageFlagBits::eVertex,
       {LoadShader("../../shaders/vert.spv"),
        {{0, vk::DescriptorType::eUniformBuffer, 1,
          vk::ShaderStageFlagBits::eVertex, nullptr}}}},
      {vk::ShaderStageFlagBits::eFragment,
       {LoadShader("../../shaders/frag.spv"), {}}}};

  PipelineSettings pipelineSettings{shaders};
  pipelineSettings.AddVertexLayout<ColouredVertex2D>();
  auto pipeline = device->CreatePipeline(pipelineSettings);

  std::unique_ptr<Buffer> buffer =
      std::make_unique<VertexBuffer<ColouredVertex2D>>(
          std::vector<ColouredVertex2D>({{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                         {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                         {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}}));

  auto uniformData = std::make_shared<UniformDataImpl<MVP>>(
      MVP{2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});

  buffer->SetUniformData(vk::ShaderStageFlagBits::eVertex, uniformData);

  std::unique_ptr<Buffer> buffer2 =
      std::make_unique<IndexBuffer<ColouredVertex2D>>(
          std::vector<ColouredVertex2D>({{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                         {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                         {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                         {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}}),
          std::vector<uint16_t>({0, 1, 2, 2, 3, 0}));

  buffer2->SetUniformData(vk::ShaderStageFlagBits::eVertex, uniformData);

  Command command;
  // // TODO: need to add some kind of priority
  command.AddVertexBuffer(std::move(buffer2));
  command.AddVertexBuffer(std::move(buffer));
  auto commandId = pipeline->AddCommand(std::move(command));

  while (!window.ShouldClose()) {
    window.Update();
    device->Draw(pipeline, commandId);
  }
  device->WaitIdle();
}
