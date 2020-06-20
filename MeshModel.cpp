#include "MeshModel.hpp"

MeshModel::MeshModel(std::vector<Mesh> &newMeshList)
    : m_meshList(newMeshList),
      m_model(glm::mat4{1.0f})
{
}

uint32_t MeshModel::GetMeshCount()
{
    return static_cast<uint32_t>(m_meshList.size());
}

Mesh *MeshModel::GetMesh(size_t index)
{
    if (index >= m_meshList.size())
    {
        throw std::runtime_error("Attempted to access invalid Mesh Index");
    }

    return &m_meshList[index];
}

glm::mat4 MeshModel::GetModel()
{
    return m_model;
}

const glm::mat4 *MeshModel::GetModelPtr()
{
    return &m_model;
}

void MeshModel::SetModel(const glm::mat4 &newModel)
{
    m_model = newModel;
}

void MeshModel::DestroyModel()
{
    for (Mesh &mesh : m_meshList)
    {
        mesh.DestroyBuffers();
    }
}

std::vector<std::string> MeshModel::LoadMaterials(const aiScene *scene)
{
    // Create 1:1 sized list of textures
    std::vector<std::string> textureList(scene->mNumMaterials);

    // Go through each material and copy its texture file name (if it exists)
    for (size_t i = 0; i < scene->mNumMaterials; i++)
    {
        // Get the material
        aiMaterial *material = scene->mMaterials[i];

        // Inintalize the texture to empty string (will be replaced if texture exists)
        textureList[i] = "";

        // Check for a Diffuse Texture (standard detail texture)
        if (material->GetTextureCount(aiTextureType_DIFFUSE))
        {
            aiString path;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
            {
                // Cut off any directory information already present
                int idx = std::string(path.data).rfind("\\");

                std::string fileName = std::string(path.data).substr(idx + 1);

                textureList[i] = fileName;
            }
        }
    }

    return textureList;
}

std::vector<Mesh> MeshModel::LoadModel(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
                                       aiNode *node, const aiScene *scene, std::vector<int> &matToTex)
{
    std::vector<Mesh> meshList;

    // Go through each mesh at this node and create it, then add it to out meshList
    for (size_t i = 0; i < node->mNumMeshes; i++)
    {
        meshList.push_back(
            LoadMesh(physicalDevice, device, transferQueue, transferCommandPool, scene->mMeshes[node->mMeshes[i]], scene, matToTex));
    }

    // Go through each node attached to this node and load it, then append their meshes to this node's mesh list
    for (size_t i = 0; i < node->mNumChildren; i++)
    {
        std::vector<Mesh> newList = LoadModel(physicalDevice, device, transferQueue, transferCommandPool, node->mChildren[i], scene, matToTex);
        meshList.insert(meshList.end(), newList.begin(), newList.end());
    }

    return meshList;
}

Mesh MeshModel::LoadMesh(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
                         aiMesh *mesh, const aiScene *scene, std::vector<int> &matToTex)
{
    std::vector<uint32_t> indices;
    // Vertex list for holding all vertices for mesh
    std::vector<Vertex> vertices(mesh->mNumVertices);

    for (size_t i = 0; i < mesh->mNumVertices; i++)
    {
        // Set Position
        vertices[i].pos = {mesh->mVertices[i].x,
                           mesh->mVertices[i].y,
                           mesh->mVertices[i].z};

        // Set tex coords (if they exist)
        if (mesh->mTextureCoords[0])
        {
            vertices[i].tex = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
        }
        else
        {
            vertices[i].tex = {0.0f, 0.0f};
        }

        // Set Color (just use white for now)
        vertices[i].col = {1.f, 1.f, 1.f};
    }

    // Iterate over indices through faces and copy across
    for (size_t i = 0; i < mesh->mNumFaces; i++)
    {
        // Get a face
        aiFace face = mesh->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    // Create new mesh with details and return it
    Mesh newMesh = Mesh(physicalDevice, device, transferQueue, transferCommandPool, &vertices, &indices, matToTex[mesh->mMaterialIndex]);

    return newMesh;
}

MeshModel::~MeshModel()
{
}
