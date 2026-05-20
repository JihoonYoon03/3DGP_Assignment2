#include "stdafx.h"
#include "Camera.h"

CCamera::CCamera()
{
	m_xmf4x4View = Matrix4x4::Identity();
	m_xmf4x4Projection = Matrix4x4::Identity();
	m_d3dViewport = { 0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f };
	m_d3dScissorRect = { 0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT };
}

CCamera::~CCamera()
{
}

void CCamera::SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float fMinZ, float fMaxZ)
{
	m_d3dViewport.TopLeftX = float(xTopLeft);
	m_d3dViewport.TopLeftY = float(yTopLeft);
	m_d3dViewport.Width = float(nWidth);
	m_d3dViewport.Height = float(nHeight);
	m_d3dViewport.MinDepth = fMinZ;
	m_d3dViewport.MaxDepth = fMaxZ;
}

void CCamera::SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom)
{
	m_d3dScissorRect.left = xLeft;
	m_d3dScissorRect.top = yTop;
	m_d3dScissorRect.right = xRight;
	m_d3dScissorRect.bottom = yBottom;
}

void CCamera::GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fAspectRatio, float fFOVAngle)
{
	m_xmf4x4Projection = Matrix4x4::PerspectiveFovLH(XMConvertToRadians(fFOVAngle),
		fAspectRatio, fNearPlaneDistance, fFarPlaneDistance);
}

void CCamera::GenerateViewMatrix(XMFLOAT3 xmf3Position, XMFLOAT3 xmf3LookAt, XMFLOAT3 xmf3Up)
{
	// �� �Լ��� 1��Ī ������ �ʱ� ��ġ�� ������ �¾��Ѵ�. ���� Move/Rotate ȣ���� �����ȴ�.
	m_xmf3Position = xmf3Position;

	XMVECTOR vPos = XMLoadFloat3(&xmf3Position);
	XMVECTOR vAt  = XMLoadFloat3(&xmf3LookAt);
	XMVECTOR vUpHint = XMLoadFloat3(&xmf3Up);

	XMVECTOR vLook = XMVector3Normalize(XMVectorSubtract(vAt, vPos));
	XMVECTOR vRight = XMVector3Normalize(XMVector3Cross(vUpHint, vLook));
	XMVECTOR vUp = XMVector3Normalize(XMVector3Cross(vLook, vRight));

	XMStoreFloat3(&m_xmf3Look, vLook);
	XMStoreFloat3(&m_xmf3Right, vRight);
	XMStoreFloat3(&m_xmf3Up, vUp);

	// look ���ͷκ��� yaw/pitch �� ������ �����ν� ���� Rotate ȣ���� �ڿ������� ������ �� �ֵ��� �Ѵ�.
	m_fPitch = asinf(m_xmf3Look.y);
	m_fYaw = atan2f(m_xmf3Look.x, m_xmf3Look.z);

	m_xmf4x4View = Matrix4x4::LookAtLH(xmf3Position, xmf3LookAt, xmf3Up);
}

void CCamera::Move(const XMFLOAT3& xmf3Shift)
{
	m_xmf3Position.x += xmf3Shift.x;
	m_xmf3Position.y += xmf3Shift.y;
	m_xmf3Position.z += xmf3Shift.z;
	RegenerateViewMatrix();
}

void CCamera::SetPosition(const XMFLOAT3& xmf3Position)
{
	m_xmf3Position = xmf3Position;
	RegenerateViewMatrix();
}

void CCamera::Rotate(float fPitchDelta, float fYawDelta)
{
	// ����/�¿� ȸ��(Pitch / Yaw) ���� ����.
	m_fPitch += fPitchDelta;
	m_fYaw += fYawDelta;

	// ��/�Ʒ��� ���� �������� ���� �þ߰� ������ �� �����Ƿ� ���� 90��(89��) ��ó���� �����Ѵ�.
	const float kPitchLimit = XMConvertToRadians(89.0f);
	if (m_fPitch > kPitchLimit) m_fPitch = kPitchLimit;
	if (m_fPitch < -kPitchLimit) m_fPitch = -kPitchLimit;

	// Yaw �� -2�� ~ +2�� ������ ����ȭ�Ѵ� (�����÷ο� ����).
	const float kTwoPi = XM_2PI;
	if (m_fYaw > kTwoPi) m_fYaw -= kTwoPi;
	if (m_fYaw < -kTwoPi) m_fYaw += kTwoPi;

	RegenerateViewMatrix();
}

void CCamera::RegenerateViewMatrix()
{
	// Pitch/Yaw �κ��� �� look ���͸� �����, ����-�� ��(0,1,0) �� �̿��� ��/�� ���͸� �籸���Ѵ�.
	const float cp = cosf(m_fPitch);
	const float sp = sinf(m_fPitch);
	const float cy = cosf(m_fYaw);
	const float sy = sinf(m_fYaw);

	// Left-Handed: +Z = forward, +X = right, +Y = up.
	m_xmf3Look = XMFLOAT3(sy * cp, sp, cy * cp);

	XMVECTOR vWorldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR vLook = XMVector3Normalize(XMLoadFloat3(&m_xmf3Look));
	XMVECTOR vRight = XMVector3Normalize(XMVector3Cross(vWorldUp, vLook));
	XMVECTOR vUp = XMVector3Normalize(XMVector3Cross(vLook, vRight));

	XMStoreFloat3(&m_xmf3Right, vRight);
	XMStoreFloat3(&m_xmf3Up, vUp);

	XMFLOAT3 lookAt;
	lookAt.x = m_xmf3Position.x + m_xmf3Look.x;
	lookAt.y = m_xmf3Position.y + m_xmf3Look.y;
	lookAt.z = m_xmf3Position.z + m_xmf3Look.z;

	XMFLOAT3 upHint;
	XMStoreFloat3(&upHint, vUp);
	m_xmf4x4View = Matrix4x4::LookAtLH(m_xmf3Position, lookAt, upHint);
}

void CCamera::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
}

void CCamera::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4View;
	// �� �켱 -> �� �켱�� ���� ��ġ
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4View)));

	// Root Parameter Index 1 �� 16���� 32BitConstants
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4View, 0);

	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4Projection)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4Projection, 16);
}

void CCamera::ReleaseShaderVariables()
{
}

void CCamera::SetPositionAndTarget(const XMFLOAT3& pos, const XMFLOAT3& target)
{
	m_xmf3Position = pos;

	XMFLOAT3 d{ target.x - pos.x, target.y - pos.y, target.z - pos.z };
	const float len = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
	if (len > 1e-5f) { d.x /= len; d.y /= len; d.z /= len; }
	m_xmf3Look = d;

	XMVECTOR vWorldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR vLook    = XMLoadFloat3(&m_xmf3Look);
	XMVECTOR vRight   = XMVector3Normalize(XMVector3Cross(vWorldUp, vLook));
	XMVECTOR vUp      = XMVector3Normalize(XMVector3Cross(vLook, vRight));
	XMStoreFloat3(&m_xmf3Right, vRight);
	XMStoreFloat3(&m_xmf3Up, vUp);

	XMFLOAT3 upHint; XMStoreFloat3(&upHint, vUp);
	m_xmf4x4View = Matrix4x4::LookAtLH(pos, target, upHint);
	// m_fPitch / m_fYaw 는 궤도 각도로 유지 (변경 금지)
}

void CCamera::SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->RSSetViewports(1, &m_d3dViewport);
	pd3dCommandList->RSSetScissorRects(1, &m_d3dScissorRect);
}
