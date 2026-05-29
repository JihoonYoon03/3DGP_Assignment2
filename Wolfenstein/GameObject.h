#pragma once

#include "Mesh.h"

#include <functional>
#include <random>

class CShader;

// 충돌 태그
enum class EObjectTag {
	Generic = 0,
	Bullet,        // 플레이어 총알
	Enemy,
	Player,
	Wall,
	Pickup,
	EnemyBullet,   // 적 총알
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
	// 부모 변환 안에서 렌더링 (미니어처용)
	virtual void RenderInParent(ID3D12GraphicsCommandList* pd3dCommandList, class CCamera* pCamera, const XMFLOAT4X4& xmf4x4Parent);

	// 상수 버퍼
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseShaderVariables();

	// 위치 설정
	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 xmf3Position);

	// 월드 행렬에서 위치/방향 벡터를 반환
	XMFLOAT3 GetPosition();
	XMFLOAT3 GetLook();
	XMFLOAT3 GetUp();
	XMFLOAT3 GetRight();

	// 로컬 축 이동
	void MoveStrafe(float fDistance = 1.0f);
	void MoveUp(float fDistance = 1.0f);
	void MoveForward(float fDistance = 1.0f);

	// 회전
	void Rotate(XMFLOAT3* pxmf3Axis, float fAngle);
	void Rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);

	// Y 회전 + 위치로 월드 행렬 설정 (TPS 플레이어 모델용)
	void SetWorldYawAndPosition(float fYaw, const XMFLOAT3& xmf3Position);

	// forward 벡터 + 위치로 월드 행렬 직접 구성 (소총용)
	void SetWorldOrientation(const XMFLOAT3& xmf3Forward, const XMFLOAT3& xmf3Position);

	// 충돌 태그 / AABB
	EObjectTag GetTag() const { return m_eTag; }
	void SetTag(EObjectTag eTag) { m_eTag = eTag; }
	const XMFLOAT3& GetAABBHalf() const { return m_xmf3AABBHalf; }
	void SetAABBHalf(const XMFLOAT3& xmf3Half) { m_xmf3AABBHalf = xmf3Half; }
	bool IsAlive() const { return m_bAlive; }
	void Kill() { m_bAlive = false; }

	// 충돌 시 호출 (서브클래스에서 오버라이드)
	virtual void OnHit(CGameObject* /*pOther*/) {}

	const XMFLOAT4X4& GetWorldMatrixRef() const { return m_xmf4x4World; }
	// 외부에서 합성된 월드 행렬을 직접 주입 (사망 시 부착 등)
	void SetWorldMatrix(const XMFLOAT4X4& xmf4x4World) { m_xmf4x4World = xmf4x4World; }

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

enum class SceneState : int;

// 직선 투사체. 수명이 다하거나 벽에 닿으면 Kill().
// eTag 로 플레이어 총알(Bullet) 과 적 총알(EnemyBullet) 을 구분한다.
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

// 플레이어/적의 공통 기반 클래스. HP, 점프 물리, 소총 부착, XZ 분리 충돌 이동.
class CCharacter : public CGameObject
{
public:
	CCharacter();
	virtual ~CCharacter();

	// HP
	int  GetHP() const { return m_nHP; }
	void SetHP(int nHP) { m_nHP = nHP; }
	void TakeDamage(int n);

	// 점프
	bool IsJumping() const { return m_bJumping; }
	void StartJump(float fInitialVelocity);

	// 소총
	void SetRifleMesh(std::shared_ptr<CMesh> pMesh);
	CGameObject* GetRifle() const { return m_pRifle.get(); }

	// 현재 씬 상태 캐시
	void SetSceneState(SceneState eState) { m_eSceneState = eState; }
	SceneState GetSceneState() const { return m_eSceneState; }

	// 피격 콜백 (기본: HP -1, 0 이하면 Kill)
	virtual void OnHit(CGameObject* pOther) override;

protected:
	// XZ 분리 프로브 이동. 단차에 막혔고 점프 가능하면 자동 점프.
	void TryMoveXZ(const XMFLOAT3& xmf3Dir, float fStep, float fRadius = 0.7f);

	// 점프 중 중력 적용
	void ApplyJumpPhysics(float fTimeElapsed, float fGravity = 20.0f);

	// 점프 쿨다운 진행
	void TickJumpCooldown(float fTimeElapsed);

protected:
	int  m_nHP = 1;
	bool m_bJumping = false;
	float m_fVerticalVelocity = 0.0f;
	float m_fJumpCooldown = 0.0f;

	std::shared_ptr<CGameObject> m_pRifle;

	SceneState m_eSceneState{};
};

// 적 AI 상태
enum class EEnemyAIState {
	Idle_Move = 0,   // 패트롤 이동 중
	Idle_Pause,      // 패트롤 정지 중
	Pursue,          // 플레이어 추적 중
};

// 적 캐릭터. 패트롤하다가 시야가 통하면 영구 추적 모드. 추적 중 2~3초 간격으로 총알 발사.
class CEnemyObject : public CCharacter
{
public:
	CEnemyObject(SceneState eSceneState, unsigned int nSeed);
	virtual ~CEnemyObject();

	virtual void Animate(float fTimeElapsed) override;
	virtual void OnHit(CGameObject* pOther) override;

	// GameFramework 에서 발사 콜백과 플레이어 위치 게터 주입
	void SetFireCallback(std::function<void(const XMFLOAT3&, const XMFLOAT3&)> fn) { m_fnFire = std::move(fn); }
	void SetPlayerPosGetter(std::function<XMFLOAT3()> fn) { m_fnGetPlayer = std::move(fn); }

	// 머리 위 마커 메시 주입과 가시성 토글
	void SetMarkerMesh(std::shared_ptr<CMesh> pMesh);
	void SetMarkerVisible(bool b) { m_bMarkerVisible = b; }
	bool IsMarkerVisible() const { return m_bMarkerVisible; }
	CGameObject* GetMarker() const { return m_pMarker.get(); }

	// 사망 애니메이션 중인지
	bool IsDying() const { return m_fDeathTimer >= 0.0f; }

private:
	EEnemyAIState m_eAIState;
	float         m_fStateTimer;
	XMFLOAT3      m_xmf3IdleDir;
	float         m_fFireCooldown;
	float         m_fAimFreeze;
	bool          m_bAware;
	std::mt19937  m_rng;
	// 본체/소총 facing 단위 벡터(XZ). Pursue 진입 전엔 +Z.
	XMFLOAT3      m_xmf3FacingDir{ 0.0f, 0.0f, 1.0f };

	std::function<void(const XMFLOAT3&, const XMFLOAT3&)> m_fnFire;
	std::function<XMFLOAT3()>                            m_fnGetPlayer;

	// 헬퍼
	float RandFloat(float a, float b);
	XMFLOAT3 PickRandomFreeDirection();
	XMFLOAT3 BestFreeDirectionToward(const XMFLOAT3& towardsDir);

	// A* 경로 캐시
	std::vector<XMFLOAT3> m_vPath;
	size_t                m_nPathIndex  = 0;
	float                 m_fPathTimer  = 0.0f;
	static constexpr float kPathRecalcInterval = 0.5f;

	// 머리 위 마커 기둥
	std::shared_ptr<CGameObject> m_pMarker;
	bool                         m_bMarkerVisible = false;

	// 사망 애니메이션 상태 (0.2초 정지 후 0.8초간 옆으로 90° 쓰러짐)
	float       m_fDeathTimer = -1.0f;
	XMFLOAT4X4  m_xmf4x4DeathBaseWorld{};
	XMFLOAT3    m_xmf3DeathTipAxis{ 0.0f, 0.0f, 1.0f };
	float       m_fDeathBaseY = 0.0f;
	// 사망 시점 소총의 본체-로컬 행렬 (강체 자식처럼 따라 회전/하강시키기 위함)
	XMFLOAT4X4  m_xmf4x4DeathRifleLocal{};
};
