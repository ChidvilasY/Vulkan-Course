#include <cstring>

#include "Mesh.hpp"

Mesh::Mesh()
{
}

Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex> *vertices)
    : m_vertexCount(vertices->size()),
      m_physicalDevice(newPhysicalDevice),
      m_device(newDevice)
{
    CreateVertexBuffer(vertices);
}

int Mesh::GetVertexCount()
{
    return m_vertexCount;
}

VkBuffer Mesh::GetVertexBuffer()
{
    return m_vertexBuffer;
}

Mesh::~Mesh()
{
}

VkBuffer Mesh::CreateVertexBuffer(std::vector<Vertex> *vertices)
{
    // CREATE VERTEX BUFFER
    // Information to create a buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(Vertex) * vertices->size();  // Size of buffer (size of 1 vertex * number of vertices)
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; // Multiple types of buffer possible, we want Vertex Buffer
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;   // Similar to swapchain images, can share vertex buffers

    VkResult result = vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_vertexBuffer);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vertex Buffer");
    }

    // GET BUFFER MEMORY REQUIREMENTS
    VkMemoryRequirements memRequirements = {};
    vkGetBufferMemoryRequirements(m_device, m_vertexBuffer, &memRequirements);

    // ALLOCATE MEMORY TO BUFFER
    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memRequirements.size;
    memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(memRequirements.memoryTypeBits,            // Index of memory type on Physical Device that has required bit flags
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |      // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT    : CPU can interact with memory
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT); // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT   : Allows placement of data straight into buffer after mapping (otherwise would have to specify manually)

    // Allocate memory to VkDeviceMemory
    result = vkAllocateMemory(m_device, &memAllocInfo, nullptr, &m_vertexBufferMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate device memory");
    }

    // Allocate memory to given vertex buffer
    vkBindBufferMemory(m_device, m_vertexBuffer, m_vertexBufferMemory, 0);

    // MAP MEMORY TO VERTEX BUFFER
    void *data;                                                                // 1. Create pointer to a point in normal memory
    vkMapMemory(m_device, m_vertexBufferMemory, 0, bufferInfo.size, 0, &data); // 2. Map the vertex buffer memory to that point
    memcpy(data, vertices->data(), (size_t)bufferInfo.size);                   // 3. Copy memory from vertices to the point
    vkUnmapMemory(m_device, m_vertexBufferMemory);                             // 4. Unmap the vertex buffer memory
}

void Mesh::DestoryVertexBuffer()
{
    vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);
    m_vertexBufferMemory = nullptr;

    vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
    m_vertexBuffer = nullptr;
}

uint32_t Mesh::FindMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
    // Get propeties of physical device memory
    VkPhysicalDeviceMemoryProperties memProperties = {};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        // Index of memory type must match corresponding bit in allowedTypes
        // AND
        // Desired property bit flags are part of memory type's property
        if ((allowedTypes & (1 << i)) && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties))
        {
            // This memory type is valid, so return it's value
            return i;
        }
    }

    throw std::runtime_error("Failed to find memory type index");
}
