#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>

#include "stb_image.h"

#include "VulkanValidation.h"
#include "Utilities.h"
#include "Mesh.hpp"
#include "MeshModel.hpp"

class VulkanRenderer
{
public:
    VulkanRenderer();
    ~VulkanRenderer();

    int Init(GLFWwindow *newWindow);
    int CreateMeshModel(const std::string modelFileName);
    void UpdateModel(size_t modelID, glm::mat4 newModel);

    void Draw();
    void CleanUP();

private:
    GLFWwindow *m_window = nullptr;

    int m_currentFrame = 0;

    // Scene Objects
    std::vector<MeshModel> m_meshModels{};

    // Scene Settings
    struct UBOViewProjection
    {
        glm::mat4 projection;
        glm::mat4 view;
    } m_uboViewProjection;

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

    std::vector<VkImage> m_depthBufferImage{};
    std::vector<VkDeviceMemory> m_depthBufferImageMemory{};
    std::vector<VkImageView> m_depthBufferImageView{};

    std::vector<VkImage> m_colorBufferImage{};
    std::vector<VkDeviceMemory> m_colorBufferImageMemory{};
    std::vector<VkImageView> m_colorBufferImageView{};

    VkSampler m_textureSampler{};

    // - Descriptors
    VkDescriptorSetLayout m_descriptorSetLayout{};
    VkDescriptorSetLayout m_samplerSetLayout{};
    VkDescriptorSetLayout m_inputSetLayout{};
    VkPushConstantRange m_pushConstantRange{};

    VkDescriptorPool m_descriptorPool{};
    VkDescriptorPool m_samplerDescriptorPool{};
    VkDescriptorPool m_inputDescriptorPool{};
    std::vector<VkDescriptorSet> m_descriptorSets{};
    std::vector<VkDescriptorSet> m_samplerDescriptorSets{};
    std::vector<VkDescriptorSet> m_inputDescriptorSets{};

    std::vector<VkBuffer> m_vpUniformBuffer{};
    std::vector<VkDeviceMemory> m_vpUniformBufferMemory{};

    std::vector<VkBuffer> m_modelDynUniformBuffer{};
    std::vector<VkDeviceMemory> m_modelDynUniformBufferMemory{};

    // VkDeviceSize m_minUniformBufferOffset{};
    // size_t m_modelUniformAlignment{};
    // Model *m_modelTransferSpace{}; Using push constants instead

    // - Assets
    std::vector<VkImage> m_textureImages{};
    std::vector<VkDeviceMemory> m_textureImageMemorys{};
    std::vector<VkImageView> m_textureImageViews{};

    // - Pipeline
    VkPipeline m_graphicsPipeline{};
    VkPipelineLayout m_pipelineLayout{};

    VkPipeline m_secondPipeline{};
    VkPipelineLayout m_secondPipelineLayout{};

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
    void CreatePushConstantRange();
    void CreateGraphicsPipeline();
    void CreateColorBufferImage();
    void CreateDepthBufferImage();
    void CreateFrameBuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSynchronization();
    void CreateTextureSampler();

    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateInputDescriptorSets();

    void UpdateUniformBuffers(uint32_t imageIndex);

    // - Record Functions
    void RecordCommands(uint32_t currentImage);

    // - Get Functions
    void GetPhysicalDevice();

    // - Allocate Functions
    void AllocateDynamicBufferTransferSpace();

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
    VkFormat ChooseSupportedFormat(const std::vector<VkFormat> &formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags);

    // -- Create Functions
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    VkShaderModule CreateShaderModule(const std::vector<char> &code);
    VkImage CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                        VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propFlags,
                        VkDeviceMemory *imageMemory);

    int CreateTextureImage(const std::string fileName);
    int CreateTexture(const std::string fileName);
    int CreateTextureDescriptor(VkImageView textureImageView);

    // -- Loader Functions
    stbi_uc *LoadTexture(const std::string fileName, int *width, int *height, VkDeviceSize *imageSize);
};
