
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
       {LoadShader("../../shaders/vert.spv"), {}}},
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

  std::unique_ptr<Buffer> buffer2 =
      std::make_unique<IndexBuffer<ColouredVertex2D>>(
          std::vector<ColouredVertex2D>({{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                         {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                         {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                         {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}}),
          std::vector<uint16_t>({0, 1, 2, 2, 3, 0}));

  Command command;
  // // TODO: need to add some kind of priority
  command.AddVertexBuffer(std::move(buffer2));
  command.AddVertexBuffer(std::move(buffer));
  auto commandId = pipeline->AddCommand(std::move(command));
  (void)commandId;

  while (!window.ShouldClose()) {
    window.Update();
    device->Draw(pipeline, commandId);
  }
  device->WaitIdle();
}
