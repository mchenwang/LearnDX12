#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <DirectXMath.h>

using namespace DirectX;

class Camera
{
protected:
    float m_viewW;
    float m_viewH;
    
    float m_near;
    float m_far;

    XMFLOAT4 m_position;
    XMFLOAT4 m_focusPoint;
    XMFLOAT4 m_upDirection;

    XMMATRIX m_viewMatrix;
    XMMATRIX m_projectionMatrix;

    void SetViewMatrix();
    virtual void SetProjectionMatrix() = 0;
public:
    Camera(
        float windowWidth, float windowHeight, float near, float far,
        XMFLOAT4 position, XMFLOAT4 focusPoint, XMFLOAT4 upDirection
    );
    ~Camera() = default;

    void UpdatePosition(XMFLOAT4 position);
    void UpdateFocusPoint(XMFLOAT4 focusPoint);
    void UpdateUpDirection(XMFLOAT4 upDirection);

    virtual void UpdateViewport(float windowWidth, float windowHeight);

    XMFLOAT4 GetPosition() const;

    XMMATRIX GetViewMatrix();
    XMMATRIX GetProjectionMatrix();
};

class PerspectiveCamera : public Camera
{
private:
    // view frustum data
    float m_fov;
    float m_aspectRatio;

    void SetAspectRatio(float windowWidth, float windowHeight);
    void SetProjectionMatrix() override;
public:
    PerspectiveCamera(
        float windowWidth,
        float windowHeight,
        float fov = 45.f,
        float near = 0.001f,
        float far = 100.f,
        XMFLOAT4 position = XMFLOAT4(0.5f, 0.5f, 5.f, 1.f),
        XMFLOAT4 focusPoint = XMFLOAT4(0.f, 0.f, 0.f, 1.f),
        XMFLOAT4 upDirection = XMFLOAT4(0.f, 1.f, 0.f, 0.f)
    );
    ~PerspectiveCamera() = default;

    void UpdateViewport(float windowWidth, float windowHeight) override;
    void UpdateFoV(float FoV);
};

class OrthographicCamera : public Camera
{
    void SetProjectionMatrix() override;
public:
    OrthographicCamera(
        float windowWidth,
        float windowHeight,
        float near = 0.001f,
        float far = 100.f,
        XMFLOAT4 position = XMFLOAT4(0.5f, 0.5f, 5.f, 1.f),
        XMFLOAT4 focusPoint = XMFLOAT4(0.f, 0.f, 0.f, 1.f),
        XMFLOAT4 upDirection = XMFLOAT4(0.f, 1.f, 0.f, 0.f)
    );
};

#endif