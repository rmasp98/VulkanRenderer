
#include <iostream>

#include "buffers/image_buffer.hpp"
#include "instance.hpp"
#include "vulkan/vulkan.hpp"
#include "window.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

struct Vertex {
  glm::vec3 Position;
  glm::vec3 Colour;
  glm::vec2 UVs;

  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDetails(
      uint32_t binding) {
    return {
        {0, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Position)},
        {1, binding, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, Colour)},
        {2, binding, vk::Format::eR32G32Sfloat, offsetof(Vertex, UVs)}};
  }
  auto operator<=>(Vertex const& rhs) const = default;
};

template <>
struct std::hash<Vertex> {
  size_t operator()(Vertex const& vertex) const {
    return ((std::hash<glm::vec3>()(vertex.Position) ^
             (std::hash<glm::vec3>()(vertex.Colour) << 1)) >>
            1) ^
           (std::hash<glm::vec2>()(vertex.UVs) << 1);
  }
};

int main() {
  auto window = Window("test", 800, 500);

  Instance instance(window);
  auto device = instance.GetDevice(DeviceFeatures::SampleShading);

  std::unordered_map<vk::ShaderStageFlagBits, std::vector<char>> shaders{
      {vk::ShaderStageFlagBits::eVertex,
       {LoadShader("../../shaders/vert.spv")}},
      {vk::ShaderStageFlagBits::eFragment,
       {LoadShader("../../shaders/frag.spv")}}};

  PipelineSettings pipelineSettings{
      .Shaders = shaders,
      .Multisampling = {{}, vk::SampleCountFlagBits::e64, true, 0.2f}};
  pipelineSettings.AddVertexLayout<Vertex>();
  auto pipeline = device->CreatePipeline(pipelineSettings);

  auto uniformBuffer = std::make_shared<UniformBuffer<MVP>>(
      MVP{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, 0);

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

  std::unique_ptr<Buffer> mesh =
      std::make_unique<IndexBuffer<Vertex>>(vertices, indices);

  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load("../../test/viking_room.png", &texWidth,
                              &texHeight, &texChannels, STBI_rgb_alpha);

  std::vector<unsigned char> pixelVector(pixels,
                                         pixels + (texWidth * texHeight * 4));

  if (!pixels) {
    throw std::runtime_error("Could not read image file: ");
  }

  auto imageData = std::make_shared<UniformImage>(
      pixelVector,
      ImageProperties{.Extent = {static_cast<uint32_t>(texWidth),
                                 static_cast<uint32_t>(texHeight), 1},
                      .MipLevels = 10,
                      .MinLod = 5.0f,
                      .MaxLod = 10.0f,
                      .MipMapMode = vk::SamplerMipmapMode::eLinear},
      1);

  mesh->AddUniform(uniformBuffer);
  mesh->AddUniform(imageData);

  Command command;
  command.AddVertexBuffer(std::move(mesh));
  auto commandId = pipeline->AddCommand(std::move(command));

  auto view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  auto proj =
      glm::perspective(glm::radians(45.0f), 800.0f / 500.0f, 0.1f, 10.0f);
  proj[1][1] *= -1;

  float time = glfwGetTime();
  while (!window.ShouldClose()) {
    window.Update();
    device->Draw(pipeline, commandId);

    auto model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f),
                             glm::vec3(0.0f, 0.0f, 1.0f));

    auto mvp = proj * view * model;
    uniformBuffer->Update(*reinterpret_cast<MVP*>(&mvp));

    // Force 60 FPS
    auto waittime = (1.0f / 60.0f) - (glfwGetTime() - time);
    usleep(waittime * 1000000);
    time = glfwGetTime();
  }
  device->WaitIdle();
}
