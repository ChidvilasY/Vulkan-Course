#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "Utilities.h"

class Mesh
{
public:
    Mesh();
    Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex> *vertices);

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

    void CreateVertexBuffer(std::vector<Vertex> *vertices, VkQueue transferQueue, VkCommandPool transferCommandPool);
};
