#include "Model.h"
#include "happly.h"
#include "path.h"
#include <locale>
#include <codecvt>

inline static std::string to_byte_string(const std::wstring& input)
{
    //std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(input);
}

std::wstring Model::GetModelFullPath(std::wstring model_name)
{
    return std::wstring(model_path) + model_name;
}

Model::Model(std::wstring model_name) noexcept
{
    m_vertices = nullptr;
    m_indicies = nullptr;

    happly::PLYData plyIn(to_byte_string(Model::GetModelFullPath(model_name)));
    SetVertices(plyIn.getVertexPositions());
    SetIndicies(plyIn.getFaceIndices<uint32_t>());
}

Model::~Model()
{
    if (m_vertices) delete[] m_vertices;
    if (m_indicies) delete[] m_indicies;
    m_vertices = nullptr;
    m_indicies = nullptr;
}
static Vertex g_Vertices[8] = {
    { XMFLOAT3(.0f, .0f, .0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) }, // 0
    { XMFLOAT3(.0f, .0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) }, // 1
    { XMFLOAT3(.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) }, // 2
    { XMFLOAT3(.0f, 1.0f, .0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) }, // 3
    { XMFLOAT3(1.0f, .0f, .0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }, // 4
    { XMFLOAT3(1.0f, .0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) }, // 5
    { XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }, // 6
    { XMFLOAT3(1.0f, 1.0f, .0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) }  // 7
};
void Model::SetVertices(std::vector<std::array<double, 3>>& positions)
{
    if (m_vertices)
    {
        delete[] m_vertices;
        m_vertices = nullptr;
    }

    m_numVertices = positions.size();
    m_vertices = new Vertex[m_numVertices];
    for (int i = 0; i < m_numVertices; ++i)
    {
        m_vertices[i] = Vertex{
            XMFLOAT3(positions[i][0], positions[i][1], positions[i][2]),
            g_Vertices[(i%8)].color
        };
    }
}

// cut a polygon to several triangles
// Note that the face list generates triangles in the order of a TRIANGLE FAN, not a TRIANGLE STRIP. In the example above, the first face
//   4 0 1 2 3
// Is composed of the triangles 0,1,2 and 0,2,3 and not 0,1,2 and 1,2,3.
static void CutPolygon(std::vector<uint32_t>& polygon, std::vector<std::array<uint32_t, 3>>& triangles)
{
    uint32_t i0 = 0;
    // uint32_t i1 = 1;
    uint32_t i2 = 2;
    for (; i2 < polygon.size(); i2++)
    {
        uint32_t i1 = i2 - 1;
        triangles.emplace_back(std::array<uint32_t, 3>{polygon[i0], polygon[i1], polygon[i2]});
    }
    return ;
}

void Model::SetIndicies(std::vector<std::vector<uint32_t>>& faces)
{
    std::vector<uint32_t> indicies;
    for(auto& face: faces)
    {
        assert(face.size() >= 3 && "model format error.");
        std::vector<std::array<uint32_t, 3>> triangles;
        CutPolygon(face, triangles);
        for (auto& tri: triangles)
        {
            indicies.push_back(tri[0]);
            indicies.push_back(tri[1]);
            indicies.push_back(tri[2]);
        }
    }

    if (m_indicies)
    {
        delete[] m_indicies;
        m_indicies = nullptr;
    }
    m_numIndicies = indicies.size();
    m_indicies = new uint32_t[m_numIndicies];
    for (int i = 0; i < m_numIndicies; ++i) 
    {
        m_indicies[i] = indicies[i];
    }
}

Vertex* Model::GetVertices() const
{
    return m_vertices;
}
uint32_t* Model::GetIndicies() const
{
    return m_indicies;
}
uint32_t Model::GetVerticesNum() const
{
    return m_numVertices;
}
uint32_t Model::GetIndiciesNum() const
{
    return m_numIndicies;
}