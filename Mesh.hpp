#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "Utilities.h"

class Mesh
{
public:
    Mesh();
    Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex> *vertices, std::vector<uint32_t> *indices);

    int GetVertexCount();
    VkBuffer GetVertexBuffer();

    int GetIndexCount();
    VkBuffer GetIndexBuffer();

    void DestroyBuffers();

    ~Mesh();

private:
    int m_vertexCount;
    VkBuffer m_vertexBuffer{};
    VkDeviceMemory m_vertexBufferMemory{};

    int m_indexCount;
    VkBuffer m_indexBuffer{};
    VkDeviceMemory m_indexBufferMemory{};

    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;

    void CreateVertexBuffer(std::vector<Vertex> *vertices, VkQueue transferQueue, VkCommandPool transferCommandPool);
    void CreateIndexBuffer(std::vector<uint32_t> *indices, VkQueue transferQueue, VkCommandPool transferCommandPool);
};
