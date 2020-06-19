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
    glm::vec2 tex; // Texture Coords (u, v)
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

static VkCommandBuffer BeginCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
    // Command buffer to hold transfer commands
    VkCommandBuffer commandBuffer;

    // Command Buffer details
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    // Allocate command buffer from pool
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    // Information to begin the command buffer record
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // We're only using the command buffer once, so set up for one time submit

    // Begin recording transfer commands
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

static void EndAndSubmitCommandBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    // Queue submission information
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Submit transfer command to transfer queue and wait until it finishes
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // Free temporary command buffer back to pool
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

static void CopyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
                       VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
    // Create buffer
    VkCommandBuffer transferCommandBuffer = BeginCommandBuffer(device, transferCommandPool);

    // Region of data to copy from and to
    VkBufferCopy bufferCopyRegion = {};
    bufferCopyRegion.srcOffset = 0;
    bufferCopyRegion.dstOffset = 0;
    bufferCopyRegion.size = bufferSize;

    // Command to copy src buffer to dst buffer
    vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

    // End and submit the buffer
    EndAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void CopyImageBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
                            VkBuffer srcBuffer, VkImage dstImage, uint32_t width, uint32_t height)
{
    // Create buffer
    VkCommandBuffer transferCommandBuffer = BeginCommandBuffer(device, transferCommandPool);

    VkBufferImageCopy imageRegion = {};
    imageRegion.bufferOffset = 0;                                        // Offset into data
    imageRegion.bufferRowLength = 0;                                     // Row length of data to calculate data spacing
    imageRegion.bufferImageHeight = 0;                                   // Image height to calculate data spacing
    imageRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Which aspect of image to copy
    imageRegion.imageSubresource.mipLevel = 0;                           // Mipmap level to copy
    imageRegion.imageSubresource.baseArrayLayer = 0;                     // Starting array layer(if any)
    imageRegion.imageSubresource.layerCount = 1;                         // No of layers to copy starting at baseArrayLayer
    imageRegion.imageOffset = {0, 0, 0};                                 // Offset into image (as opposed to raw data in buffer)
    imageRegion.imageExtent = {width, height, 1};                        // Size of region to copy as (x, y, z) values

    // Copy buffer to given image
    vkCmdCopyBufferToImage(transferCommandBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);

    // End and submit the buffer
    EndAndSubmitCommandBuffer(device, transferCommandPool, transferQueue, transferCommandBuffer);
}

static void TransitionImageLayout(VkDevice device, VkQueue queue, VkCommandPool commandPool, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    // Create buffer
    VkCommandBuffer commandBuffer = BeginCommandBuffer(device, commandPool);

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = currentLayout;                               // Layout to transition from
    imageMemoryBarrier.newLayout = newLayout;                                   // Layout to transition to
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;           // Queue family to transition from
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;           // Queue family to tranistion to
    imageMemoryBarrier.image = image;                                           // Image being accessed and modified as part of barrier
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Aspect of image being altered
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;                       // First mip level to start alteration on
    imageMemoryBarrier.subresourceRange.levelCount = 1;                         // Number of mip levels to alter starting from baseMipLevel
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;                     // First layer to start alterations on
    imageMemoryBarrier.subresourceRange.layerCount = 1;                         // Number of layer to alter starting from baseArrayLayer

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    // If transitioning from new image to image ready to recieve data...
    if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = 0;                            // Memory access stage tranition must after...
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // Memory access stage tranition must before.

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    // If transitioning from transfer destination to shader readable...
    else if (currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // Memory access stage tranition must after...
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;    // Memory access stage tranition must before.

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::runtime_error("Unimplemented");
    }


    vkCmdPipelineBarrier(
        commandBuffer,
        srcStage, dstStage,    // Pipeline stages (match to src and dst AccessMasks)
        0,                     // Dependency flags
        0, nullptr,            // Memory barrier count & data
        0, nullptr,            // Buffer memory barrier count & data
        1, &imageMemoryBarrier // Image memory barrier count & data
    );

    // End and submit the buffer
    EndAndSubmitCommandBuffer(device, commandPool, queue, commandBuffer);
}