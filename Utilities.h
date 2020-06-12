#pragma once

#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

constexpr int MAX_FRAME_DRAWS = 2;
constexpr int MAX_OBJECTS = 2;


const std::vector<const char *> gDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// Vertex data representation
struct Vertex
{
    glm::vec3 pos; // Vertex Position (x, y, z)
    glm::vec3 col; // Vertex Color (r, g, b)
};

// Indices (locations) of Queue Families (if they exist at all)
struct QueueFamilyIndices
{
    int graphicsFamily = -1;     // Locaiton of Graphics Queue Family
    int presentationFamily = -1; // Location of Presentation Queue Family

    // Check if Queue families are valid
    bool IsValid()
    {
        return (graphicsFamily >= 0) && (presentationFamily >= 0);
    }
};

struct SwapChainDetails
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};  // Surface properties, e.g. image size/extent
    std::vector<VkSurfaceFormatKHR> formats;         // Surface image formats, e.g. RGBA and sizeof each color
    std::vector<VkPresentModeKHR> presentationModes; // How images should be presented to screen
};

struct SwapchainImage
{
    VkImage image;
    VkImageView imageView;
};

static std::vector<char> ReadFile(const std::string &fileName)
{
    // Open stream from given file
    // std::ios::binary tells stream to read file as binary
    // std::ios::ate tells stream start reading from end of file
    std::ifstream file(fileName, std::ios::binary | std::ios::ate);

    // Check if file stream successfully opened
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + fileName);
    }

    // Get current read position and use to resize file buffer
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> fileBuffer(fileSize);

    // Move read position to start of the file
    file.seekg(0);

    // Read file data into the buffer
    file.read(fileBuffer.data(), fileSize);

    // Close the file
    file.close();

    return fileBuffer;
}

static uint32_t FindMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
    // Get propeties of physical device memory
    VkPhysicalDeviceMemoryProperties memProperties = {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

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

static void CreateBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsageFlags,
                         VkMemoryPropertyFlags bufferProperties, VkBuffer *buffer, VkDeviceMemory *bufferMemory)
{
    // CREATE VERTEX BUFFER
    // Information to create a buffer
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;                       // Size of buffer (size of 1 vertex * number of vertices)
    bufferInfo.usage = bufferUsageFlags;                // Multiple types of buffer possible
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Similar to swapchain images, can share vertex buffers

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, buffer);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vertex Buffer");
    }

    // GET BUFFER MEMORY REQUIREMENTS
    VkMemoryRequirements memRequirements = {};
    vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

    // ALLOCATE MEMORY TO BUFFER
    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memRequirements.size;
    memAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(physicalDevice,
                                                       memRequirements.memoryTypeBits, // Index of memory type on Physical Device that has required bit flags
                                                       bufferProperties);              // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT    : CPU can interact with memory
                                                                                       // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT   : Allows placement of data straight into buffer after mapping (otherwise would have to specify manually)

    // Allocate memory to VkDeviceMemory
    result = vkAllocateMemory(device, &memAllocInfo, nullptr, bufferMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate device memory");
    }

    // Allocate memory to given vertex buffer
    vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static void CopyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
                       VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
    // Command buffer to hold transfer commands
    VkCommandBuffer transferCommandBuffer;

    // Command Buffer details
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = transferCommandPool;
    allocInfo.commandBufferCount = 1;

    // Allocate command buffer from pool
    vkAllocateCommandBuffers(device, &allocInfo, &transferCommandBuffer);

    // Information to begin the command buffer record
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // We're only using the command buffer once, so set up for one time submit

    // Begin recording transfer commands
    vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

    // Region of data to copy from and to
    VkBufferCopy bufferCopyRegion = {};
    bufferCopyRegion.srcOffset = 0;
    bufferCopyRegion.dstOffset = 0;
    bufferCopyRegion.size = bufferSize;

    // Command to copy src buffer to dst buffer
    vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

    vkEndCommandBuffer(transferCommandBuffer);

    // Queue submission information
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &transferCommandBuffer;


    // Submit transfer command to transfer queue and wait until it finishes
    vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(transferQueue);

    // Free temporary command buffer back to pool
    vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}
