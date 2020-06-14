#define GLM_FORCE_DEPTH_ZERO_TO_ONE

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

        glm::mat4 firstModel(1.f);
        firstModel = glm::translate(firstModel, glm::vec3(0.f, 1.f, -3.f));
        firstModel = glm::rotate(firstModel, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
        vulkanRenderer.UpdateModel(0, firstModel);

        glm::mat4 secondModel(1.f);
        secondModel = glm::translate(secondModel, glm::vec3(0.f, 0.f, sinf(glm::radians(angle * 5)) * 2.f - 4.f));
        secondModel = glm::rotate(secondModel, glm::radians(-angle * 5), glm::vec3(0.0f, 0.0f, 1.0f));
        vulkanRenderer.UpdateModel(1, secondModel);

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
