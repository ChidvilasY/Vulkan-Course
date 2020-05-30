#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <string>

#include "VulkanRenderer.h"

GLFWwindow *window;
VulkanRenderer vulkanRenderer;

void InitWindow(std::string wName = "Vulkan Window", const int width = 800, const int height = 600)
{
    // GLFW Init
    glfwInit();

    // Tell GLFW NOT to work with OpenGl
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // Create a window
    window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}

int main(int argc, char *argv[])
{
    // Create Window
    InitWindow();

    // Create Vulkan Renderer Instance
    if (vulkanRenderer.Init(window) == EXIT_FAILURE)
    {
        return EXIT_FAILURE;
    }

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        vulkanRenderer.Draw();
    }

    // Cleanup Vulkan
    vulkanRenderer.CleanUP();

    // Destroy window
    glfwDestroyWindow(window);

    // Terminate GLFW
    glfwTerminate();

    return 0;
}
