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

	// forward 벡터(메시의 +Z 가 향할 방향) 와 위치로 월드 행렬을 직접 구성한다.
	// right = normalize(worldUp × forward), up = normalize(forward × right).
	// forward 가 world-up 과 거의 평행하면 보조 up 으로 +Z 를 사용한다.
	// 소총 모델을 카메라/플레이어와 무관하게 임의 방향으로 정렬하는 용도.
	void SetWorldOrientation(const XMFLOAT3& xmf3Forward, const XMFLOAT3& xmf3Position);

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
// CCharacter
// ==========================================================================
// 플레이어/적이 공유하는 캐릭터 기반 클래스. CGameObject 가 제공하는 위치/
// 월드 행렬 위에 체력, 점프 물리, 소총 부착, XZ 분리 충돌 이동을 더한다.
//
// 서브클래스(CPlayer / CEnemyObject) 가 차별화하는 부분:
//   - 입력 주체 (키보드/마우스 vs AI 상태머신)
//   - 시각 모델 (메시/색상)
//   - 소총의 부착 위치/방향 (어깨 너머 카메라 vs 적 가슴 높이)
class CCharacter : public CGameObject
{
public:
	CCharacter();
	virtual ~CCharacter();

	// 체력 관련
	int  GetHP() const { return m_nHP; }
	void SetHP(int nHP) { m_nHP = nHP; }
	void TakeDamage(int n);

	// 점프 물리 상태
	bool IsJumping() const { return m_bJumping; }
	void StartJump(float fInitialVelocity);

	// 소총 부착/접근. 서브클래스에서 소총 메시를 주입할 때 호출.
	void SetRifleMesh(std::shared_ptr<CMesh> pMesh);
	CGameObject* GetRifle() const { return m_pRifle.get(); }

	// 현재 씬 상태(MAP1/MAP2) 캐시. TryMoveXZ 가 충돌 판정에 사용.
	void SetSceneState(SceneState eState) { m_eSceneState = eState; }
	SceneState GetSceneState() const { return m_eSceneState; }

	// 피격 콜백 — 기본 구현은 체력 1 감산 후 0 이하면 Kill().
	// 서브클래스는 OnHit 을 오버라이드해 추가 연출(반동 등) 을 더할 수 있다.
	virtual void OnHit(CGameObject* pOther) override;

protected:
	// XZ 분리 프로브 이동 — 평면 dx/dz 각각에 대해 IsBlockedInMap 으로 충돌 확인.
	// 단차에 막혔고 점프 가능 상태면 자동으로 점프 시작.
	void TryMoveXZ(const XMFLOAT3& xmf3Dir, float fStep, float fRadius = 0.7f);

	// 점프 중일 때 매 프레임 호출 — 중력을 적용하고 바닥에 닿으면 상태 복귀.
	void ApplyJumpPhysics(float fTimeElapsed, float fGravity = 20.0f);

	// 점프 쿨다운 타이머 진행 (양수일 때만 감산).
	void TickJumpCooldown(float fTimeElapsed);

protected:
	int  m_nHP = 1;
	bool m_bJumping = false;
	float m_fVerticalVelocity = 0.0f;
	float m_fJumpCooldown = 0.0f;

	std::shared_ptr<CGameObject> m_pRifle;

	SceneState m_eSceneState{};
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

class CEnemyObject : public CCharacter
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

	// 적 머리 위 마커 기둥의 메시를 주입한다. SpawnEnemiesForMap 에서 호출.
	// 내부에서 m_pMarker(CGameObject) 를 생성하고 메시를 부착한다.
	void SetMarkerMesh(std::shared_ptr<CMesh> pMesh);

	// 마커 가시성 토글. Scene::SetEnemyMarkersVisible 이 잔여 적 ≤3 일 때 true 로 설정.
	void SetMarkerVisible(bool b) { m_bMarkerVisible = b; }
	bool IsMarkerVisible() const { return m_bMarkerVisible; }
	// Scene::Render 가 살아있는 적 별로 마커를 추가 렌더할 수 있도록 raw 포인터 노출.
	CGameObject* GetMarker() const { return m_pMarker.get(); }

	// 소총 메시 주입 / 접근은 CCharacter 가 제공한다 (SetRifleMesh, GetRifle).

private:
	EEnemyAIState m_eAIState;
	float         m_fStateTimer;     // 현재 상태 남은 시간(초)
	XMFLOAT3      m_xmf3IdleDir;     // Idle_Move 진입 시 4방향 중 하나
	float         m_fFireCooldown;   // 다음 발사까지 남은 시간(초, 인스턴스 랜덤 2~3초)
	float         m_fAimFreeze;      // 발사 직후 정지 타이머(초, ~0.3초)
	bool          m_bAware;          // 한 번 본 후 영구 true
	std::mt19937  m_rng;             // 인스턴스 전용 RNG
	// Pursue 중 매 프레임 갱신되는 본체/소총 facing 단위 벡터(XZ).
	// LOS 가 열려 있으면 플레이어 방향, 막혀 있으면 현재 이동 방향(= A* 다음
	// 웨이포인트). 기본값은 +Z 로 두어 Pursue 진입 전엔 회전이 적용되지 않는다.
	XMFLOAT3      m_xmf3FacingDir{ 0.0f, 0.0f, 1.0f };

	std::function<void(const XMFLOAT3&, const XMFLOAT3&)> m_fnFire;
	std::function<XMFLOAT3()>                            m_fnGetPlayer;

	// 헬퍼: [a, b) 균등 실수 난수.
	float RandFloat(float a, float b);
	// 헬퍼: 4방향 (+X,-X,+Z,-Z) 중 벽이 없는 방향을 랜덤 선택. 모두 막혀 있으면 zero 벡터.
	XMFLOAT3 PickRandomFreeDirection();
	// 헬퍼: 4방향 중 막히지 않으면서 towardsDir 과 내적이 최대인 방향. 없으면 zero.
	XMFLOAT3 BestFreeDirectionToward(const XMFLOAT3& towardsDir);

	// A* 경로 캐시 — 추적 중 갱신 간격마다 재계산.
	std::vector<XMFLOAT3> m_vPath;           // 경유 셀 월드 좌표 목록
	size_t                m_nPathIndex  = 0; // 현재 향하고 있는 웨이포인트 인덱스
	float                 m_fPathTimer  = 0.0f; // 경로 재계산 타이머
	static constexpr float kPathRecalcInterval = 0.5f;

	// 머리 위 마커 기둥. SetMarkerMesh 호출 시 생성되며, Animate 가 매 프레임 위치를
	// 적 머리 위로 동기화한다. 가시성은 m_bMarkerVisible 에 따라 Scene::Render 가 결정.
	std::shared_ptr<CGameObject> m_pMarker;
	bool                         m_bMarkerVisible = false;
};
