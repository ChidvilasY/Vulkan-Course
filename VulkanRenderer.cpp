#include <cstring>
#include <array>

#include "VulkanRenderer.h"

VulkanRenderer::VulkanRenderer()
{
}

VulkanRenderer::~VulkanRenderer()
{
}

int VulkanRenderer::Init(GLFWwindow *newWindow)
{
    m_window = newWindow;

    try
    {
        CreateInstance();
        CreateDebugCallback();
        CreateSurface();
        GetPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapChain();
        CreateRenderPass();
        CreateDescriptorSetLayout();
        CreatePushConstantRange();
        CreateGraphicsPipeline();
        CreateDepthBufferImage();
        CreateFrameBuffers();
        CreateCommandPool();
        CreateCommandBuffers();
        CreateTextureSampler();
        // AllocateDynamicBufferTransferSpace();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateSynchronization();

        {
            m_uboViewProjection.projection = glm::perspective(glm::radians(45.f), m_swapChainExtent.width / (float)m_swapChainExtent.height, 0.1f, 100.f);
            m_uboViewProjection.view = glm::lookAt(glm::vec3(10.f, 0.0f, 20.0f), glm::vec3(0.0f, 0.0f, -2.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            m_uboViewProjection.projection[1][1] *= -1; // Vulkan Considers Y-axis negative to

            // Create default no texture
            CreateTexture("plain.jpg");
        }
    }
    catch (std::runtime_error &e)
    {
        std::cout << __FILE__ << ":" << __LINE__ << ":" << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}

void VulkanRenderer::Draw()
{
    // -- GET NEXT IMAGE --
    // Wait for given fence to signal (open) from last draw before continuing
    vkWaitForFences(m_mainDevice.logicalDevice, 1, &m_drawFences[m_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
    // Manually reset (close) fences
    vkResetFences(m_mainDevice.logicalDevice, 1, &m_drawFences[m_currentFrame]);

    // Get index of next image to be drawn to, and signal semaphore when ready to be drawn to
    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_mainDevice.logicalDevice, m_swapchain, std::numeric_limits<uint64_t>::max(), m_imageAvailable[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    RecordCommands(imageIndex);
    UpdateUniformBuffers(imageIndex);

    // -- SUBMIT COMMAND BUFFER TO RENDER
    // Queue submission information
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;                              // Number of semaphores to wait on
    submitInfo.pWaitSemaphores = &m_imageAvailable[m_currentFrame]; // List of semaphores to wait on
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.pWaitDstStageMask = waitStages;                        // Stages to check semaphores at
    submitInfo.commandBufferCount = 1;                                // Number of command buffers to submit
    submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];       // Command buffer to submit
    submitInfo.signalSemaphoreCount = 1;                              // Number of semaphores to signal
    submitInfo.pSignalSemaphores = &m_renderFinished[m_currentFrame]; // Semaphores to signal when command buffer finishes

    // Submit command buffer to queue
    VkResult result = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_drawFences[m_currentFrame]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit Command Buffer to Queue");
    }

    // -- PRESENT RENDERED IMAGE TO SCREEN --
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;                              // Number of semaphores to wait on
    presentInfo.pWaitSemaphores = &m_renderFinished[m_currentFrame]; // Semaphores to wait on
    presentInfo.swapchainCount = 1;                                  // Number of swapchains to present to
    presentInfo.pSwapchains = &m_swapchain;                          // Swapchains to present images to
    presentInfo.pImageIndices = &imageIndex;                         // Index of images in Swapchains to present

    // Present image
    result = vkQueuePresentKHR(m_presentationQueue, &presentInfo);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present image");
    }

    // Get next frame (use % MAX_FRAME_DRAWS to keep value below MAX_FRAME_DRAWS)
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::CleanUP()
{
    // Wait until no actions being run on device before destoying
    vkDeviceWaitIdle(m_mainDevice.logicalDevice);

    // free(m_modelTransferSpace);

    for (auto &meshModel : m_meshModels)
    {
        meshModel.DestroyModel();
    }

    vkDestroyDescriptorPool(m_mainDevice.logicalDevice, m_samplerDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_mainDevice.logicalDevice, m_samplerSetLayout, nullptr);

    vkDestroySampler(m_mainDevice.logicalDevice, m_textureSampler, nullptr);
    m_textureSampler = nullptr;

    for (size_t i = 0; i < m_textureImages.size(); i++)
    {
        vkDestroyImageView(m_mainDevice.logicalDevice, m_textureImageViews[i], nullptr);
        m_textureImageViews[i] = nullptr;

        vkDestroyImage(m_mainDevice.logicalDevice, m_textureImages[i], nullptr);
        m_textureImages[i] = nullptr;

        vkFreeMemory(m_mainDevice.logicalDevice, m_textureImageMemorys[i], nullptr);
        m_textureImageMemorys[i] = nullptr;
    }

    vkDestroyImageView(m_mainDevice.logicalDevice, m_depthBufferImageView, nullptr);
    m_depthBufferImageView = nullptr;
    vkDestroyImage(m_mainDevice.logicalDevice, m_depthBufferImage, nullptr);
    m_depthBufferImage = nullptr;
    vkFreeMemory(m_mainDevice.logicalDevice, m_depthBufferImageMemory, nullptr);
    m_depthBufferImageMemory = nullptr;

    vkDestroyDescriptorPool(m_mainDevice.logicalDevice, m_descriptorPool, nullptr);
    m_descriptorPool = nullptr;

    vkDestroyDescriptorSetLayout(m_mainDevice.logicalDevice, m_descriptorSetLayout, nullptr);
    m_descriptorSetLayout = nullptr;

    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        vkDestroyBuffer(m_mainDevice.logicalDevice, m_vpUniformBuffer[i], nullptr);
        m_vpUniformBuffer[i] = nullptr;

        vkFreeMemory(m_mainDevice.logicalDevice, m_vpUniformBufferMemory[i], nullptr);
        m_vpUniformBufferMemory[i] = nullptr;

        // vkDestroyBuffer(m_mainDevice.logicalDevice, m_modelDynUniformBuffer[i], nullptr);
        // m_modelDynUniformBuffer[i] = nullptr;

        // vkFreeMemory(m_mainDevice.logicalDevice, m_modelDynUniformBufferMemory[i], nullptr);
        // m_modelDynUniformBufferMemory[i] = nullptr;
    }

    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
    {
        vkDestroySemaphore(m_mainDevice.logicalDevice, m_renderFinished[i], nullptr);
        m_renderFinished[i] = nullptr;

        vkDestroySemaphore(m_mainDevice.logicalDevice, m_imageAvailable[i], nullptr);
        m_imageAvailable[i] = nullptr;

        vkDestroyFence(m_mainDevice.logicalDevice, m_drawFences[i], nullptr);
        m_drawFences[i] = nullptr;
    }

    vkDestroyCommandPool(m_mainDevice.logicalDevice, m_graphicsCommandPool, nullptr);
    m_graphicsCommandPool = nullptr;

    for (auto &frameBuffer : m_swapChainFrameBuffers)
    {
        vkDestroyFramebuffer(m_mainDevice.logicalDevice, frameBuffer, nullptr);
        frameBuffer = nullptr;
    }

    vkDestroyPipeline(m_mainDevice.logicalDevice, m_graphicsPipeline, nullptr);
    m_graphicsPipeline = nullptr;

    vkDestroyPipelineLayout(m_mainDevice.logicalDevice, m_pipelineLayout, nullptr);
    m_pipelineLayout = nullptr;

    vkDestroyRenderPass(m_mainDevice.logicalDevice, m_renderPass, nullptr);
    m_renderPass = nullptr;

    for (auto image : m_swapChainImages)
    {
        vkDestroyImageView(m_mainDevice.logicalDevice, image.imageView, nullptr);
        image.imageView = 0;
    }

    vkDestroySwapchainKHR(m_mainDevice.logicalDevice, m_swapchain, nullptr);
    m_swapchain = 0;

    vkDestroySurfaceKHR(m_vkInstance, m_surface, nullptr);
    m_surface = 0;

    if (gEnableValidationLayers)
    {
        DestroyDebugReportCallbackEXT(m_vkInstance, m_callback, nullptr);
        DestroyDebugUtilsMessengerEXT(m_vkInstance, m_debugMessenger, nullptr);
        m_debugMessenger = 0;
        m_callback = 0;
    }

    vkDestroyDevice(m_mainDevice.logicalDevice, nullptr);
    m_mainDevice.logicalDevice = nullptr;

    vkDestroyInstance(m_vkInstance, nullptr);
    m_vkInstance = nullptr;
}

void VulkanRenderer::CreateInstance()
{
    // Checking if Validation layers are available
    if (gEnableValidationLayers && !CheckValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    // Information about application itself
    // Most data here doesn't affect the program and is for developer convenience
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan App";               // Custom name of the application
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // Custom version of application
    appInfo.pEngineName = "No Engine";                     // Custom Engine Name
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);      // Custom Engine Version
    appInfo.apiVersion = VK_API_VERSION_1_0;               // The Vulkan Version

    // Creation information for a VkInstance (Vulkan Instance)
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Create list to hold instance extensions
    std::vector<const char *> instanceExtensions = std::vector<const char *>();

    uint32_t glfwExtensionCount = 0; // GLFW may require multiple extensions
    const char **glfwExtensions;     // Extensions passed as array of cstrings, so need ptr to ptr

    // Get GLFW extensions
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Add GLFW extensions to list of extensions
    for (size_t i = 0; i < glfwExtensionCount; i++)
    {
        instanceExtensions.push_back(glfwExtensions[i]);
    }

    if (gEnableValidationLayers)
    {
        instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Checking if requested extensions are supported
    if (!CheckInstanceExtensionsSupport(instanceExtensions))
    {
        throw std::runtime_error("VkInstance does not support extensions!");
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMsngrCreateInfoExt;
    if (gEnableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(gValidationLayers.size());
        createInfo.ppEnabledLayerNames = gValidationLayers.data();

        // VK_EXT_debug_utils style
        debugUtilsMsngrCreateInfoExt.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsMsngrCreateInfoExt.pNext = NULL;
        debugUtilsMsngrCreateInfoExt.flags = 0;
        debugUtilsMsngrCreateInfoExt.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsMsngrCreateInfoExt.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsMsngrCreateInfoExt.pfnUserCallback = DebugMessengerContextCallBack;
        debugUtilsMsngrCreateInfoExt.pUserData = this;
        createInfo.pNext = &debugUtilsMsngrCreateInfoExt;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    // Create Instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_vkInstance);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Vulkan Instance!");
    }

    if (gEnableValidationLayers)
    {
        auto CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_vkInstance, "vkCreateDebugUtilsMessengerEXT");
        result = CreateDebugUtilsMessengerEXT(m_vkInstance, &debugUtilsMsngrCreateInfoExt, NULL, &m_debugMessenger);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create a Vulkan Instance!");
        }
    }
}

void VulkanRenderer::CreateLogicalDevice()
{
    // Vector for Queue Creation information
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    // Set for Family Indices
    std::set<int> queueFamilyIndices = {m_indices.graphicsFamily, m_indices.presentationFamily};

    for (const int queFamilyIndex : queueFamilyIndices)
    {
        // Queue the logical device needs to create and info to do so (Only 1 for now, will add more later)
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queFamilyIndex; // The index of the family to create a queue from
        queueCreateInfo.queueCount = 1;                    // No of queues to create
        float priority = 1.0f;
        queueCreateInfo.pQueuePriorities = &priority; // Vulkan needs to know how to handle multiple queues, so declare priority (1 - highest)

        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Information to create Logical Device (sometimes called "device")
    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());   // Number of Queue Create Infos
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();                             // List of queue create infos so device can create required
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(gDeviceExtensions.size()); // Number of Logical Device extensions
    deviceCreateInfo.ppEnabledExtensionNames = gDeviceExtensions.data();                      // List of enabled logical device extensions

    // Physical Device Features the logical device will be using
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE; // Enabling Anisotropy

    deviceCreateInfo.pEnabledFeatures = &deviceFeatures; // Physical Device features logical device will use

    VkResult result = vkCreateDevice(m_mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &m_mainDevice.logicalDevice);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Logical Device");
    }

    // Queues are created at the same time as the device...
    // So we want handle to queues
    // From given logical device, of given Queue Family, of given Queue Index (0 since only one queue), place reference in given VkQueue
    vkGetDeviceQueue(m_mainDevice.logicalDevice, m_indices.graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_mainDevice.logicalDevice, m_indices.presentationFamily, 0, &m_presentationQueue);
}

void VulkanRenderer::CreateSurface()
{
    // Create Surface (creates a surface create info struct, runs the create surface function, returns the result
    VkResult result = glfwCreateWindowSurface(m_vkInstance, m_window, nullptr, &m_surface);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Surface creation failed");
    }
}

void VulkanRenderer::CreateSwapChain()
{
    // Get Swap Chain details so we can pick best settings
    SwapChainDetails swapChainDetails = GetSwapChainDetails(m_mainDevice.physicalDevice);

    // Find optimal values for swap chain
    // Choose Best Surface Format
    VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(swapChainDetails.formats);

    // Choose Best Presentation Mode
    VkPresentModeKHR presentMode = ChooseBestPresentationMode(swapChainDetails.presentationModes);

    // Choose Swap Chain Image Resolution
    VkExtent2D imageExtent = ChooseSwapExtent(swapChainDetails.surfaceCapabilities);

    // How many images are in the swap chain? Get 1 more than the minimum to allow triple buffering
    uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

    // If imageCount is higher than max, clamp it to max
    // If 0, then limitless
    if (swapChainDetails.surfaceCapabilities.maxImageCount > 0 && swapChainDetails.surfaceCapabilities.maxImageCount < imageCount)
    {
        imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = m_surface;                                                  // Swapchain surface
    swapChainCreateInfo.imageFormat = surfaceFormat.format;                                   // Swapchain format
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;                           // Swapchain color space
    swapChainCreateInfo.presentMode = presentMode;                                            // Swapchain presentation mode
    swapChainCreateInfo.imageExtent = imageExtent;                                            // Swapchain image extents
    swapChainCreateInfo.minImageCount = imageCount;                                           // Minimum images in Swapchain
    swapChainCreateInfo.imageArrayLayers = 1;                                                 // Number of layers for each image in chain
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;                     // What attachment images will be used as
    swapChainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform; // Transform to perform on swap chain
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;                   // How to handle blending images with external graphics (e.g. other windows)
    swapChainCreateInfo.clipped = VK_TRUE;                                                    // Whether to clip parts of image not in view (e.g. behind another window, off screen, etc)

    // Queues to share between
    uint32_t queueFamilyIndices[] = {static_cast<uint32_t>(m_indices.graphicsFamily), static_cast<uint32_t>(m_indices.presentationFamily)};
    if (m_indices.graphicsFamily != m_indices.presentationFamily)
    {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // Image share handling
        swapChainCreateInfo.queueFamilyIndexCount = 2;                     // Number of queues to share images between
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;      // Array of queues to share between
    }
    else
    {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.queueFamilyIndexCount = 0;
        swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    }

    // If old swap chain been destroyed and this one replaces it, then link old one to quickly hand over responsibilities
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    // Create Swapchain
    VkResult result = vkCreateSwapchainKHR(m_mainDevice.logicalDevice, &swapChainCreateInfo, nullptr, &m_swapchain);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swap chain");
    }

    // Store for later references
    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = imageExtent;

    uint32_t swapChainImageCount;
    vkGetSwapchainImagesKHR(m_mainDevice.logicalDevice, m_swapchain, &swapChainImageCount, nullptr);
    std::vector<VkImage> images(swapChainImageCount);
    vkGetSwapchainImagesKHR(m_mainDevice.logicalDevice, m_swapchain, &swapChainImageCount, images.data());

    for (VkImage image : images)
    {
        // Store image handle
        SwapchainImage swapChainImage = {};
        swapChainImage.image = image;

        // Create Image View here
        swapChainImage.imageView = CreateImageView(image, m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        // Add to swapchain image list
        m_swapChainImages.push_back(swapChainImage);
    }
}

void VulkanRenderer::CreateRenderPass()
{
    // ATTACHMENTS
    // Color attachment of render pass
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = m_swapChainImageFormat;                   // Format to use for attachment
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;                   // No of samples to write for multisampling
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;              // Describes what to do with the attachment before rendering
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;            // Describes what to do with the attachment after rendering
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   // Describes what to do with stencil before rendering
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Describes what to do with stencil after rendering

    // Framebuffer data will be stored as an image, but images can be given different data layouts
    // to give optimal use for certain operations
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;     // Image data layout before render pass starts
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Image data layout after render pass (to change to)

    // Depth attachment of render pass
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = ChooseSupportedFormat({VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
                                                   VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // REFERENCES
    // Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment reference
    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Information about particular subpass
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Pipeline type subpass is to be bound to
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;
    subpass.pDepthStencilAttachment = &depthAttachmentReference;

    // Need to determine when the layer trasitions occur using subpass dependencies
    std::array<VkSubpassDependency, 2> subpassDependencies{};

    // Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    // Transition must happen after ...
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;                    // Subpass index (VK_SUBPASS_EXTERNAL = Special value meaning outside of renderpass)
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Pipeline stage
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;           // Stage access mask (memory access)

    // Transition must happen before ...
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = 0;

    // Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    // Transition must happen after ...
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Transition must happen before ...
    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[1].dependencyFlags = 0;

    std::array<VkAttachmentDescription, 2> renderPassAttachments = {colorAttachment, depthAttachment};

    // Create info for Render Pass
    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
    renderPassCreateInfo.pAttachments = renderPassAttachments.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassCreateInfo.pDependencies = subpassDependencies.data();

    VkResult result = vkCreateRenderPass(m_mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &m_renderPass);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Render Pass");
    }
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
    // UNIFORM DESCRIPTOR SET LAYTOUT
    // VP Binding info
    VkDescriptorSetLayoutBinding vpLayoutBinding = {};
    vpLayoutBinding.binding = 0;                                        // Binding point in shader (designated by binding number in shader)
    vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // Type of decriptor (uniform, dynamic uniform, image sampler, etc)
    vpLayoutBinding.descriptorCount = 1;                                // Number of descriptors for binding
    vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;            // Shader to stage to bind to
    vpLayoutBinding.pImmutableSamplers = nullptr;                       // For Texture: Can make sampler data unchangeable (immutable) by specifying in layout

    // // MODEL Binding info
    // VkDescriptorSetLayoutBinding modelLayoutBinding = {};
    // modelLayoutBinding.binding = 1;
    // modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    // modelLayoutBinding.descriptorCount = 1;
    // modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    // modelLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {vpLayoutBinding};

    // Create Descriptor Set Layout with given bindings
    VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size()); // Number of binding infos
    layoutCreateInfo.pBindings = layoutBindings.data();                           // Array of binding infos

    // Create Descriptor Set Layout
    VkResult result = vkCreateDescriptorSetLayout(m_mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &m_descriptorSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to Create a Descriptor Set Layout");
    }

    // SAMPLER DESCRIPTOR SET LAYTOUT
    // Texture Bindinig info
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    // Descriptor Set Layout with given binding for texture
    VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
    textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    textureLayoutCreateInfo.bindingCount = 1;
    textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

    // Create Descriptor Set Layout
    result = vkCreateDescriptorSetLayout(m_mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &m_samplerSetLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to Create Sampler Descriptor Set Layout");
    }
}

void VulkanRenderer::CreateGraphicsPipeline()
{
    // Read in SPIR-V code of shaders
    std::vector<char> vertShaderCode = ReadFile("Shaders/vert.spv");
    std::vector<char> fragShaderCode = ReadFile("Shaders/frag.spv");

    // Create Shader Modules
    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    // -- SHADER STAGE CREATION INFORMATION --
    // Vertex Stage creation information
    VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = {};
    vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // Shader Stage name
    vertexShaderCreateInfo.module = vertShaderModule;          // Shader module to be used by stage
    vertexShaderCreateInfo.pName = "main";                     // Entry point into shader

    // Fragment Stage creation information
    VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = {};
    fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; // Shader Stage name
    fragmentShaderCreateInfo.module = fragShaderModule;            // Shader module to be used by stage
    fragmentShaderCreateInfo.pName = "main";                       // Entry point into shader

    // Put shader stage creation info into an array
    // Graphics Pipeline creation info requires array of shader stage creates
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderCreateInfo, fragmentShaderCreateInfo};

    // How the data for a single vertex (including info such as position, color, texture coord, normals etc) is as a whole
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;                             // Can bind multiple streams of data
    bindingDescription.stride = sizeof(Vertex);                 // Size of single vertex object
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // How to move between data after each vertex
                                                                // VK_VERTEX_INPUT_RATE_VERTEX      : Move on to the next vertex
                                                                // VK_VERTEX_INPUT_RATE_INSTANCE    : Move to a vertex next instance
    // How the data for an attibute is defined within a vertex
    std::array<VkVertexInputAttributeDescription, 3> attributeDescription;

    // Position attribute
    attributeDescription[0].binding = 0;                         // Which binding the data is at (should be same as above)
    attributeDescription[0].location = 0;                        // Location in shader where data will be read from
    attributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT; // Format the data will take (also helps define size of data)
    attributeDescription[0].offset = offsetof(Vertex, pos);      // Where this attibute is defined in the data for a single vertex

    // Color attribute
    attributeDescription[1].binding = 0;                         // Which binding the data is at (should be same as above)
    attributeDescription[1].location = 1;                        // Location in shader where data will be read from
    attributeDescription[1].format = VK_FORMAT_R32G32B32_SFLOAT; // Format the data will take (also helps define size of data)
    attributeDescription[1].offset = offsetof(Vertex, col);      // Where this attibute is defined in the data for a single vertex

    // Texture attribute
    attributeDescription[2].binding = 0;                      // Which binding the data is at (should be same as above)
    attributeDescription[2].location = 2;                     // Location in shader where data will be read from
    attributeDescription[2].format = VK_FORMAT_R32G32_SFLOAT; // Format the data will take (also helps define size of data)
    attributeDescription[2].offset = offsetof(Vertex, tex);   // Where this attibute is defined in the data for a single vertex

    // -- VERTEX INPUT --
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;                                     // List of Vertex Binding Descriptions (data spacing/stride info)
    vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size()); //
    vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescription.data();                           // List of Vertex Attribute Descriptions (data format and where to bind to/from)

    // -- INPUT ASSEMBLY --
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // Primitive type to assemble into
    inputAssembly.primitiveRestartEnable = VK_FALSE;              // Allow overriding of "strip" topology to start new primitives

    // -- VIEWPORT & SCISSOR --
    // Create a viewport info struct
    VkViewport viewport = {};
    viewport.x = 0.0f;                                              // X - start coordinate
    viewport.y = 0.0f;                                              // Y - start coordinate
    viewport.width = static_cast<float>(m_swapChainExtent.width);   // Width of viewport
    viewport.height = static_cast<float>(m_swapChainExtent.height); // Height of viewport
    viewport.minDepth = 0.0f;                                       // min framebuffer depth
    viewport.maxDepth = 1.0f;                                       // max framebuffer depth

    // Crete a scissor info struct
    VkRect2D scissor = {};
    scissor.offset = {0, 0};            // Offset to use region from
    scissor.extent = m_swapChainExtent; // Extent to describe region to use, starting at offset

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

#if 0 // TODO: To resize view port
    // -- DYNAMIC STATES --
    // Dynamic states to enable
    std::vector<VkDynamicState> dynamicStateEnables;
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);   // Dynamic Viewport : Can resize in command buffer with vkCmdSetViewport(commandbuffer, 0, 1, &buffer);
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);    // Dynamic Scissor : Can resize in command buffer with vkCmdSetScissor(commandbuffer, 0, 1, &buffer);

    // Dynamic State creation info
    VkPipelineDynamicStateCreateInfo dynamicStateCreationInfo = {};
    dynamicStateCreationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreationInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
    dynamicStateCreationInfo.pDynamicStates = dynamicStateEnables.data();
#endif

    // -- RASTERIZER --
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
    rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.depthClampEnable = VK_FALSE;                 // Change if fragments beyond near/far planes are clipped (default) or clamped to plane
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;          // Whether to discard data and skip rasterizer. Never creates fragments, only suitable for pipeline without framebuffer output
    rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;          // How to handle filling points between vertices
    rasterizerCreateInfo.lineWidth = 1.0f;                            // How thick lines should be when drawn
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;            // Which face of a triangle to cull
    rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // Winding to determine which side is front
    rasterizerCreateInfo.depthBiasEnable = VK_FALSE;                  // Whether to add depth bias to fragments (good for stopping "shadow acne" in shadown mapping)

    // -- MULTISAMPLING --
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
    multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;               // Enable multisample shading or not
    multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // Number of samples to use per fragment

    // -- BLENDING --
    // Blending decides how to blend a new color being written to a fragment, with the old value
    VkPipelineColorBlendAttachmentState colorState = {};
    colorState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT |
                                VK_COLOR_COMPONENT_A_BIT; // Colors to apply blending to
    colorState.blendEnable = VK_TRUE;                     // Enable blending

    // Blending uses equation : (srcColorBlendFactor * new color) colorBlendOp (dstColorBlendFactor * old color)
    colorState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorState.colorBlendOp = VK_BLEND_OP_ADD;

    // Summarised: (VK_BLEND_FACTOR_SRC_ALPHA * new color) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * old color)
    //             (new color alpha * new color) + ((1 - new color alpha) - old color)

    colorState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorState.alphaBlendOp = VK_BLEND_OP_ADD;

    // Summarised: (1 * new alpha) + (0 * old alpha) = new alpha

    VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo = {};
    colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendingCreateInfo.logicOpEnable = VK_FALSE; // Alternative to calculations is to use logical operations
    colorBlendingCreateInfo.attachmentCount = 1;
    colorBlendingCreateInfo.pAttachments = &colorState;

    // -- PIPELINE LAYOUT --
    std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = {m_descriptorSetLayout, m_samplerSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &m_pushConstantRange;

    // Create Pipeline Layout
    VkResult result = vkCreatePipelineLayout(m_mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Pipeline Layout");
    }

    // -- DEPTH STENCIL TESTING --
    //
    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;           // Enable checking depth to deteremine fragment write
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;          // Enable writing to depth buffer (to replace old value)
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS; // Comparision operation that allows on overwrite (is in front)
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;    // Depth Bounds Test: Does the depth value exist between two values
    depthStencilCreateInfo.stencilTestEnable = VK_FALSE;        // Enable Stencil Test

    // -- GRAPHICS PIPELINE CREATION --
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;                             // Number of shader stages
    pipelineCreateInfo.pStages = shaderStages;                     // List of shader stages
    pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo; // All the fixed function pipeline states
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;
    pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendingCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
    pipelineCreateInfo.layout = m_pipelineLayout; // Pipeline Layout pipeline should use
    pipelineCreateInfo.renderPass = m_renderPass; // Render pass description the pipeline is compatible with
    pipelineCreateInfo.subpass = 0;               // Subpass of render pass to use with pipeline

    // Pipeline Derivatives: Can create multiple pipelines that derive from one another for optimisation
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // Existing pipeline to derive from
    pipelineCreateInfo.basePipelineIndex = -1;              // or index of pipeline being created to derive from (in case creating multiple at one)

    // Create Graphics Pipeline
    result = vkCreateGraphicsPipelines(m_mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_graphicsPipeline);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Graphics Pipeline");
    }

    // Destroy Shader Modules, no longer needed after Pipeline created
    vkDestroyShaderModule(m_mainDevice.logicalDevice, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_mainDevice.logicalDevice, vertShaderModule, nullptr);
}

void VulkanRenderer::GetPhysicalDevice()
{
    // Get no of available vulkan capable physical devices
    uint32_t physicalDevicesCount;
    vkEnumeratePhysicalDevices(m_vkInstance, &physicalDevicesCount, nullptr);

    if (physicalDevicesCount == 0)
    {
        throw std::runtime_error("Can't find GPUs that support Vulakn Instance");
    }

    // Get list of physical devices
    std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
    vkEnumeratePhysicalDevices(m_vkInstance, &physicalDevicesCount, physicalDevices.data());

    for (const auto &physicalDevice : physicalDevices)
    {
        if (CheckDeviceSuitable(physicalDevice))
        {
            m_mainDevice.physicalDevice = physicalDevice;
            break;
        }
    }

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_mainDevice.physicalDevice, &deviceProperties);

    // m_minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

bool VulkanRenderer::CheckInstanceExtensionsSupport(std::vector<const char *> checkExtensions)
{
    uint32_t extensionsCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, nullptr);

    // Create a list of VkExtensionProperties using count
    std::vector<VkExtensionProperties> extensions(extensionsCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionsCount, extensions.data());

    // Check if given extensions are in the list of available extensions
    for (const auto &checkExtension : checkExtensions)
    {
        bool hasExtension = false;

        for (const auto &extension : extensions)
        {
            if (strcmp(checkExtension, extension.extensionName))
            {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension)
        {
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::CheckDeviceSuitable(VkPhysicalDevice device)
{
    // Information about device itself (ID, type, name, vendor etc)
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    std::cout << "Vendor ID: "
              << "0x" << std::hex << deviceProperties.vendorID << " ";
    std::cout << "Device ID: "
              << "0x" << std::hex << deviceProperties.deviceID << " ";
    std::cout << deviceProperties.deviceName << std::endl;

    // Information about what the device can do (geo shader, tess shader, wide lines etc)
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    // Check if queues are supported
    m_indices = GetQueueFamilies(device);

    bool extensionsSupported = CheckDeviceExtensionsSupport(device);

    bool swapChainValid = false;
    if (extensionsSupported)
    {
        SwapChainDetails swapChainDetails = GetSwapChainDetails(device);
        swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
    }

    return m_indices.IsValid() && extensionsSupported && swapChainValid && deviceFeatures.samplerAnisotropy;
}

bool VulkanRenderer::CheckDeviceExtensionsSupport(VkPhysicalDevice device)
{
    // Get extensions count
    uint32_t extensionsCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, nullptr);

    if (extensionsCount == 0)
    {
        return false;
    }

    // Populate extensions
    std::vector<VkExtensionProperties> extensions(extensionsCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, extensions.data());

    for (const auto &deviceExtension : gDeviceExtensions)
    {
        bool hasExtension = false;
        for (const auto &extension : extensions)
        {
            if (strcmp(extension.extensionName, deviceExtension))
            {
                hasExtension = true;
                break;
            }
        }

        if (!hasExtension)
        {
            return false;
        }
    }

    return true;
}

bool VulkanRenderer::CheckValidationLayerSupport()
{
    // Get number of validation layers to create vector of appropriate size
    uint32_t validationLayerCount;
    vkEnumerateInstanceLayerProperties(&validationLayerCount, nullptr);

    // Check if no validation layers found AND we want at least 1 layer
    if (validationLayerCount == 0 && gValidationLayers.size() > 0)
    {
        return false;
    }

    std::vector<VkLayerProperties> availableLayers(validationLayerCount);
    vkEnumerateInstanceLayerProperties(&validationLayerCount, availableLayers.data());

    // Check if given Validation Layer is in list of given Validation Layers
    for (const auto &validationLayer : gValidationLayers)
    {
        bool hasLayer = false;
        for (const auto &availableLayer : availableLayers)
        {
            if (strcmp(validationLayer, availableLayer.layerName) == 0)
            {
                hasLayer = true;
                break;
            }
        }

        if (!hasLayer)
        {
            return false;
        }
    }

    return true;
}

QueueFamilyIndices VulkanRenderer::GetQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    // Get all Queue Family Property info for the given device
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

    // Go through each queue family and check if it has at least 1 of the required types of queue
    int i = 0;
    for (const auto &queueFamily : queueFamilyList)
    {
        // Check if Queue Family has at least 1 queue in that family (could have no queues)
        // Queue can be multiple types defined throught bitfield, need to bitwise AND with VK_QUEUE_*_BIT to check if has required type
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            // If Queue family is valid, then get index
            indices.graphicsFamily = i;
        }

        // Check if Queue Family supports presentation
        VkBool32 presentationSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentationSupport);
        // Check if queue is Presentation type (can be both GraphicsFamily and Presentation)
        if (queueFamily.queueCount > 0 && presentationSupport)
        {
            indices.presentationFamily = i;
        }

        if (indices.IsValid())
        {
            break;
        }

        i++;
    }

    return indices;
}

void VulkanRenderer::CreateDebugCallback()
{
    // Only create callback if validation enabled
    if (!gEnableValidationLayers)
        return;

    VkDebugReportCallbackCreateInfoEXT callbackCreateInfo = {};
    callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT; // Which validation reports should initiate callback
    callbackCreateInfo.pfnCallback = debugCallback;                                             // Pointer to callback function itself

    // Create debug callback with custom create function
    VkResult result = CreateDebugReportCallbackEXT(m_vkInstance, &callbackCreateInfo, nullptr, &m_callback);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Debug Callback!");
    }
}

SwapChainDetails VulkanRenderer::GetSwapChainDetails(VkPhysicalDevice device)
{
    SwapChainDetails swapChainDetails;

    // -- CAPABILITIES --
    // Get the surface capabilities for the given surface on the given physical device
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &swapChainDetails.surfaceCapabilities);

    // -- FORMATS --
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

    // If formats returned, get the list of formats
    if (formatCount != 0)
    {
        swapChainDetails.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, swapChainDetails.formats.data());
    }

    uint32_t presentationCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationCount, nullptr);

    // If presentation modes returned, get list of presentation modes
    if (presentationCount != 0)
    {
        swapChainDetails.presentationModes.resize(presentationCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentationCount, swapChainDetails.presentationModes.data());
    }

    return swapChainDetails;
}

// Best format is subjective, ours will be:
// Format       :   VK_FORMAT_R8G8B8A8_UNORM (VK_FORMAT_B8G8R8A8_UNORM as backup)
// ColorSpace   :   VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats)
{
    // If only 1 format availble and is undefined, then this means ALL formats are available
    if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    for (const auto &format : formats)
    {
        if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) &&
            (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
        {
            return format;
        }
    }

    return formats[0];
}

VkPresentModeKHR VulkanRenderer::ChooseBestPresentationMode(const std::vector<VkPresentModeKHR> &presentationModes)
{
    // Look for Mailbox presentation mode
    for (const auto &presentationMode : presentationModes)
    {
        if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return presentationMode;
        }
    }

    // If cant find, use fifo mode
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &surfaceCapabilities)
{
    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return surfaceCapabilities.currentExtent;
    }
    else
    {
        // If value can vary, need to set manually

        // Get window size
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);

        // Create new extent using window size
        VkExtent2D newExtent = {};
        newExtent.width = static_cast<uint32_t>(width);
        newExtent.height = static_cast<uint32_t>(height);

        // Surface also defines max and min, so make sure within boundaries by clamping value
        newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
        newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

        return newExtent;
    }
}

VkImageView VulkanRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewCreateInfo{};
    viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCreateInfo.image = image;                                // Image to create view for
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;             // Type of image (1D, 2D, 3D, Cube etc)
    viewCreateInfo.format = format;                              // Format of image data
    viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // Allows remaping of rgba components to other rgba values
    viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; // ""
    viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; // ""
    viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY; // ""

    // Subresources allow the view to view only
    viewCreateInfo.subresourceRange.aspectMask = aspectFlags; // Which aspect of image to view (e.g. COLOR_BIT for viewing color
    viewCreateInfo.subresourceRange.baseMipLevel = 0;         // Start mipmap level to view from
    viewCreateInfo.subresourceRange.levelCount = 1;           // Number of mipmap levels to view
    viewCreateInfo.subresourceRange.baseArrayLayer = 0;       // Start array level to view from
    viewCreateInfo.subresourceRange.layerCount = 1;           // Number of array levels to view

    // Create image view and return it
    VkImageView imageView;
    VkResult result = vkCreateImageView(m_mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image view");
    }

    return imageView;
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<char> &code)
{
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = code.size();
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(m_mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a shader module");
    }

    return shaderModule;
}

void VulkanRenderer::CreateFrameBuffers()
{
    // Resize framebuffer count to equal swap chain immage count
    m_swapChainFrameBuffers.resize(m_swapChainImages.size());

    // Create a framebuffer for each swap chain image
    for (size_t i = 0; i < m_swapChainFrameBuffers.size(); i++)
    {
        std::array<VkImageView, 2> attachments = {m_swapChainImages[i].imageView, m_depthBufferImageView};

        VkFramebufferCreateInfo frameBufferCreateInfo = {};
        frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCreateInfo.renderPass = m_renderPass;                                   // Render pass layout the Framebuffer will be used with
        frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size()); //
        frameBufferCreateInfo.pAttachments = attachments.data();                           // List of attachments (1:1 with Render pass)
        frameBufferCreateInfo.width = m_swapChainExtent.width;                             // Framebuffer width
        frameBufferCreateInfo.height = m_swapChainExtent.height;                           // Framebuffer height
        frameBufferCreateInfo.layers = 1;                                                  // Framebuffer layers

        VkResult result = vkCreateFramebuffer(m_mainDevice.logicalDevice, &frameBufferCreateInfo, nullptr, &m_swapChainFrameBuffers[i]);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Framebuffer");
        }
    }
}

void VulkanRenderer::CreateCommandPool()
{
    VkCommandPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = m_indices.graphicsFamily; // Queue Family type that buffers from this command pool will use

    // Create a Graphics Queue Family Command Pool
    VkResult result = vkCreateCommandPool(m_mainDevice.logicalDevice, &poolCreateInfo, nullptr, &m_graphicsCommandPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to Create Graphics Command Pool");
    }
}

void VulkanRenderer::CreateCommandBuffers()
{
    // Resize command buffer count to have one for each framebuffer
    m_commandBuffers.resize(m_swapChainFrameBuffers.size());

    VkCommandBufferAllocateInfo commandBuffAllocInfo = {};
    commandBuffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBuffAllocInfo.commandPool = m_graphicsCommandPool;
    commandBuffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // VK_COMMAND_BUFFER_LEVEL_PRIMARY : Buffer submitted directly to queue. Cant be called by other buffers.
                                                                  // VK_COMMAND_BUFFER_LEVEL_SECONDARY : Buffer cant be called directly, Can be called from other buffers via "vkCmdExecuteCommands" when recording commands in primary buffer
    commandBuffAllocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    // Allocate command buffers and place handles in array of buffers
    VkResult result = vkAllocateCommandBuffers(m_mainDevice.logicalDevice, &commandBuffAllocInfo, m_commandBuffers.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void VulkanRenderer::RecordCommands(uint32_t currentImage)
{
    // Information about how to begin each command buffer
    VkCommandBufferBeginInfo bufferBeignInfo = {};
    bufferBeignInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // Information about how to begin a render pass (only needed for graphical applications)
    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_renderPass;             // Render Pass to begin
    renderPassBeginInfo.renderArea.offset = {0, 0};            // Start point of render pass in pixels
    renderPassBeginInfo.renderArea.extent = m_swapChainExtent; // Size of region to run render pass on (starting at offset)

    std::array<VkClearValue, 2> clearValues = {};
    clearValues[0] = {{0.2f, 0.6f, 0.8, 1.0f}};
    clearValues[1].depthStencil.depth = 1.0f;

    renderPassBeginInfo.pClearValues = clearValues.data(); // List of clear values
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

    renderPassBeginInfo.framebuffer = m_swapChainFrameBuffers[currentImage];

    // Start recording commands to command buffer
    VkResult result = vkBeginCommandBuffer(m_commandBuffers[currentImage], &bufferBeignInfo);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to start recording a Command Buffer");
    }

    {
        // Begin Render Pass
        vkCmdBeginRenderPass(m_commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        {
            // Bind Pipeline to be used in render pass
            vkCmdBindPipeline(m_commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

            for (size_t j = 0; j < m_meshModels.size(); j++)
            {
                MeshModel thisModel = m_meshModels[j];

                // Push Constants to given shader stage directly (no buffer)
                vkCmdPushConstants(m_commandBuffers[currentImage], m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                                   0, sizeof(Model), thisModel.GetModelPtr());

                for (uint32_t k = 0; k < thisModel.GetMeshCount(); k++)
                {
                    VkBuffer vertexBuffers[] = {thisModel.GetMesh(k)->GetVertexBuffer()}; // Buffers to bind
                    VkDeviceSize offsets[] = {0};                                 // Offsests into buffers being bound

                    vkCmdBindVertexBuffers(m_commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);

                    // Bind mesh index buffer, with 0 offset and using the uint32_t type
                    vkCmdBindIndexBuffer(m_commandBuffers[currentImage], thisModel.GetMesh(k)->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                    // Dynamic Offset Amount
                    // uint32_t dynamicOffset = static_cast<uint32_t>(m_modelUniformAlignment) * j;

                    //
                    std::array<VkDescriptorSet, 2> descriptorSetGroup = {m_descriptorSets[currentImage], m_samplerDescriptorSets[thisModel.GetMesh(k)->GetTexId()]};

                    // Bind Descriptor Sets
                    vkCmdBindDescriptorSets(m_commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                                            0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

                    // Execute Pipepline
                    vkCmdDrawIndexed(m_commandBuffers[currentImage], thisModel.GetMesh(k)->GetIndexCount(), 1, 0, 0, 0);
                }
            }
        }

        // End Rendere Pass
        vkCmdEndRenderPass(m_commandBuffers[currentImage]);
    }

    // Stop recording to command buffer
    result = vkEndCommandBuffer(m_commandBuffers[currentImage]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to start recording a Command Buffer");
    }
}

void VulkanRenderer::CreateSynchronization()
{
    m_imageAvailable.resize(MAX_FRAME_DRAWS);
    m_renderFinished.resize(MAX_FRAME_DRAWS);
    m_drawFences.resize(MAX_FRAME_DRAWS);

    // Semaphore creation information
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
    {
        if (vkCreateSemaphore(m_mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &m_imageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &m_renderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(m_mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &m_drawFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create a semaphore and/or fence");
        }
    }
}

void VulkanRenderer::CreateUniformBuffers()
{
    // View Projection Buffer size
    VkDeviceSize vpBufferSize = sizeof(UBOViewProjection);

    // Model buffer size
    // VkDeviceSize modelBufferSize = m_modelUniformAlignment * MAX_OBJECTS;

    // One uniform buffer for each image (and by extension, command buffer)
    m_vpUniformBuffer.resize(m_swapChainImages.size());
    m_vpUniformBufferMemory.resize(m_swapChainImages.size());
    // m_modelDynUniformBuffer.resize(m_swapChainImages.size());
    // m_modelDynUniformBufferMemory.resize(m_swapChainImages.size());

    // Create Uniform Buffers
    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        CreateBuffer(m_mainDevice.physicalDevice, m_mainDevice.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_vpUniformBuffer[i], &m_vpUniformBufferMemory[i]);

        // CreateBuffer(m_mainDevice.physicalDevice, m_mainDevice.logicalDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        //              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &m_modelDynUniformBuffer[i], &m_modelDynUniformBufferMemory[i]);
    }
}

void VulkanRenderer::CreateDescriptorPool()
{
    // CREATE UNIFORM DESCRIPTOR POOL
    // Type of decriptors + how many DESCRIPTORS, not Descriptor Sets (combined makes the pool size)
    VkDescriptorPoolSize vpPoolSize = {};
    vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    vpPoolSize.descriptorCount = static_cast<uint32_t>(m_vpUniformBuffer.size());

    // // Model Pool (DYNAMIC)
    // VkDescriptorPoolSize modelPoolSize = {};
    // modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    // modelPoolSize.descriptorCount = static_cast<uint32_t>(m_modelDynUniformBuffer.size());

    std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {vpPoolSize};

    // Data to create Descriptor Pool
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = static_cast<uint32_t>(m_swapChainImages.size());         // Max no of Descriptor Sets that can be created from pool
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size()); // Amount of Pool Sizes being passed
    descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();                           // Pool Sizes to create pool with

    // Create Descriptor Pool
    VkResult result = vkCreateDescriptorPool(m_mainDevice.logicalDevice, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Descriptor Pool");
    }

    // CREATE SAMPLER DESCRIPTOR POOL
    // Texture Sampler Pool
    VkDescriptorPoolSize samplerPoolSize = {};
    samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerPoolSize.descriptorCount = MAX_OBJECTS;

    VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
    samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
    samplerPoolCreateInfo.poolSizeCount = 1;
    samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

    result = vkCreateDescriptorPool(m_mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &m_samplerDescriptorPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Sampler Descriptor Pool");
    }
}

void VulkanRenderer::CreateDescriptorSets()
{
    // Resize Descriptor Set list so one for every buffer
    m_descriptorSets.resize(m_swapChainImages.size());

    std::vector<VkDescriptorSetLayout> setLayouts(m_swapChainImages.size(), m_descriptorSetLayout);

    // Descriptor Set Allocation Info
    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = m_descriptorPool;                                    // Pool to allocate descriptor set from
    setAllocInfo.descriptorSetCount = static_cast<uint32_t>(m_swapChainImages.size()); // No of sets to allocate
    setAllocInfo.pSetLayouts = setLayouts.data();                                      // Layouts to use to allocate sets (1:1 relationship)

    // Allocate Descriptor Sets (multiple)
    VkResult result = vkAllocateDescriptorSets(m_mainDevice.logicalDevice, &setAllocInfo, m_descriptorSets.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Descriptor Sets");
    }

    // Update all of descriptor set buffer bindings
    for (size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        // VIEW PROJECTION DESCRIPTOR
        // Buffer info and offset info
        VkDescriptorBufferInfo vpBufferInfo = {};
        vpBufferInfo.buffer = m_vpUniformBuffer[i];     // Buffer to get data from
        vpBufferInfo.offset = 0;                        // Position of start of data
        vpBufferInfo.range = sizeof(UBOViewProjection); // Size of data

        // Data about connection between binding and buffer
        VkWriteDescriptorSet vpSetWrite = {};
        vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vpSetWrite.dstSet = m_descriptorSets[i];                       // Descriptor Set to update
        vpSetWrite.dstBinding = 0;                                     // Binding to update (matches with binding on layout/shader)
        vpSetWrite.dstArrayElement = 0;                                // Index in array to update
        vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // Type of descriptor
        vpSetWrite.descriptorCount = 1;                                // Amount to update
        vpSetWrite.pBufferInfo = &vpBufferInfo;                        // Information about buffer data to bind

        // // MODEL DESCRIPTOR
        // // Model Buffer Binding info
        // VkDescriptorBufferInfo modelBufferInfo = {};
        // modelBufferInfo.buffer = m_modelDynUniformBuffer[i];
        // modelBufferInfo.offset = 0;
        // modelBufferInfo.range = m_modelUniformAlignment;

        // VkWriteDescriptorSet modelSetWrite = {};
        // modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        // modelSetWrite.dstSet = m_descriptorSets[i];
        // modelSetWrite.dstBinding = 1;
        // modelSetWrite.dstArrayElement = 0;
        // modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        // modelSetWrite.descriptorCount = 1;
        // modelSetWrite.pBufferInfo = &modelBufferInfo;

        // List of Descriptor Set Writes
        std::vector<VkWriteDescriptorSet> setWrites = {vpSetWrite};

        // Update the descirptor sets with new buffer/binding info
        vkUpdateDescriptorSets(m_mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
    }
}

void VulkanRenderer::UpdateUniformBuffers(uint32_t imageIndex)
{
    // Copy VP data
    void *data;
    vkMapMemory(m_mainDevice.logicalDevice, m_vpUniformBufferMemory[imageIndex], 0, sizeof(UBOViewProjection), 0, &data);
    memcpy(data, &m_uboViewProjection, sizeof(UBOViewProjection));
    vkUnmapMemory(m_mainDevice.logicalDevice, m_vpUniformBufferMemory[imageIndex]);

#if 0 // Dynamic Uniform Buffer, Using Push constants instead
    // Copy Model data
    for (size_t i = 0; i < m_meshList.size(); i++)
    {
        Model *thisModel = (Model *)((uint64_t)m_modelTransferSpace + (i * m_modelUniformAlignment));
        *thisModel = m_meshList[i].GetModel();
    }

    // Map the list of model data
    vkMapMemory(m_mainDevice.logicalDevice, m_modelDynUniformBufferMemory[imageIndex], 0, m_modelUniformAlignment * m_meshList.size(), 0, &data);
    memcpy(data, m_modelTransferSpace, m_modelUniformAlignment * m_meshList.size());
    vkUnmapMemory(m_mainDevice.logicalDevice, m_modelDynUniformBufferMemory[imageIndex]);
#endif
}

void VulkanRenderer::UpdateModel(size_t modelID, glm::mat4 newModel)
{
    if (modelID < m_meshModels.size())
    {
        m_meshModels[modelID].SetModel(newModel);
    }
}

void VulkanRenderer::AllocateDynamicBufferTransferSpace()
{
#if 0 // Dynamic Uniform Buffers are not used anymore, using push constants instead
    // Calculate allignment of model data
    m_modelUniformAlignment = (sizeof(Model) + m_minUniformBufferOffset - 1) & ~(m_minUniformBufferOffset - 1);

    // Create space in memory to hold dynamic buffer that is aligned to our required alignement and hold MAX_OBJECTS
    m_modelTransferSpace = (Model *)aligned_alloc(m_modelUniformAlignment, m_modelUniformAlignment * MAX_OBJECTS);
#endif
}

void VulkanRenderer::CreatePushConstantRange()
{
    // Define Push Constant values (no 'create' needed!)
    m_pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Shader stage push constant will go to
    m_pushConstantRange.offset = 0;                              // Offset into given data to push constant
    m_pushConstantRange.size = sizeof(Model);                    // Size of data being passed
}

void VulkanRenderer::CreateDepthBufferImage()
{
    // Get supported format for depth buffer
    m_depthFormat = ChooseSupportedFormat({VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
                                          VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

    // Create Depth Buffer Image
    m_depthBufferImage = CreateImage(m_swapChainExtent.width, m_swapChainExtent.height, m_depthFormat, VK_IMAGE_TILING_OPTIMAL,
                                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &m_depthBufferImageMemory);

    m_depthBufferImageView = CreateImageView(m_depthBufferImage, m_depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkImage VulkanRenderer::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                    VkImageUsageFlags usageFlags, VkMemoryPropertyFlags propFlags,
                                    VkDeviceMemory *imageMemory)
{
    // CREATE IMAGE
    // Image Create Info
    VkImageCreateInfo imageCreateInfo = {};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;              // Type of Image (1D, 2D or 3D)
    imageCreateInfo.extent.width = width;                      // Width of Image extent
    imageCreateInfo.extent.height = height;                    // Height of Image extent
    imageCreateInfo.extent.depth = 1;                          // Depth of Image (just 1, no 3D aspect)
    imageCreateInfo.mipLevels = 1;                             // No of mipmap levels
    imageCreateInfo.arrayLayers = 1;                           //No of levels in large array
    imageCreateInfo.format = format;                           // Format type of image
    imageCreateInfo.tiling = tiling;                           // How large data should be "tiled" (arranged for optimal reading speed)
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Layout for imgage data on creation
    imageCreateInfo.usage = usageFlags;                        // Bit flags defining what image will be used for
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;           // No of samples for multi-sampling
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;   // Whether the image can be shared between queues

    // CREATE MEMORY FOR IMAGE
    VkImage image;
    VkResult result = vkCreateImage(m_mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create an image");
    }

    // CREATE MEMORY FOR IMAGE

    // Get memory requirements for a type of image
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(m_mainDevice.logicalDevice, image, &memoryRequirements);

    VkMemoryAllocateInfo memoryAllocInfo = {};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memoryRequirements.size;
    memoryAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(m_mainDevice.physicalDevice, memoryRequirements.memoryTypeBits, propFlags);

    result = vkAllocateMemory(m_mainDevice.logicalDevice, &memoryAllocInfo, nullptr, imageMemory);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate memory for image");
    }

    // Connect memory to image
    vkBindImageMemory(m_mainDevice.logicalDevice, image, *imageMemory, 0);

    return image;
}

VkFormat VulkanRenderer::ChooseSupportedFormat(const std::vector<VkFormat> &formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
    // Loop through options and find compatible one
    for (VkFormat format : formats)
    {
        // Get properties for given format on this device
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(m_mainDevice.physicalDevice, format, &properties);

        // Depending on tiling choice, need to check for different feature flags
        if ((tiling == VK_IMAGE_TILING_LINEAR) && (properties.linearTilingFeatures & featureFlags))
        {
            return format;
        }
        else if ((tiling == VK_IMAGE_TILING_OPTIMAL) && (properties.optimalTilingFeatures & featureFlags))
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find a matching format");
}

stbi_uc *VulkanRenderer::LoadTexture(const std::string filename, int *width, int *height, VkDeviceSize *imageSize)
{
    // Number of channels image uses
    int channels;

    // Load pixel data for image
    const std::string fileLoc = "Textures/" + filename;
    stbi_uc *image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);
    if (image == nullptr)
    {
        throw std::runtime_error("Failed to load a Texture file! (" + filename + ")");
    }

    // Calculate image size using given and known data
    *imageSize = *width * *height * 4;

    return image;
}

int VulkanRenderer::CreateTextureImage(const std::string fileName)
{
    // Load image file
    int width, height;
    VkDeviceSize imageSize;

    stbi_uc *imageData = LoadTexture(fileName, &width, &height, &imageSize);

    // Create staging buffer to hold loaded data, ready to copy to device
    VkBuffer imageStagingBuffer;
    VkDeviceMemory imageStagingBufferMemory;
    CreateBuffer(m_mainDevice.physicalDevice, m_mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &imageStagingBuffer, &imageStagingBufferMemory);

    // Copy image data to staging buffer
    void *data;
    vkMapMemory(m_mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, imageData, static_cast<size_t>(imageSize));
    vkUnmapMemory(m_mainDevice.logicalDevice, imageStagingBufferMemory);

    // Free original image data
    stbi_image_free(imageData);

    // Create image to hold final texture
    VkImage texImage;
    VkDeviceMemory texImageMemory;
    texImage = CreateImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texImageMemory);

    // COPY DATA TO IMAGE
    // Transition image to be DST for copy operation
    TransitionImageLayout(m_mainDevice.logicalDevice, m_graphicsQueue, m_graphicsCommandPool, texImage,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy image data
    CopyImageBuffer(m_mainDevice.logicalDevice, m_graphicsQueue, m_graphicsCommandPool, imageStagingBuffer, texImage, width, height);

    TransitionImageLayout(m_mainDevice.logicalDevice, m_graphicsQueue, m_graphicsCommandPool, texImage,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Add texture data to vector for reference
    m_textureImages.push_back(texImage);
    m_textureImageMemorys.push_back(texImageMemory);

    // Destroy staging buffers
    vkDestroyBuffer(m_mainDevice.logicalDevice, imageStagingBuffer, nullptr);
    vkFreeMemory(m_mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

    // Return index of new texture image
    return m_textureImages.size() - 1;
}

int VulkanRenderer::CreateTexture(const std::string fileName)
{
    // Create Texture image and get its location in array
    int textureImageLoc = CreateTextureImage(fileName);

    // Create Image view and add to list
    VkImageView imageView = CreateImageView(m_textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
    m_textureImageViews.push_back(imageView);

    // Create Descriptor Set Here
    int descriptorLoc = CreateTextureDescriptor(imageView);

    return descriptorLoc;
}

void VulkanRenderer::CreateTextureSampler()
{
    // Sampler Creation Info
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;                   // How to render when image is magnified on screen
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;                   // How to render when image is minified on screen
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // How to handle texture wrap in U (x) direction
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // How to handle texture wrap in V (y) direction
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // How to handle texture wrap in W (z) direction
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // Border beyond texture, only works for border clamp
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;             // Whether the coords should be normalized between 0 and 1
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;     // Mipmap interpolation mode
    samplerCreateInfo.mipLodBias = 0.0f;                              // Level of detail bias for mip level
    samplerCreateInfo.minLod = 0.0f;                                  // Minimum level of detail to pick mip level
    samplerCreateInfo.maxLod = 0.0f;                                  // Maximum level of detail to pick mip level
    samplerCreateInfo.anisotropyEnable = VK_TRUE;                     // Enable Anisotropic filtering
    samplerCreateInfo.maxAnisotropy = 16.0f;                          // Maximum Anisotroy sample level

    VkResult result = vkCreateSampler(m_mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &m_textureSampler);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create a Texture Sampler");
    }
}

int VulkanRenderer::CreateTextureDescriptor(VkImageView textureImageView)
{
    VkDescriptorSet descriptorSet;

    VkDescriptorSetAllocateInfo setAllocInfo = {};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = m_samplerDescriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &m_samplerSetLayout;

    VkResult result = vkAllocateDescriptorSets(m_mainDevice.logicalDevice, &setAllocInfo, &descriptorSet);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Texture Descriptor Sets");
    }

    // Texture Image info
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Image layout when in use
    imageInfo.imageView = textureImageView;                           // Image to bind to set
    imageInfo.sampler = m_textureSampler;                             // Sampler to use for set

    // Descriptor Write Info
    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    // Update new Descriptor Set
    vkUpdateDescriptorSets(m_mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

    // Add Descriptor set to list
    m_samplerDescriptorSets.push_back(descriptorSet);

    // Return descriptor set location
    return m_samplerDescriptorSets.size() - 1;
}

int VulkanRenderer::CreateMeshModel(const std::string modelFileName)
{
    // Import Model "scene"
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(modelFileName, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);
    if (scene == nullptr)
    {
        throw std::runtime_error("Failed to load (" + modelFileName + ")");
    }

    // Get vector of all materials with 1:1 ID placement
    std::vector<std::string> textureNames = MeshModel::LoadMaterials(scene);

    // Conversion from the materials list IDs to our Descriptor Array IDs
    std::vector<int> matToTex(textureNames.size());

    // Loop over texture names and create textures for them
    for (size_t i = 0; i < textureNames.size(); i++)
    {
        // If material had no texture, set '0' to indicate no texture, texture 0 will be reserved for a default texture
        if (textureNames[i].empty())
        {
            matToTex[i] = 0;
        }
        // Otherwise, create texture and set value to index of texture
        else
        {
            matToTex[i] = CreateTexture(textureNames[i]);
        }
    }

    // Load in all meshes
    std::vector<Mesh> modelMeshes = MeshModel::LoadModel(m_mainDevice.physicalDevice, m_mainDevice.logicalDevice, m_graphicsQueue, m_graphicsCommandPool,
                                                         scene->mRootNode, scene, matToTex);

    // Create Mesh Model and add it list
    MeshModel meshModel = MeshModel(modelMeshes);
    m_meshModels.push_back(meshModel);

    return m_meshModels.size() - 1;
}
