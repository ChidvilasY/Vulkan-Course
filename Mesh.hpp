#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "Utilities.h"

class Mesh
{
public:
    Mesh();
    Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex> *vertices);

    int GetVertexCount();
    VkBuffer GetVertexBuffer();

    void DestoryVertexBuffer();

    ~Mesh();

private:
    int m_vertexCount;
    VkBuffer m_vertexBuffer{};
    VkDeviceMemory m_vertexBufferMemory{};

    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;

    VkBuffer CreateVertexBuffer(std::vector<Vertex> *vertices);

    uint32_t FindMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties);
};
