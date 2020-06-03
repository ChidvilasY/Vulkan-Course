#include <cstring>

#include "Mesh.hpp"

Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, VkQueue transferQueue, VkCommandPool transferCommandPool, std::vector<Vertex> *vertices, std::vector<uint32_t> *indices)
    : m_vertexCount(vertices->size()),
      m_indexCount(indices->size()),
      m_physicalDevice(newPhysicalDevice),
      m_device(newDevice)
{
    CreateVertexBuffer(vertices, transferQueue, transferCommandPool);
    CreateIndexBuffer(indices, transferQueue, transferCommandPool);
}

int Mesh::GetVertexCount()
{
    return m_vertexCount;
}

VkBuffer Mesh::GetVertexBuffer()
{
    return m_vertexBuffer;
}

int Mesh::GetIndexCount()
{
    return m_indexCount;
}

VkBuffer Mesh::GetIndexBuffer()
{
    return m_indexBuffer;
}

Mesh::~Mesh()
{
}

void Mesh::CreateVertexBuffer(std::vector<Vertex> *vertices, VkQueue transferQueue, VkCommandPool transferCommandPool)
{
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices->size();

    // Temporary Buffer to "Stage" vertex data before transferring to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // Create Buffer and allocate memory
    CreateBuffer(m_physicalDevice, m_device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &stagingBuffer, &stagingBufferMemory);

    // MAP MEMORY TO VERTEX BUFFER
    void *data;                                                          // 1. Create pointer to a point in normal memory
    vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data); // 2. Map the vertex buffer memory to that point
    memcpy(data, vertices->data(), bufferSize);                          // 3. Copy memory from vertices to the point
    vkUnmapMemory(m_device, stagingBufferMemory);                        // 4. Unmap the vertex buffer memory

    // Create buffer with TRANSFER_DST_BIT to mark as recipient of transfer data (also VERTEX_BUFFER)
    // Buffer memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not CPU(host)
    CreateBuffer(m_physicalDevice, m_device, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 &m_vertexBuffer, &m_vertexBufferMemory);

    CopyBuffer(m_device, transferQueue, transferCommandPool, stagingBuffer, m_vertexBuffer, bufferSize);

    // Clean up staging buffer parts
    vkFreeMemory(m_device, stagingBufferMemory, nullptr);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
}

void Mesh::CreateIndexBuffer(std::vector<uint32_t> *indices, VkQueue transferQueue, VkCommandPool transferCommandPool)
{
    VkDeviceSize bufferSize = sizeof(uint32_t) * indices->size();

    // Temporary Buffer to "Stage" index data before transferring to GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    // Create Buffer and allocate memory
    CreateBuffer(m_physicalDevice, m_device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &stagingBuffer, &stagingBufferMemory);

    // MAP MEMORY TO INDEX BUFFER
    void *data;                                                          // 1. Create pointer to a point in normal memory
    vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data); // 2. Map the index buffer memory to that point
    memcpy(data, indices->data(), bufferSize);                           // 3. Copy memory from vertices to the point
    vkUnmapMemory(m_device, stagingBufferMemory);                        // 4. Unmap the index buffer memory

    // Create buffer with TRANSFER_DST_BIT to mark as recipient of transfer data (also INDEX_BUFFER)
    // Buffer memory is to be DEVICE_LOCAL_BIT meaning memory is on the GPU and only accessible by it and not CPU(host)
    CreateBuffer(m_physicalDevice, m_device, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 &m_indexBuffer, &m_indexBufferMemory);

    CopyBuffer(m_device, transferQueue, transferCommandPool, stagingBuffer, m_indexBuffer, bufferSize);

    // Clean up staging buffer parts
    vkFreeMemory(m_device, stagingBufferMemory, nullptr);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
}

void Mesh::DestroyBuffers()
{
    vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
    m_vertexBufferMemory = nullptr;

    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    m_vertexBuffer = nullptr;

    vkFreeMemory(m_device, m_indexBufferMemory, nullptr);
    m_indexBufferMemory = nullptr;

    vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
    m_indexBuffer = nullptr;
}
