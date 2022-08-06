#pragma once

#include "vulkan/vulkan.hpp"

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

struct ColouredVertex2D {
  float Vertex[2];  // location 0
  float Colour[3];  // location 1

  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDetails(
      uint32_t binding) {
    return {{0, binding, vk::Format::eR32G32Sfloat,
             offsetof(ColouredVertex2D, Vertex)},
            {1, binding, vk::Format::eR32G32B32Sfloat,
             offsetof(ColouredVertex2D, Colour)}};
  }
};

struct UVVertex2D {
  float Vertex[2];  // location 0
  float UV[2];      // location 1

  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDetails(
      uint32_t binding) {
    return {
        {0, binding, vk::Format::eR32G32Sfloat, offsetof(UVVertex2D, Vertex)},
        {1, binding, vk::Format::eR32G32Sfloat, offsetof(UVVertex2D, UV)}};
  }
};

struct UVColouredVertex3D {
  std::array<float, 3> Vertex;  // location 0
  std::array<float, 3> Colour;  // location 1
  std::array<float, 2> UV;      // location 2

  static std::vector<vk::VertexInputAttributeDescription> GetAttributeDetails(
      uint32_t binding) {
    return {{0, binding, vk::Format::eR32G32B32Sfloat,
             offsetof(UVColouredVertex3D, Vertex)},
            {1, binding, vk::Format::eR32G32B32Sfloat,
             offsetof(UVColouredVertex3D, Colour)},
            {2, binding, vk::Format::eR32G32Sfloat,
             offsetof(UVColouredVertex3D, UV)}};
  }

  auto operator<=>(UVColouredVertex3D const& rhs) const = default;
};

struct MVP {
  float data[16];
  auto operator<=>(MVP const& rhs) const = default;
};