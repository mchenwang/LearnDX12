#ifndef __MODEL_H__
#define __MODEL_H__

#include <DirectXMath.h>
#include <vector>
#include <string>

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT4 color;
};

class Model
{
private:
    Vertex* m_vertices = nullptr;
    uint32_t* m_indicies = nullptr;
    uint32_t m_numVertices = 0;
    uint32_t m_numIndicies = 0;

    void SetVertices(std::vector<std::array<double, 3>>&);
    void SetIndicies(std::vector<std::vector<uint32_t>>&);

public:
    static std::wstring GetModelFullPath(std::wstring model_name);

    Model(std::wstring model_name) noexcept;
    ~Model();

    Vertex* GetVertices() const;
    uint32_t* GetIndicies() const;
    uint32_t GetVerticesNum() const;
    uint32_t GetIndiciesNum() const;
};

#endif