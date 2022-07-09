
#include <iostream>

#include "buffers/image_buffer.hpp"
#include "instance.hpp"
#include "vulkan/vulkan.hpp"
#include "window.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main() {
  std::tuple<int, float, bool> test;
  float test2 = 10.0f;
  auto test3 = static_cast<void*>(&test2);
  std::get<1>(test) =
      *static_cast<std::tuple_element<1, decltype(test)>::type*>(test3);

  std::cout << std::get<1>(test) << std::endl;

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
  // pipelineSettings.AddVertexLayout<ColouredVertex2D>();
  pipelineSettings.AddVertexLayout<UVVertex2D>();
  auto pipeline = device->CreatePipeline(pipelineSettings);

  auto uniformBuffer = std::make_shared<UniformBuffer<MVP>>(
      MVP{2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, 0);

  std::unique_ptr<Buffer> uvBuffer = std::make_unique<IndexBuffer<UVVertex2D>>(
      std::vector<UVVertex2D>({{{-0.5f, -0.5f}, {0.0f, 0.0f}},
                               {{0.5f, -0.5f}, {1.0f, 0.0f}},
                               {{0.5f, 0.5f}, {1.0f, 1.0f}},
                               {{-0.5f, 0.5f}, {0.0f, 1.0f}}}),
      std::vector<uint16_t>({0, 1, 2, 2, 3, 0}));

  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load("../../test/vulkan.jpg", &texWidth, &texHeight,
                              &texChannels, STBI_rgb_alpha);

  std::vector<unsigned char> pixelVector(pixels,
                                         pixels + (texWidth * texHeight * 4));

  if (!pixels) {
    throw std::runtime_error("Could not read image file: ");
  }

  auto imageData = std::make_shared<UniformImage>(
      pixelVector,
      ImageProperties({static_cast<uint32_t>(texWidth),
                       static_cast<uint32_t>(texHeight), 1}),
      1);

  uvBuffer->AddUniform(uniformBuffer);
  uvBuffer->AddUniform(imageData);

  Command command;
  command.AddVertexBuffer(std::move(uvBuffer));
  auto commandId = pipeline->AddCommand(std::move(command));

  glfwSwapInterval(1);
  auto time = glfwGetTime();
  while (!window.ShouldClose()) {
    window.Update();
    device->Draw(pipeline, commandId);

    // Force 60 FPS
    auto waittime = (1.0f / 60.0f) - (glfwGetTime() - time);
    usleep(waittime * 1000000);
    time = glfwGetTime();
  }
  device->WaitIdle();
}
