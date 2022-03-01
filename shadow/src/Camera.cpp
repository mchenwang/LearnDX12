#include "Camera.h"

Camera::Camera(float windowWidth, float windowHeight, float near, float far, XMFLOAT4 position, XMFLOAT4 focusPoint, XMFLOAT4 upDirection)
    : m_viewW(windowWidth)
    , m_viewH(windowHeight)
    , m_near(near)
    , m_far(far)
    , m_position(position)
    , m_focusPoint(focusPoint)
    , m_upDirection(upDirection)
{
    SetViewMatrix();
}

PerspectiveCamera::PerspectiveCamera(
    float windowWidth, float windowHeight,
    float fov, float near, float far,
    XMFLOAT4 position, XMFLOAT4 focusPoint, XMFLOAT4 upDirection)
    : m_fov(fov)
    , Camera(windowWidth, windowHeight, near, far, position, focusPoint, upDirection)
{
    SetAspectRatio(windowWidth, windowHeight);
    SetProjectionMatrix();
}

OrthographicCamera::OrthographicCamera(
    float windowWidth, float windowHeight,
    float near, float far,
    XMFLOAT4 position, XMFLOAT4 focusPoint, XMFLOAT4 upDirection)
    : Camera(windowWidth, windowHeight, near, far, position, focusPoint, upDirection)
{
    SetProjectionMatrix();
}

void Camera::SetViewMatrix()
{
    const XMVECTOR position = XMLoadFloat4(&m_position);
    const XMVECTOR focusPoint = XMLoadFloat4(&m_focusPoint);
    const XMVECTOR upDirection = XMLoadFloat4(&m_upDirection);

    m_viewMatrix = XMMatrixLookAtLH(position, focusPoint, upDirection);
}

void Camera::UpdatePosition(XMFLOAT4 position)
{
    m_position = position;
}
void Camera::UpdateFocusPoint(XMFLOAT4 focusPoint)
{
    m_focusPoint = focusPoint;
}
void Camera::UpdateUpDirection(XMFLOAT4 upDirection)
{
    m_upDirection = upDirection;
}
void Camera::UpdateViewport(float windowWidth, float windowHeight)
{
    m_viewW = windowHeight;
    m_viewH = windowHeight;
}

XMFLOAT4 Camera::GetPosition() const
{
    return m_position;
}

XMMATRIX Camera::GetViewMatrix()
{
    SetViewMatrix();
    return m_viewMatrix;
}

XMMATRIX Camera::GetProjectionMatrix()
{
    SetProjectionMatrix();
    return m_projectionMatrix;
}

void PerspectiveCamera::SetAspectRatio(float windowWidth, float windowHeight)
{
    m_aspectRatio = windowWidth / windowHeight;
}

void PerspectiveCamera::SetProjectionMatrix()
{
    m_projectionMatrix = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(m_fov),
        m_aspectRatio, m_near, m_far);
}

void PerspectiveCamera::UpdateViewport(float windowWidth, float windowHeight)
{
    m_viewW = windowHeight;
    m_viewH = windowHeight;
    SetAspectRatio(windowWidth, windowHeight);
}
void PerspectiveCamera::UpdateFoV(float FoV)
{
    m_fov = FoV;
}

void OrthographicCamera::SetProjectionMatrix()
{
    auto focusPoint = XMLoadFloat4(&m_focusPoint);
    XMFLOAT3 o;
    focusPoint = XMVector3TransformCoord(focusPoint, GetViewMatrix());
    XMStoreFloat3(&o, focusPoint);
    m_projectionMatrix = XMMatrixOrthographicOffCenterLH(o.x - 2, o.x + 2, o.y - 2, o.y + 2, o.z - 2, o.z + 2);
}