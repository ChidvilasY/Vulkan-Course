#pragma once

#include <fstream>

#include <glm/glm.hpp>

constexpr int MAX_FRAME_DRAWS = 2;

const std::vector<const char*> gDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vertex data representation
struct Vertex
{
    glm::vec3 pos; // Vertex Position (x, y, z)
    glm::vec3 col; // Vertex Color (r, g, b)
};

// Indices (locations) of Queue Families (if they exist at all)
struct QueueFamilyIndices
{
    int graphicsFamily = -1;        // Locaiton of Graphics Queue Family
    int presentationFamily = -1;    // Location of Presentation Queue Family

    // Check if Queue families are valid
    bool IsValid()
    {
        return (graphicsFamily >= 0) && (presentationFamily >= 0);
    }
};

struct SwapChainDetails
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};     // Surface properties, e.g. image size/extent
    std::vector<VkSurfaceFormatKHR> formats;            // Surface image formats, e.g. RGBA and sizeof each color
    std::vector<VkPresentModeKHR> presentationModes;    // How images should be presented to screen
};

struct SwapchainImage {
    VkImage image;
    VkImageView imageView;
};

static std::vector<char> ReadFile(const std::string& fileName)
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