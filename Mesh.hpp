#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>

#include "Utilities.h"

struct Model {
    glm::mat4 model;
};

class Mesh
{
public:
    Mesh();
    Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex> *vertices, std::vector<uint32_t> *indices);

    void SetModel(glm::mat4 newModel);
    Model GetModel();
    const Model* GetModelPtr();

    int GetVertexCount();
    VkBuffer GetVertexBuffer();

    int GetIndexCount();
    VkBuffer GetIndexBuffer();

    void DestroyBuffers();

    ~Mesh();

private:
    Model m_uboModel;

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
