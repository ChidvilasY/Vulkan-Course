#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>

#include "VulkanValidation.h"
#include "Utilities.h"

class VulkanRenderer
{
public:
    VulkanRenderer();
    ~VulkanRenderer();

    int Init(GLFWwindow *newWindow);
    void CleanUP();

private:
    GLFWwindow *m_window = nullptr;

    // Vulkan Components
    // - Main
    VkInstance m_vkInstance = nullptr;

    struct
    {
        VkPhysicalDevice physicalDevice = nullptr;
        VkDevice logicalDevice = nullptr;
    } m_mainDevice{};

    VkQueue m_graphicsQueue = nullptr;
    VkQueue m_presentationQueue = nullptr;

    VkSurfaceKHR m_surface{};
    VkSwapchainKHR m_swapchain{};
    std::vector<SwapchainImage> m_swapChainImages{};
    std::vector<VkFramebuffer> m_swapChainFrameBuffers{};
    std::vector<VkCommandBuffer> m_commandBuffers{};

    // - Pipeline
    VkPipeline m_graphicsPipeline;
    VkPipelineLayout m_pipelineLayout{};
    VkRenderPass m_renderPass{};

    // - Pools
    VkCommandPool m_graphicsCommandPool{};

    // - Utility
    QueueFamilyIndices m_indices{};
    VkFormat m_swapChainImageFormat{};
    VkExtent2D m_swapChainExtent{};

    // - Validation
    VkDebugReportCallbackEXT m_callback{};
    VkDebugUtilsMessengerEXT m_debugMessenger{};

    // Vulkan Functions
    // - Create Functions
    void CreateInstance();
    void CreateLogicalDevice();
    void CreateDebugCallback();
    void CreateSurface();
    void CreateSwapChain();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    void CreateFrameBuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();

    // - Record Functions
    void RecordCommands();

    // - Get Functions
    void GetPhysicalDevice();

    // - Support Functions
    // -- Checker Functions
    bool CheckInstanceExtensionsSupport(std::vector<const char *> checkExtensions);
    bool CheckDeviceSuitable(VkPhysicalDevice device);
    bool CheckDeviceExtensionsSupport(VkPhysicalDevice device);
    bool CheckValidationLayerSupport();

    // -- Getter Functions
    QueueFamilyIndices GetQueueFamilies(VkPhysicalDevice device);
    SwapChainDetails GetSwapChainDetails(VkPhysicalDevice device);

    // -- Choose Functions
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
    VkPresentModeKHR ChooseBestPresentationMode(const std::vector<VkPresentModeKHR> &presentationModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &surfaceCapabilities);

    // -- Create Functions
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkShaderModule CreateShaderModule(const std::vector<char> &code);
};
