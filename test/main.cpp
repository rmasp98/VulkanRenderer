
#include <iostream>

#include "buffers/image_buffer.hpp"
#include "containers.hpp"
#include "instance.hpp"
#include "vulkan/vulkan.hpp"
#include "window.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main() {
  auto window = Window("test", 800, 500);

  vulkan_renderer::Instance instance(window);
  auto device =
      instance.GetDevice(vulkan_renderer::DeviceFeatures::SampleShading);

  auto renderPass =
      device->CreateRenderPass({true, vk::SampleCountFlagBits::e64});

  std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> shaders{
      {vk::ShaderStageFlagBits::eVertex,
       {vulkan_renderer::LoadShader("../../test/vert.spv")}},
      {vk::ShaderStageFlagBits::eFragment,
       {vulkan_renderer::LoadShader("../../test/frag.spv")}}};

  vulkan_renderer::PipelineSettings pipelineSettings{
      .Shaders = shaders,
      .Multisampling = {{}, vk::SampleCountFlagBits::e64, true, 0.2f}};
  pipelineSettings.Rasterizer.setFrontFace(vk::FrontFace::eCounterClockwise);
  pipelineSettings.AddVertexLayout<Vertex>();
  auto pipeline = device->CreatePipeline(pipelineSettings, renderPass);

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                        "../../test/viking_room.obj")) {
    throw std::runtime_error(warn + err);
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::unordered_map<Vertex, uint32_t> uniqueVertices;
  for (const auto& shape : shapes) {
    for (const auto& index : shape.mesh.indices) {
      Vertex vertex{{attrib.vertices[3 * index.vertex_index + 0],
                     attrib.vertices[3 * index.vertex_index + 1],
                     attrib.vertices[3 * index.vertex_index + 2]},
                    {1.0f, 1.0f, 1.0f},
                    {attrib.texcoords[2 * index.texcoord_index + 0],
                     1.0f - attrib.texcoords[2 * index.texcoord_index + 1]}};

      if (uniqueVertices.count(vertex) == 0) {
        uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
        vertices.push_back(vertex);
      }

      indices.push_back(uniqueVertices[vertex]);
    }
  }

  auto mesh =
      std::make_shared<vulkan_renderer::IndexBuffer<Vertex>>(vertices, indices);

  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load("../../test/viking_room.png", &texWidth,
                              &texHeight, &texChannels, STBI_rgb_alpha);

  std::vector<unsigned char> pixelVector(pixels,
                                         pixels + (texWidth * texHeight * 4));

  if (!pixels) {
    throw std::runtime_error("Could not read image file: ");
  }

  auto imageData = std::make_shared<vulkan_renderer::UniformImage>(
      pixelVector,
      vulkan_renderer::ImageProperties{
          .Extent = {static_cast<uint32_t>(texWidth),
                     static_cast<uint32_t>(texHeight), 1},
          // .MipLevels = 10,
          // .MinLod = 5.0f,
          // .MaxLod = 10.0f,
          // .MipMapMode = vk::SamplerMipmapMode::eLinear
      },
      1);

  auto uniform = std::make_shared<vulkan_renderer::PushConstantData<MVP>>(
      MVP{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
      vk::ShaderStageFlagBits::eVertex, 0);
  mesh->AddPushConstant(uniform);

  // auto uniform = std::make_shared<vulkan_renderer::UniformBuffer<MVP>>(
  //     MVP{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, 0);
  // mesh->AddUniform(uniform);

  mesh->AddUniform(imageData);

  vulkan_renderer::Command command;
  command.AddVertexBuffer(mesh);
  auto commandHandle = device->AddCommand(std::move(command));

  auto view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  auto proj =
      glm::perspective(glm::radians(45.0f), 800.0f / 500.0f, 0.1f, 10.0f);
  proj[1][1] *= -1;

  float time = glfwGetTime();
  while (!window.ShouldClose()) {
    window.Update();

    device->StartRender(renderPass);

    auto model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f),
                             glm::vec3(0.0f, 0.0f, 1.0f));

    auto mvp = proj * view * model;
    uniform->Update(*reinterpret_cast<MVP*>(&mvp));

    device->Draw(commandHandle, pipeline);

    device->PresentRender();

    // Force 60 FPS
    // TODO: this can be done by setting present mode to
    // VK_PRESENT_MODE_FIFO_KHR
    auto waittime = (1.0f / 60.0f) - (glfwGetTime() - time);
    if (waittime > 0) {
      usleep(waittime * 1000000);
    }
    time = glfwGetTime();
  }
  device->WaitIdle();
}
