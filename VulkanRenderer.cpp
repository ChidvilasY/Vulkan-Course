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
        CreateGraphicsPipeline();
        CreateFrameBuffers();
        CreateCommandPool();
        CreateCommandBuffers();
        RecordCommands();
        CreateSynchronization();
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

    // Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Information about particular subpass
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Pipeline type subpass is to be bound to
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentReference;

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

    // Create info for Render Pass
    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colorAttachment;
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

    // -- VERTEX INPUT (TODO: Put in vertex descriptions when resources created)--
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputCreateInfo.pVertexBindingDescriptions = nullptr; // List of Vertex Binding Descriptions (data spacing/stride info)
    vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
    vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr; // List of Vertex Attribute Descriptions (data format and where to bind to/from)

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
    rasterizerCreateInfo.depthClampEnable = VK_FALSE;         // Change if fragments beyond near/far planes are clipped (default) or clamped to plane
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;  // Whether to discard data and skip rasterizer. Never creates fragments, only suitable for pipeline without framebuffer output
    rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;  // How to handle filling points between vertices
    rasterizerCreateInfo.lineWidth = 1.0f;                    // How thick lines should be when drawn
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;    // Which face of a triangle to cull
    rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE; // Winding to determine which side is front
    rasterizerCreateInfo.depthBiasEnable = VK_FALSE;          // Whether to add depth bias to fragments (good for stopping "shadow acne" in shadown mapping)

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

    // -- PIPELINE LAYOUT (TODO: Apply Future Descriptor Set Layouts) --
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    // Create Pipeline Layout
    VkResult result = vkCreatePipelineLayout(m_mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Pipeline Layout");
    }

    // -- DEPTH STENCIL TESTING --
    // TODO: Set up depth stencil testing

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
    pipelineCreateInfo.pDepthStencilState = nullptr;
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
    std::cout << "Vendor ID: " << deviceProperties.vendorID << std::endl;

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

    return m_indices.IsValid() && extensionsSupported && swapChainValid;
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
        std::array<VkImageView, 1> attachments = {m_swapChainImages[i].imageView};

        VkFramebufferCreateInfo frameBufferCreateInfo = {};
        frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferCreateInfo.renderPass = m_renderPass; // Render pass layout the Framebuffer will be used with
        frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        frameBufferCreateInfo.pAttachments = attachments.data(); // List of attachments (1:1 with Render pass)
        frameBufferCreateInfo.width = m_swapChainExtent.width;   // Framebuffer width
        frameBufferCreateInfo.height = m_swapChainExtent.height; // Framebuffer height
        frameBufferCreateInfo.layers = 1;                        // Framebuffer layers

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

void VulkanRenderer::RecordCommands()
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
    VkClearValue clearValues[] = {{{0.6f, 0.65f, 0.4, 1.0f}}};
    renderPassBeginInfo.pClearValues = clearValues; // List of clear values (TODO: Depth Attachment Clear Value)
    renderPassBeginInfo.clearValueCount = 1;

    for (size_t i = 0; i < m_commandBuffers.size(); i++)
    {
        renderPassBeginInfo.framebuffer = m_swapChainFrameBuffers[i];

        // Start recording commands to command buffer
        VkResult result = vkBeginCommandBuffer(m_commandBuffers[i], &bufferBeignInfo);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to start recording a Command Buffer");
        }

        {
            // Begin Render Pass
            vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            {
                // Bind Pipeline to be used in render pass
                vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

                // Execute Pipepline
                vkCmdDraw(m_commandBuffers[i], 3, 1, 0, 0);
            }

            // End Rendere Pass
            vkCmdEndRenderPass(m_commandBuffers[i]);
        }

        // Stop reecording to command buffer
        result = vkEndCommandBuffer(m_commandBuffers[i]);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to start recording a Command Buffer");
        }
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
