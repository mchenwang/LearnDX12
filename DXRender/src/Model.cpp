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
    Reconstruct();
    SetIndicies(plyIn.getFaceIndices<uint32_t>());

    CalculateVertexNormal();
}

Model::~Model()
{
    if (m_vertices) delete[] m_vertices;
    if (m_indicies) delete[] m_indicies;
    m_vertices = nullptr;
    m_indicies = nullptr;
}

void Model::Reconstruct()
{
    if (m_vertices == nullptr || m_numVertices == 0) return;

    float maxX, minX, maxY, minY, maxZ, minZ;
    maxX = minX = m_vertices[0].position.x;
    maxY = minY = m_vertices[0].position.y;
    maxZ = minZ = m_vertices[0].position.z;

    for (int i = 1; i < m_numVertices; ++i)
    {
        if (m_vertices[i].position.x > maxX) maxX = m_vertices[i].position.x;
        if (m_vertices[i].position.x < minX) minX = m_vertices[i].position.x;
        if (m_vertices[i].position.y > maxY) maxY = m_vertices[i].position.y;
        if (m_vertices[i].position.y < minY) minY = m_vertices[i].position.y;
        if (m_vertices[i].position.z > maxZ) maxZ = m_vertices[i].position.z;
        if (m_vertices[i].position.z < minZ) minZ = m_vertices[i].position.z;
    }

    float offsetX = (maxX + minX) / 2.;
    float offsetY = (maxY + minY) / 2.;
    float offsetZ = (maxZ + minZ) / 2.;

    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    float rangeZ = maxZ - minZ;
    float range = std::max(rangeX, std::max(rangeY, rangeZ)) / 2.;

    for (int i = 0; i < m_numVertices; ++i)
    {
        m_vertices[i].position.x = (m_vertices[i].position.x - offsetX) / range;
        m_vertices[i].position.y = (m_vertices[i].position.y - offsetY) / range;
        m_vertices[i].position.z = (m_vertices[i].position.z - offsetZ) / range;
    }
}

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
            XMFLOAT3(positions[i][0], positions[i][1], positions[i][2]), // position
            XMFLOAT3(0.f, 0.f, 0.f),                                     // normal
            // XMFLOAT4(1.f, 1.f, 1.f, 1.f),                                // color
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

static DirectX::XMVECTOR operator-(const DirectX::XMFLOAT3& A, const DirectX::XMFLOAT3& B)
{
    return DirectX::XMVectorSet(A.x - B.x, A.y - B.y, A.z - B.z, 0.f);
}
// static inline DirectX::XMVECTOR DXCross(const DirectX::XMVECTOR& v1, const DirectX::XMVECTOR& v2)
// {
//     auto XMV1 = XMLoadFloat3(&v1);
//     auto XMV2 = XMLoadFloat3(&v2);
//     auto XMRet = XMVector3Cross(XMV1, XMV2);
//     DirectX::XMFLOAT3 ret;
//     XMStoreFloat3(&ret, XMRet);
//     return ret; 
// }
static DirectX::XMFLOAT3 operator+=(DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

void Model::CalculateVertexNormal()
{
    for (int i = 0; i + 2 < m_numIndicies; i += 3)
    {
        auto A = m_vertices[m_indicies[i    ]].position;
        auto B = m_vertices[m_indicies[i + 1]].position;
        auto C = m_vertices[m_indicies[i + 2]].position;
        auto AB = B - A;
        auto AC = C - A;

        DirectX::XMFLOAT3 normal;
        XMStoreFloat3(&normal, XMVector3Cross(AB, AC));

        m_vertices[m_indicies[i    ]].normal += normal;
        m_vertices[m_indicies[i + 1]].normal += normal;
        m_vertices[m_indicies[i + 2]].normal += normal;
    }

    for (int i = 0; i < m_numVertices; ++i)
    {
        auto ret = XMVector3Normalize(XMLoadFloat3(&m_vertices[i].normal));
        XMStoreFloat3(&m_vertices[i].normal, ret);
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