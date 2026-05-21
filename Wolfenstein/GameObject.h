#pragma once

#include "Mesh.h"

#include <functional>
#include <random>

class CShader;

// Tag used for type-driven collision queries. The generalized collision
// helper takes two tags and reports overlapping AABB pairs, so adding a
// new participant in collision (e.g. an enemy) only needs a new tag value
// and a call site -- no per-pair code path.
//
// Bullet  : 플레이어가 발사한 총알 (적과 충돌하여 적을 처치).
// EnemyBullet : 적이 발사한 총알 (플레이어와 충돌하여 라이프를 깎음).
// Bullet 과 EnemyBullet 끼리는 충돌 페어로 등록하지 않아 서로 통과한다.
enum class EObjectTag {
	Generic = 0,
	Bullet,
	Enemy,
	Player,
	Wall,
	Pickup,
	EnemyBullet,
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
// 생성자 마지막 인자 eTag 로 플레이어 총알(EObjectTag::Bullet) 과
// 적 총알(EObjectTag::EnemyBullet) 을 같은 클래스로 표현한다.
class CBulletObject : public CGameObject
{
public:
	CBulletObject(const XMFLOAT3& xmf3Start, const XMFLOAT3& xmf3Dir, float fSpeed,
		EObjectTag eTag = EObjectTag::Bullet);
	virtual ~CBulletObject();

	void SetSceneState(SceneState eState) { m_eSceneState = eState; }
	virtual void Animate(float fTimeElapsed) override;

private:
	XMFLOAT3 m_xmf3Velocity{ 0.0f, 0.0f, 0.0f };
	float m_fLife = 3.0f;
	SceneState m_eSceneState;
};

// ==========================================================================
// CEnemyObject
// ==========================================================================
// 적 AI 객체. 평소엔 패트롤(이동 1~2초 / 정지 2~3초 인스턴스 랜덤)을 반복하고,
// 플레이어와 시야가 통하는 순간 영구 추적 모드(m_bAware=true)로 전환된다.
// 추적 모드에서는 2~3초 간격(인스턴스 랜덤)으로 총알을 발사하며, 발사 직후
// 약 0.3초간 정지하여 조준 연출을 보인다.
// 이동 속도는 플레이어의 0.3배(1.8 u/s) 이며 벽 충돌은 IsBlockedInMap 으로
// 플레이어와 동일한 X/Z 분리 프로브 방식으로 처리한다.
// HP 는 2 — 플레이어 총알 2발 맞으면 Kill().
enum class EEnemyAIState {
	Idle_Move = 0,   // 패트롤 이동 중
	Idle_Pause,      // 패트롤 정지 중
	Pursue,          // 플레이어 추적 중 (m_bAware==true 일 때만)
};

class CEnemyObject : public CGameObject
{
public:
	CEnemyObject(SceneState eSceneState, unsigned int nSeed);
	virtual ~CEnemyObject();

	virtual void Animate(float fTimeElapsed) override;
	virtual void OnHit(CGameObject* pOther) override;

	// GameFramework 에서 적 총알 발사 콜백과 플레이어 위치 게터를 주입한다.
	// CEnemyObject 가 CGameFramework 를 직접 알지 않게 하기 위함.
	void SetFireCallback(std::function<void(const XMFLOAT3&, const XMFLOAT3&)> fn) { m_fnFire = std::move(fn); }
	void SetPlayerPosGetter(std::function<XMFLOAT3()> fn) { m_fnGetPlayer = std::move(fn); }

private:
	EEnemyAIState m_eAIState;
	float         m_fStateTimer;     // 현재 상태 남은 시간(초)
	XMFLOAT3      m_xmf3IdleDir;     // Idle_Move 진입 시 4방향 중 하나
	float         m_fFireCooldown;   // 다음 발사까지 남은 시간(초, 인스턴스 랜덤 2~3초)
	float         m_fAimFreeze;      // 발사 직후 정지 타이머(초, ~0.3초)
	int           m_nHP;             // 잔여 체력 (시작 2)
	bool          m_bAware;          // 한 번 본 후 영구 true
	SceneState    m_eSceneState;
	std::mt19937  m_rng;             // 인스턴스 전용 RNG

	std::function<void(const XMFLOAT3&, const XMFLOAT3&)> m_fnFire;
	std::function<XMFLOAT3()>                            m_fnGetPlayer;

	// 헬퍼: [a, b) 균등 실수 난수.
	float RandFloat(float a, float b);
	// 헬퍼: 4방향 (+X,-X,+Z,-Z) 중 벽이 없는 방향을 랜덤 선택. 모두 막혀 있으면 zero 벡터.
	XMFLOAT3 PickRandomFreeDirection();
	// 헬퍼: 4방향 중 막히지 않으면서 towardsDir 과 내적이 최대인 방향. 없으면 zero.
	XMFLOAT3 BestFreeDirectionToward(const XMFLOAT3& towardsDir);
	// 헬퍼: dir 방향으로 fStep 만큼 이동을 시도한다. X/Z 축 분리 프로브.
	void TryMoveXZ(const XMFLOAT3& xmf3Dir, float fStep);

	// 점프 물리
	bool  m_bJumping          = false;
	float m_fVerticalVelocity = 0.0f;
	float m_fJumpCooldown     = 0.0f;

	// A* 경로 캐시 — Pursue 중 LOS 가 막혔을 때 최단 경로를 따라 이동한다.
	// 0.5초마다 또는 stuck/끝 도달 시 재계산.
	std::vector<XMFLOAT2> m_vPath;
	size_t                m_nPathIdx     = 0;
	float                 m_fRepathTimer = 0.0f;
};
