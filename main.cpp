
#include <iostream>
#include <memory>

#include "pipeline.hpp"
#include "render_api.hpp"
#include "utils.hpp"
#include "vulkan/vulkan.hpp"
#include "window.hpp"

int main() {
  auto window = Window("Test", 800, 600);
  RenderApi renderer{window};

  // TODO: setting bindings properly
  // std::unordered_map<vk::ShaderStageFlagBits, ShaderDetails> shaders{
  //    {vk::ShaderStageFlagBits::eVertex,
  //     {LoadShader("../shaders/vert.spv"), {}}},
  //    {vk::ShaderStageFlagBits::eFragment,
  //     {LoadShader("../shaders/frag.spv"), {}}}};

  // auto pipelineId = renderer.RegisterPipeline(shaders);

  // std::unique_ptr<VertexBuffer<ColouredVertex2D>> buffer(
  //     new VertexBuffer<ColouredVertex2D>(
  //         {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
  //          {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
  //          {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}}));

  // std::unique_ptr<IndexBuffer<ColouredVertex2D>> buffer2(
  //     new IndexBuffer<ColouredVertex2D>({{{-0.5f, -0.5f}, {1.0f, 0.0f,
  //     0.0f}},
  //                                        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
  //                                        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
  //                                        {{-0.5f, 0.5f},
  //                                        {1.0f, 1.0f, 1.0f}}},
  //                                       {0, 1, 2, 2, 3, 0}));

  // Command command;
  //// TODO: need to add some kind of priority
  // command.AddVertexBuffer(std::move(buffer2));
  // command.AddVertexBuffer(std::move(buffer));
  //// TODO: do we want to name the command so that we can disable/delete it
  // auto commandId = renderer.RegisterCommand(pipelineId, std::move(command));

  // while (!window.ShouldClose()) {
  //   window.Update();
  //   renderer.Draw(pipelineId, commandId);
  // }
  // renderer.WaitIdle();
}
