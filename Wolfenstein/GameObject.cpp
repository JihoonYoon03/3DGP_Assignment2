#include "stdafx.h"
#include "GameObject.h"
#include "Shader.h"
#include "Camera.h"
#include "Maps.h"

CGameObject::CGameObject()
{
	XMStoreFloat4x4(&m_xmf4x4World, XMMatrixIdentity());
}

CGameObject::~CGameObject()
{
	if (m_pShader) {
		m_pShader->ReleaseShaderVariables();
	}
}

void CGameObject::SetShader(std::shared_ptr<CShader> pShader)
{
	m_pShader = std::move(pShader);
}

void CGameObject::SetMesh(std::shared_ptr<CMesh> pMesh)
{
	m_pMesh = std::move(pMesh);
}

void CGameObject::ReleaseUploadBuffers()
{
	// ���� ���۸� ���� ���ε� ���۸� �Ҹ��Ų��.
	if (m_pMesh) m_pMesh->ReleaseUploadBuffers();
}

void CGameObject::Animate(float fTimeElapsed)
{

}

void CGameObject::OnPrepareRender()
{

}

void CGameObject::RenderInParent(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, const XMFLOAT4X4& xmf4x4Parent)
{
	OnPrepareRender();

	// (���� ��� * �θ� ���) �� ��ġ�� 32-bit constants ���� 0 �� ���ε��Ѵ�.
	XMMATRIX mtxLocal = XMLoadFloat4x4(&m_xmf4x4World);
	XMMATRIX mtxParent = XMLoadFloat4x4(&xmf4x4Parent);
	XMMATRIX mtxCombined = XMMatrixMultiply(mtxLocal, mtxParent);
	XMFLOAT4X4 xmf4x4Transposed;
	XMStoreFloat4x4(&xmf4x4Transposed, XMMatrixTranspose(mtxCombined));
	pd3dCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4Transposed, 0);

	if (m_pShader) m_pShader->Render(pd3dCommandList, pCamera);
	if (m_pMesh) m_pMesh->Render(pd3dCommandList);
}

void CGameObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	OnPrepareRender();

	// ��ü�� ������ ���̴� ����(cBuffer)�� �����Ѵ�.
	UpdateShaderVariables(pd3dCommandList);

	// ���� ��ü�� ���� ��ȯ ����� ���̴��� ��� ���۷� ����(����)�Ѵ�.
	if (m_pShader) m_pShader->Render(pd3dCommandList, pCamera);

	// ���� ��ü�� �޽��� ����Ǿ� ������ �޽��� �������Ѵ�.
	if (m_pMesh) m_pMesh->Render(pd3dCommandList);
}

void CGameObject::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
}

void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4World)));
	// ��ü�� ���� ��ȯ ����� ��Ʈ ����� ���� ���̴� ����(cBuffer)�� �����Ѵ�.
	pd3dCommandList->SetGraphicsRoot32BitConstants(0, 16, &xmf4x4World, 0);
}

void CGameObject::ReleaseShaderVariables()
{
}

void CGameObject::SetPosition(float x, float y, float z)
{
	m_xmf4x4World._41 = x;
	m_xmf4x4World._42 = y;
	m_xmf4x4World._43 = z;
}

void CGameObject::SetPosition(XMFLOAT3 xmf3Position)
{
	SetPosition(xmf3Position.x, xmf3Position.y, xmf3Position.z);
}

XMFLOAT3 CGameObject::GetPosition()
{
	return XMFLOAT3(m_xmf4x4World._41, m_xmf4x4World._42, m_xmf4x4World._43);
}

XMFLOAT3 CGameObject::GetLook()
{
	return Vector3::Normalize(XMFLOAT3(m_xmf4x4World._31, m_xmf4x4World._32, m_xmf4x4World._33));
}

XMFLOAT3 CGameObject::GetUp()
{
	return Vector3::Normalize(XMFLOAT3(m_xmf4x4World._21, m_xmf4x4World._22, m_xmf4x4World._23));
}

XMFLOAT3 CGameObject::GetRight()
{
	return Vector3::Normalize(XMFLOAT3(m_xmf4x4World._11, m_xmf4x4World._12, m_xmf4x4World._13));
}

void CGameObject::MoveStrafe(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Right = GetRight();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Right, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::MoveUp(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Up = GetUp();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Up, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::MoveForward(float fDistance)
{
	XMFLOAT3 xmf3Position = GetPosition();
	XMFLOAT3 xmf3Look = GetLook();
	xmf3Position = Vector3::Add(xmf3Position, xmf3Look, fDistance);
	CGameObject::SetPosition(xmf3Position);
}

void CGameObject::Rotate(XMFLOAT3* pxmf3Axis, float fAngle)
{
	XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(pxmf3Axis), XMConvertToRadians(fAngle));
	m_xmf4x4World = Matrix4x4::Multiply(mtxRotate, m_xmf4x4World);
}

void CGameObject::Rotate(float fPitch, float fYaw, float fRoll)
{
	XMMATRIX mtxRotate = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(fPitch),
		XMConvertToRadians(fYaw),
		XMConvertToRadians(fRoll));
	m_xmf4x4World = Matrix4x4::Multiply(mtxRotate, m_xmf4x4World);
}

void CGameObject::SetWorldYawAndPosition(float fYaw, const XMFLOAT3& xmf3Position)
{
	// Build rotation-around-Y then translation, store the row-major result
	// directly so the rotation does not accumulate across frames.
	XMMATRIX mRot = XMMatrixRotationY(fYaw);
	XMMATRIX mTr  = XMMatrixTranslation(xmf3Position.x, xmf3Position.y, xmf3Position.z);
	XMStoreFloat4x4(&m_xmf4x4World, XMMatrixMultiply(mRot, mTr));
}

// ==========================================================================
// ==========================================================================
CRotatingObject::CRotatingObject()
{
	m_xmf3RotationAxis = XMFLOAT3(0.0f, 1.0f, 0.0f);
	m_fRotationSpeed = 90.0f;
}

CRotatingObject::~CRotatingObject()
{
}

void CRotatingObject::Animate(float fTimeElapsed)
{
	CGameObject::Rotate(&m_xmf3RotationAxis, m_fRotationSpeed * fTimeElapsed);
}

// ==========================================================================
// CBulletObject
// ==========================================================================
CBulletObject::CBulletObject(const XMFLOAT3& xmf3Start, const XMFLOAT3& xmf3Dir, float fSpeed)
	: m_eSceneState(static_cast<SceneState>(0))
{
	m_eTag = EObjectTag::Bullet;
	m_xmf3AABBHalf = XMFLOAT3(0.2f, 0.2f, 0.2f);
	float len = sqrtf(xmf3Dir.x * xmf3Dir.x + xmf3Dir.y * xmf3Dir.y + xmf3Dir.z * xmf3Dir.z);
	if (len < 1e-5f) len = 1.0f;
	m_xmf3Velocity = XMFLOAT3(xmf3Dir.x / len * fSpeed, xmf3Dir.y / len * fSpeed, xmf3Dir.z / len * fSpeed);
	SetPosition(xmf3Start);
}

CBulletObject::~CBulletObject()
{
}

void CBulletObject::Animate(float fTimeElapsed)
{
	if (!m_bAlive) return;

	XMFLOAT3 pos(m_xmf4x4World._41, m_xmf4x4World._42, m_xmf4x4World._43);
	pos.x += m_xmf3Velocity.x * fTimeElapsed;
	pos.y += m_xmf3Velocity.y * fTimeElapsed;
	pos.z += m_xmf3Velocity.z * fTimeElapsed;
	SetPosition(pos);

	m_fLife -= fTimeElapsed;
	if (m_fLife <= 0.0f) { m_bAlive = false; return; }

	// Bullet vs. maze geometry. We check two things:
	//   1. The cell is solid (wall or out-of-bounds). IsBlockedInMap with a
	//      large feetY ignores step heights so only true walls trigger.
	//   2. The bullet is below the cell's floor surface (it hit the side of
	//      a step). No STEP_UP_TOLERANCE because the bullet is a point.
	if (IsBlockedInMap(m_eSceneState, pos.x, pos.z, pos.y + 1000.0f)) {
		m_bAlive = false;
		return;
	}
	const float floorTopY = GetFloorHeightAt(m_eSceneState, pos.x, pos.z);
	if (floorTopY > pos.y) {
		m_bAlive = false;
	}
}
