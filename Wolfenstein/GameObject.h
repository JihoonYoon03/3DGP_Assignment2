#pragma once

#include "Mesh.h"

class CShader;

// Tag used for type-driven collision queries. The generalized collision
// helper takes two tags and reports overlapping AABB pairs, so adding a
// new participant in collision (e.g. an enemy) only needs a new tag value
// and a call site -- no per-pair code path.
enum class EObjectTag {
	Generic = 0,
	Bullet,
	Enemy,
	Player,
	Wall,
	Pickup,
};

class CGameObject
{
public:
	CGameObject();
	virtual ~CGameObject();

	void ReleaseUploadBuffers();
	virtual void SetMesh(std::shared_ptr<CMesh> pMesh);
	virtual void SetShader(std::shared_ptr<CShader> pShader);
	virtual void Animate(float fTimeElapsed);
	virtual void OnPrepareRender();
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, class CCamera* pCamera);
	// �θ�(�̴Ͼ�ó ��ȯ ��) �ȿ��� �ڽ��� �������Ѵ�. ���� ��Ŀ� �θ� ����� ���ؼ� ����Ѵ�.
	virtual void RenderInParent(ID3D12GraphicsCommandList* pd3dCommandList, class CCamera* pCamera, const XMFLOAT4X4& xmf4x4Parent);

	// ��� ���۸� �����Ѵ�.
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	// ��� ������ ������ �����Ѵ�.
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseShaderVariables();

	// ���� ��ü�� ��ġ�� �����Ѵ�.
	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 xmf3Position);

	// ���� ��ü�� ���� ��ȯ ��Ŀ��� ��ġ ���Ϳ� ���� ���͸� ��ȯ�Ѵ�.
	XMFLOAT3 GetPosition();
	XMFLOAT3 GetLook();
	XMFLOAT3 GetUp();
	XMFLOAT3 GetRight();

	// ���� ��ü�� ���� x-��, y-��, z-�� �������� �̵���Ų��.
	void MoveStrafe(float fDistance = 1.0f);
	void MoveUp(float fDistance = 1.0f);
	void MoveForward(float fDistance = 1.0f);

	// ���� ��ü�� ȸ����Ų��.
	void Rotate(XMFLOAT3* pxmf3Axis, float fAngle);
	void Rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);

	// Set the world matrix to a yaw-only rotation around Y combined with a
	// translation. Used by the TPS player so the visible model rotates to
	// match the camera's yaw instead of staying axis-aligned.
	void SetWorldYawAndPosition(float fYaw, const XMFLOAT3& xmf3Position);

	// ---- Collision tag / AABB ----
	// Tag drives which type-pair the generalized collision helper considers
	// this object for. AABB half-extents are in world units around the
	// object's translation column.
	EObjectTag GetTag() const { return m_eTag; }
	void SetTag(EObjectTag eTag) { m_eTag = eTag; }
	const XMFLOAT3& GetAABBHalf() const { return m_xmf3AABBHalf; }
	void SetAABBHalf(const XMFLOAT3& xmf3Half) { m_xmf3AABBHalf = xmf3Half; }
	bool IsAlive() const { return m_bAlive; }
	void Kill() { m_bAlive = false; }

	// Called by the collision helper when this object is reported as
	// participating in an overlap. Subclasses can override to react.
	virtual void OnHit(CGameObject* /*pOther*/) {}

	const XMFLOAT4X4& GetWorldMatrixRef() const { return m_xmf4x4World; }

protected:
	XMFLOAT4X4 m_xmf4x4World;
	std::shared_ptr<CMesh> m_pMesh;
	std::shared_ptr<CShader> m_pShader;

	EObjectTag m_eTag = EObjectTag::Generic;
	XMFLOAT3 m_xmf3AABBHalf{ 0.5f, 0.5f, 0.5f };
	bool m_bAlive = true;
};

class CRotatingObject : public CGameObject
{
public:
	CRotatingObject();
	virtual ~CRotatingObject();

private:
	XMFLOAT3 m_xmf3RotationAxis;
	float m_fRotationSpeed;

public:
	void SetRotationSpeed(float fRotationSpeed) { m_fRotationSpeed = fRotationSpeed; }
	void SetRotationAxis(XMFLOAT3 xmf3RotationAxis) { m_xmf3RotationAxis = xmf3RotationAxis; }
	virtual void Animate(float fTimeElapsed);
};

// Forward declaration used by CBulletObject's animation path.
enum class SceneState : int;

// Simple straight-line projectile. Lives for at most m_fLife seconds and
// dies if its center enters a blocked grid cell. The Scene owns its
// lifetime and prunes dead bullets every frame.
class CBulletObject : public CGameObject
{
public:
	CBulletObject(const XMFLOAT3& xmf3Start, const XMFLOAT3& xmf3Dir, float fSpeed);
	virtual ~CBulletObject();

	void SetSceneState(SceneState eState) { m_eSceneState = eState; }
	virtual void Animate(float fTimeElapsed) override;

private:
	XMFLOAT3 m_xmf3Velocity{ 0.0f, 0.0f, 0.0f };
	float m_fLife = 3.0f;
	SceneState m_eSceneState;
};
