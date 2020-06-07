#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>

#include "VulkanValidation.h"
#include "Utilities.h"
#include "Mesh.hpp"

class VulkanRenderer
{
public:
    VulkanRenderer();
    ~VulkanRenderer();

    int Init(GLFWwindow *newWindow);

    void UpdateModel(glm::mat4 newModel);

    void Draw();
    void CleanUP();

private:
    GLFWwindow *m_window = nullptr;

    int m_currentFrame = 0;

    // Scene Objects
    std::vector<Mesh> m_meshList;

    // Scene Settings
    struct MVP {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 model;
    } mvp;

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

    // - Descriptors
    VkDescriptorSetLayout m_descriptorSetLayout{};

    VkDescriptorPool m_descriptorPool{};
    std::vector<VkDescriptorSet> m_descriptorSets{};

    std::vector<VkBuffer> m_uniformBuffer{};
    std::vector<VkDeviceMemory> m_uniformBufferMemory{};

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

    // - Synchronization
    std::vector<VkSemaphore> m_imageAvailable{};
    std::vector<VkSemaphore> m_renderFinished{};
    std::vector<VkFence> m_drawFences{};

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
    void CreateDescriptorSetLayout();
    void CreateGraphicsPipeline();
    void CreateFrameBuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSynchronization();

    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();

    void UpdateUniformBuffer(uint32_t imageIndex);

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
