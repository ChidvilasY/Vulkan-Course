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

    float angle = 0.0f;
    double deltaTime = 0.0f;
    double lastTime = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        double now = glfwGetTime();
        deltaTime = now - lastTime;
        lastTime = now;

        angle += 10.0f * deltaTime;

        if (angle > 360.f)
        {
            angle = 0.f;
        }

        vulkanRenderer.UpdateModel(glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f)));

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
