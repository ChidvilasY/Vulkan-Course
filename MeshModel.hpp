#pragma once

#include <vector>
#include <cstdint>

#include <glm/glm.hpp>

#include "Mesh.hpp"

class MeshModel
{
public:
    MeshModel();
    MeshModel(std::vector<Mesh> &newMeshList);

    uint32_t GetMeshCount();
    Mesh *GetMesh(size_t index);

    glm::mat4 GetModel();
    const glm::mat4 *GetModelPtr();
    void SetModel(const glm::mat4 &newModel);

    void DestroyModel();

    static std::vector<std::string> LoadMaterials(const aiScene *scene);
    static std::vector<Mesh> LoadModel(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
                                       aiNode *node, const aiScene *scene, std::vector<int> &matToTex);
    static Mesh LoadMesh(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
                         aiMesh *mesh, const aiScene *scene, std::vector<int> &matToTex);

    ~MeshModel();

private:
    std::vector<Mesh> m_meshList;
    glm::mat4 m_model;
};
