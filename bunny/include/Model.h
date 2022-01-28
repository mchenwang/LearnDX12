#ifndef __MODEL_H__
#define __MODEL_H__

#include <DirectXMath.h>
#include <vector>
#include <string>

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT3 normal;
    // XMFLOAT4 color;
};

class Model
{
private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indicies;

    void SetVertices(std::vector<std::array<double, 3>>&);
    void SetIndicies(std::vector<std::vector<uint32_t>>&);
    void CalculateVertexNormal();
    void Reconstruct(bool addFloor = true); // 将模型移动放缩到 [-1,1]^3 的空间内

public:
    static std::wstring GetModelFullPath(std::wstring model_name);

    Model(std::wstring model_name) noexcept;
    ~Model() = default;

    std::vector<Vertex> GetVertices() const;
    std::vector<uint32_t> GetIndicies() const;
    uint32_t GetVerticesNum() const;
    uint32_t GetIndiciesNum() const;
};

#endif