
#include <iostream>

#include "buffers/image_buffer.hpp"
#include "instance.hpp"
#include "vulkan/vulkan.hpp"
#include "window.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

int main() {
  auto window = Window("test", 800, 500);
  // RenderApi renderer{window};

  Instance instance(window);
  auto device = instance.GetDevice();

  std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> shaders{
      {vk::ShaderStageFlagBits::eVertex,
       {LoadShader("../../shaders/vert.spv")}},
      {vk::ShaderStageFlagBits::eFragment,
       {LoadShader("../../shaders/frag.spv")}}};

  PipelineSettings pipelineSettings{shaders};
  // pipelineSettings.AddVertexLayout<ColouredVertex2D>();
  // pipelineSettings.AddVertexLayout<UVVertex2D>();
  pipelineSettings.AddVertexLayout<UVColouredVertex3D>();
  auto pipeline = device->CreatePipeline(pipelineSettings);

  auto uniformBuffer = std::make_shared<UniformBuffer<MVP>>(
      MVP{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, 0);

  std::unique_ptr<Buffer> uvBuffer =
      std::make_unique<IndexBuffer<UVColouredVertex3D>>(
          std::vector<UVColouredVertex3D>(
              {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
               {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
               {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
               {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

               {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
               {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
               {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
               {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}}),

          std::vector<uint16_t>({0, 1, 2, 2, 3, 0,
                                 /*Second Square*/
                                 4, 5, 6, 6, 7, 4}));

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
  float time = glfwGetTime();
  while (!window.ShouldClose()) {
    window.Update();
    device->Draw(pipeline, commandId);

    auto model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                             glm::vec3(0.0f, 0.0f, 1.0f));
    auto view =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    auto proj =
        glm::perspective(glm::radians(45.0f), 800.0f / 500.0f, 0.1f, 10.0f);
    proj[1][1] *= -1;

    auto mvp = proj * view * model;
    uniformBuffer->Update(*reinterpret_cast<MVP*>(&mvp));

    // Force 60 FPS
    auto waittime = (1.0f / 60.0f) - (glfwGetTime() - time);
    usleep(waittime * 1000000);
    time = glfwGetTime();
  }
  device->WaitIdle();
}
