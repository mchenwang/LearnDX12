#ifndef __MODEL_H__
#define __MODEL_H__

#include <DirectXMath.h>
#include <vector>
#include <string>
#include "common/ModelLoader.h"

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT2 uv;
    // XMFLOAT4 color;
};

class Model
{
private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indicies;

    void CalculateVertexNormal();

    static std::wstring GetModelFullPath(std::wstring model_name);

public:
    Model(std::wstring model_name, ModelType modelType, bool reconstruct = false) noexcept;
    ~Model() = default;

    std::vector<Vertex> GetVertices() const;
    std::vector<uint32_t> GetIndicies() const;
    uint32_t GetVerticesNum() const;
    uint32_t GetIndiciesNum() const;
};

#endif