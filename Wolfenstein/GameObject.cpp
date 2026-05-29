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
	// 셰이더 자원은 shared_ptr 의 마지막 참조가 풀릴 때 해제되므로 여기서 호출 안 함.
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

	// (로컬 * 부모) 전치를 b0 슬롯에 업로드
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

	UpdateShaderVariables(pd3dCommandList);

	if (m_pShader) m_pShader->Render(pd3dCommandList, pCamera);

	if (m_pMesh) m_pMesh->Render(pd3dCommandList);
}

void CGameObject::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
}

void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4World)));
	// 월드 행렬의 전치를 루트 상수로 업로드
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
	// Y 회전 + 이동 (회전 누적 없이 직접 갱신)
	XMMATRIX mRot = XMMatrixRotationY(fYaw);
	XMMATRIX mTr  = XMMatrixTranslation(xmf3Position.x, xmf3Position.y, xmf3Position.z);
	XMStoreFloat4x4(&m_xmf4x4World, XMMatrixMultiply(mRot, mTr));
}

void CGameObject::SetWorldOrientation(const XMFLOAT3& xmf3Forward, const XMFLOAT3& xmf3Position)
{
	// row-major: 1행 right, 2행 up, 3행 forward, 4행 position
	XMVECTOR vFwd = XMVector3Normalize(XMLoadFloat3(&xmf3Forward));

	// world-up 후보. forward 와 거의 평행하면 +Z 를 보조 up 으로 사용
	XMVECTOR vWorldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	if (fabsf(XMVectorGetX(XMVector3Dot(vFwd, vWorldUp))) > 0.99f) {
		vWorldUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}
	XMVECTOR vRight = XMVector3Normalize(XMVector3Cross(vWorldUp, vFwd));
	XMVECTOR vUp    = XMVector3Normalize(XMVector3Cross(vFwd, vRight));

	XMFLOAT3 r, u, f;
	XMStoreFloat3(&r, vRight);
	XMStoreFloat3(&u, vUp);
	XMStoreFloat3(&f, vFwd);

	m_xmf4x4World._11 = r.x; m_xmf4x4World._12 = r.y; m_xmf4x4World._13 = r.z; m_xmf4x4World._14 = 0.0f;
	m_xmf4x4World._21 = u.x; m_xmf4x4World._22 = u.y; m_xmf4x4World._23 = u.z; m_xmf4x4World._24 = 0.0f;
	m_xmf4x4World._31 = f.x; m_xmf4x4World._32 = f.y; m_xmf4x4World._33 = f.z; m_xmf4x4World._34 = 0.0f;
	m_xmf4x4World._41 = xmf3Position.x;
	m_xmf4x4World._42 = xmf3Position.y;
	m_xmf4x4World._43 = xmf3Position.z;
	m_xmf4x4World._44 = 1.0f;
}

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

// CBulletObject: 플레이어 총알과 적 총알을 같은 클래스로 표현 (eTag 로 구분)
CBulletObject::CBulletObject(const XMFLOAT3& xmf3Start, const XMFLOAT3& xmf3Dir, float fSpeed,
	EObjectTag eTag)
	: m_eSceneState(static_cast<SceneState>(0))
{
	m_eTag = eTag;
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

	// 벽에 부딪히거나 단차 옆면에 닿으면 사망
	if (IsBlockedInMap(m_eSceneState, pos.x, pos.z, pos.y + 1000.0f)) {
		m_bAlive = false;
		return;
	}
	const float floorTopY = GetFloorHeightAt(m_eSceneState, pos.x, pos.z);
	if (floorTopY > pos.y) {
		m_bAlive = false;
	}
}

// CCharacter
CCharacter::CCharacter()
{
}

CCharacter::~CCharacter()
{
}

void CCharacter::TakeDamage(int n)
{
	m_nHP -= n;
	if (m_nHP <= 0) {
		m_nHP = 0;
		Kill();
	}
}

void CCharacter::StartJump(float fInitialVelocity)
{
	m_bJumping = true;
	m_fVerticalVelocity = fInitialVelocity;
}

void CCharacter::SetRifleMesh(std::shared_ptr<CMesh> pMesh)
{
	// 소총은 캐릭터와 별개의 CGameObject. Animate 가 위치 동기화.
	m_pRifle = std::make_shared<CGameObject>();
	m_pRifle->SetMesh(std::move(pMesh));
}

void CCharacter::OnHit(CGameObject* /*pOther*/)
{
	// 기본 피격: HP 1 감산
	TakeDamage(1);
}

void CCharacter::TryMoveXZ(const XMFLOAT3& xmf3Dir, float fStep, float fRadius)
{
	// X/Z 축 분리 프로브 이동. 한 축이 막혀도 다른 축은 통과시켜 벽에 미끄러지듯 이동.
	XMFLOAT3 pos = GetPosition();
	const float kFeetY = pos.y - m_xmf3AABBHalf.y;
	const float kGravity = 20.0f;

	const float dx = xmf3Dir.x * fStep;
	const float dz = xmf3Dir.z * fStep;
	bool bBlockedByStep = false;

	if (dx != 0.0f) {
		const float probeX = pos.x + dx + (dx > 0.0f ? fRadius : -fRadius);
		if (!IsBlockedInMap(m_eSceneState, probeX, pos.z, kFeetY)) {
			pos.x += dx;
		} else {
			const float nextFloor = GetFloorHeightAt(m_eSceneState, probeX, pos.z);
			if (nextFloor > kFeetY + 0.05f) bBlockedByStep = true;
		}
	}
	if (dz != 0.0f) {
		const float probeZ = pos.z + dz + (dz > 0.0f ? fRadius : -fRadius);
		if (!IsBlockedInMap(m_eSceneState, pos.x, probeZ, kFeetY)) {
			pos.z += dz;
		} else {
			const float nextFloor = GetFloorHeightAt(m_eSceneState, pos.x, probeZ);
			if (nextFloor > kFeetY + 0.05f) bBlockedByStep = true;
		}
	}

	// 단차에 막혔고 점프 가능하면 자동 점프 시작
	if (bBlockedByStep && !m_bJumping && m_fJumpCooldown <= 0.0f) {
		float stepHeight = 0.0f;
		if (dx != 0.0f) {
			const float px = pos.x + dx + (dx > 0.0f ? fRadius : -fRadius);
			const float h = GetFloorHeightAt(m_eSceneState, px, pos.z);
			if (h > stepHeight) stepHeight = h;
		}
		if (dz != 0.0f) {
			const float pz = pos.z + dz + (dz > 0.0f ? fRadius : -fRadius);
			const float h = GetFloorHeightAt(m_eSceneState, pos.x, pz);
			if (h > stepHeight) stepHeight = h;
		}
		const float jumpHRaw = stepHeight - kFeetY + 0.3f;
		const float jumpH = (jumpHRaw > 0.5f) ? jumpHRaw : 0.5f;
		m_fVerticalVelocity = sqrtf(2.0f * kGravity * jumpH);
		m_bJumping = true;
		m_fJumpCooldown = 2.0f;
	}

	if (!m_bJumping) {
		const float floorY = GetFloorHeightAt(m_eSceneState, pos.x, pos.z);
		pos.y = floorY + m_xmf3AABBHalf.y;
	}
	SetPosition(pos);
}

void CCharacter::ApplyJumpPhysics(float fTimeElapsed, float fGravity)
{
	if (!m_bJumping) return;
	m_fVerticalVelocity -= fGravity * fTimeElapsed;
	XMFLOAT3 jpos = GetPosition();
	jpos.y += m_fVerticalVelocity * fTimeElapsed;
	const float groundY = GetFloorHeightAt(m_eSceneState, jpos.x, jpos.z) + m_xmf3AABBHalf.y;
	if (jpos.y <= groundY) {
		jpos.y = groundY;
		m_bJumping = false;
		m_fVerticalVelocity = 0.0f;
	}
	SetPosition(jpos);
}

void CCharacter::TickJumpCooldown(float fTimeElapsed)
{
	if (m_fJumpCooldown > 0.0f) m_fJumpCooldown -= fTimeElapsed;
}

// CEnemyObject
namespace {
	// 사망 애니메이션 타이밍
	constexpr float kEnemyDeathPauseDuration = 0.2f;
	constexpr float kEnemyDeathTipDuration   = 0.8f;
	constexpr float kEnemyDeathTotalDuration = kEnemyDeathPauseDuration + kEnemyDeathTipDuration;
	constexpr float kEnemyDeathYDrop         = 0.7f;
}

CEnemyObject::CEnemyObject(SceneState eSceneState, unsigned int nSeed)
	: m_eAIState(EEnemyAIState::Idle_Pause)
	, m_fStateTimer(0.0f)
	, m_xmf3IdleDir{ 0.0f, 0.0f, 0.0f }
	, m_fFireCooldown(0.0f)
	, m_fAimFreeze(0.0f)
	, m_bAware(false)
	, m_rng(nSeed)
{
	m_eTag = EObjectTag::Enemy;
	m_eSceneState = eSceneState;
	m_nHP = 2;
	m_xmf3AABBHalf = XMFLOAT3(0.6f, 1.3f, 0.6f);
	m_fStateTimer = RandFloat(2.0f, 3.0f);
	m_fFireCooldown = RandFloat(2.0f, 3.0f);
}

CEnemyObject::~CEnemyObject()
{
}

float CEnemyObject::RandFloat(float a, float b)
{
	std::uniform_real_distribution<float> dist(a, b);
	return dist(m_rng);
}

XMFLOAT3 CEnemyObject::PickRandomFreeDirection()
{
	// 4방향 중 벽이 없는 방향을 무작위 선택
	const XMFLOAT3 dirs[4] = {
		{ 1.0f, 0.0f, 0.0f },
		{-1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f,-1.0f },
	};
	XMFLOAT3 pos = GetPosition();
	const float kProbe = 1.2f;
	const float kFeetY = pos.y - m_xmf3AABBHalf.y;
	XMFLOAT3 freeDirs[4];
	int nFree = 0;
	for (int i = 0; i < 4; ++i) {
		const float px = pos.x + dirs[i].x * kProbe;
		const float pz = pos.z + dirs[i].z * kProbe;
		if (!IsBlockedInMap(m_eSceneState, px, pz, kFeetY)) {
			freeDirs[nFree++] = dirs[i];
		}
	}
	if (nFree == 0) return XMFLOAT3(0.0f, 0.0f, 0.0f);
	std::uniform_int_distribution<int> dist(0, nFree - 1);
	return freeDirs[dist(m_rng)];
}

XMFLOAT3 CEnemyObject::BestFreeDirectionToward(const XMFLOAT3& towardsDir)
{
	const XMFLOAT3 dirs[4] = {
		{  1.0f, 0.0f,  0.0f },
		{ -1.0f, 0.0f,  0.0f },
		{  0.0f, 0.0f,  1.0f },
		{  0.0f, 0.0f, -1.0f },
	};
	XMFLOAT3 pos = GetPosition();
	const float kProbe = 1.2f;
	const float kFeetY = pos.y - m_xmf3AABBHalf.y;
	XMFLOAT3 bestDir{ 0.0f, 0.0f, 0.0f };
	float bestDot = -2.0f;
	for (int i = 0; i < 4; ++i) {
		const float px = pos.x + dirs[i].x * kProbe;
		const float pz = pos.z + dirs[i].z * kProbe;
		if (!IsBlockedInMap(m_eSceneState, px, pz, kFeetY)) {
			const float dot = dirs[i].x * towardsDir.x + dirs[i].z * towardsDir.z;
			if (dot > bestDot) {
				bestDot = dot;
				bestDir = dirs[i];
			}
		}
	}
	return bestDir;
}

void CEnemyObject::Animate(float fTimeElapsed)
{
	if (!m_bAlive) return;

	// 사망 애니메이션 진행 (AI/이동/사격 모두 스킵)
	if (IsDying()) {
		m_fDeathTimer -= fTimeElapsed;

		// 회전 진행률 (0=정지구간, 1=완전히 누움)
		float tipNormalized;
		if (m_fDeathTimer > kEnemyDeathTipDuration) {
			tipNormalized = 0.0f;
		}
		else if (m_fDeathTimer > 0.0f) {
			tipNormalized = 1.0f - (m_fDeathTimer / kEnemyDeathTipDuration);
		}
		else {
			tipNormalized = 1.0f;
		}

		// 사망 시점 월드행렬에 forward 축 회전 적용
		const float angleRad = XMConvertToRadians(90.0f) * tipNormalized;
		XMVECTOR vAxis = XMLoadFloat3(&m_xmf3DeathTipAxis);
		XMMATRIX matRot  = XMMatrixRotationAxis(vAxis, angleRad);
		XMMATRIX matBase = XMLoadFloat4x4(&m_xmf4x4DeathBaseWorld);
		XMVECTOR vPos = matBase.r[3];
		matBase.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMMATRIX matFinal = XMMatrixMultiply(matBase, matRot);
		// Y 하강 보정
		XMFLOAT3 pos; XMStoreFloat3(&pos, vPos);
		pos.y = m_fDeathBaseY - kEnemyDeathYDrop * tipNormalized;
		matFinal.r[3] = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);
		XMStoreFloat4x4(&m_xmf4x4World, matFinal);

		// 소총도 본체에 강체 부착된 것처럼 같이 회전·하강
		if (m_pRifle) {
			XMMATRIX matRifleLocal = XMLoadFloat4x4(&m_xmf4x4DeathRifleLocal);
			XMFLOAT4X4 xmf4x4Rifle;
			XMStoreFloat4x4(&xmf4x4Rifle, XMMatrixMultiply(matRifleLocal, matFinal));
			m_pRifle->SetWorldMatrix(xmf4x4Rifle);
		}

		if (m_fDeathTimer <= 0.0f) {
			Kill();
		}
		return;
	}

	if (!m_fnGetPlayer) return;

	// 타이머 감산
	if (m_fStateTimer > 0.0f) m_fStateTimer -= fTimeElapsed;
	if (m_fFireCooldown > 0.0f) m_fFireCooldown -= fTimeElapsed;
	if (m_fAimFreeze > 0.0f) m_fAimFreeze -= fTimeElapsed;
	TickJumpCooldown(fTimeElapsed);

	// 점프 물리
	ApplyJumpPhysics(fTimeElapsed);

	// 시야 판정 (한 번 본 적은 영구 추적)
	const XMFLOAT3 myPos = GetPosition();
	const XMFLOAT3 playerPos = m_fnGetPlayer();
	if (!m_bAware) {
		if (HasLineOfSight(m_eSceneState, myPos, playerPos, MAP_EYE_HEIGHT)) {
			m_bAware = true;
			m_eAIState = EEnemyAIState::Pursue;
		}
	}
	else {
		m_eAIState = EEnemyAIState::Pursue;
	}

	const float kSpeed = 1.8f;
	const float kStep = kSpeed * fTimeElapsed;

	// 상태별 동작
	switch (m_eAIState) {
	case EEnemyAIState::Idle_Move:
	{
		const XMFLOAT3 oldPos = GetPosition();
		TryMoveXZ(m_xmf3IdleDir, kStep);
		const XMFLOAT3 newPos = GetPosition();
		const bool bMoved = (oldPos.x != newPos.x) || (oldPos.z != newPos.z);

		if (!bMoved) {
			// 벽에 막히면 즉시 새 방향 선택
			const XMFLOAT3 newDir = PickRandomFreeDirection();
			const float newLen = sqrtf(newDir.x * newDir.x + newDir.z * newDir.z);
			if (newLen > 1e-5f) {
				m_xmf3IdleDir = newDir;
				m_fStateTimer = RandFloat(1.0f, 2.0f);
			} else {
				m_eAIState = EEnemyAIState::Idle_Pause;
				m_fStateTimer = RandFloat(0.5f, 1.0f);
			}
		} else if (m_fStateTimer <= 0.0f) {
			m_eAIState = EEnemyAIState::Idle_Pause;
			m_fStateTimer = RandFloat(2.0f, 3.0f);
		}
		break;
	}
	case EEnemyAIState::Idle_Pause:
	{
		if (m_fStateTimer <= 0.0f) {
			const XMFLOAT3 dir = PickRandomFreeDirection();
			const float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
			if (len > 1e-5f) {
				m_xmf3IdleDir = dir;
				m_eAIState = EEnemyAIState::Idle_Move;
				m_fStateTimer = RandFloat(1.0f, 2.0f);
			}
			else {
				// 모든 방향 막힘 → 잠깐 더 정지
				m_fStateTimer = RandFloat(1.0f, 2.0f);
			}
		}
		break;
	}
	case EEnemyAIState::Pursue:
	{
		// 플레이어 방향 단위 벡터 (XZ)
		XMFLOAT3 dir{ playerPos.x - myPos.x, 0.0f, playerPos.z - myPos.z };
		const float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
		if (len > 1e-5f) {
			dir.x /= len; dir.z /= len;

			// A* 경로 갱신
			m_fPathTimer -= fTimeElapsed;
			if (m_fPathTimer <= 0.0f || m_vPath.empty() || m_nPathIndex >= m_vPath.size()) {
				m_vPath     = FindPathAStar(m_eSceneState, myPos, playerPos);
				m_nPathIndex = 0;
				m_fPathTimer = kPathRecalcInterval;
			}

			// 이동 방향: A* 경로의 다음 웨이포인트
			XMFLOAT3 moveDir = dir;
			if (!m_vPath.empty() && m_nPathIndex < m_vPath.size()) {
				const XMFLOAT3& wp = m_vPath[m_nPathIndex];
				XMFLOAT3 toWp{ wp.x - myPos.x, 0.0f, wp.z - myPos.z };
				const float wpDist = sqrtf(toWp.x * toWp.x + toWp.z * toWp.z);
				if (wpDist < MAP_TILE_SIZE * 0.5f) {
					++m_nPathIndex;
					if (m_nPathIndex < m_vPath.size()) {
						const XMFLOAT3& wp2 = m_vPath[m_nPathIndex];
						XMFLOAT3 toWp2{ wp2.x - myPos.x, 0.0f, wp2.z - myPos.z };
						const float d2 = sqrtf(toWp2.x * toWp2.x + toWp2.z * toWp2.z);
						if (d2 > 1e-4f) { toWp2.x /= d2; toWp2.z /= d2; moveDir = toWp2; }
					}
				} else if (wpDist > 1e-4f) {
					toWp.x /= wpDist; toWp.z /= wpDist;
					moveDir = toWp;
				}
			}

			// facing 결정: 시야 통하면 플레이어를, 막혀 있으면 이동 방향을 향함
			const bool bLos = HasLineOfSight(m_eSceneState, myPos, playerPos, MAP_EYE_HEIGHT);
			m_xmf3FacingDir = bLos ? dir : moveDir;
			SetWorldOrientation(m_xmf3FacingDir, GetPosition());

			// 발사 직후엔 잠시 정지 (조준 연출)
			if (m_fAimFreeze <= 0.0f) {
				TryMoveXZ(moveDir, kStep);
			}

			// 사격
			if (m_fFireCooldown <= 0.0f && m_fnFire) {
				XMFLOAT3 muzzle;
				if (m_pRifle) {
					const XMFLOAT3 rpos = m_pRifle->GetPosition();
					const XMFLOAT3 rfwd = m_pRifle->GetLook();
					constexpr float kMuzzle = 1.25f;
					muzzle = XMFLOAT3{
						rpos.x + rfwd.x * kMuzzle,
						rpos.y + rfwd.y * kMuzzle,
						rpos.z + rfwd.z * kMuzzle };
				}
				else {
					muzzle = XMFLOAT3{
						myPos.x + dir.x * 0.8f,
						myPos.y + 0.2f,
						myPos.z + dir.z * 0.8f };
				}
				// 플레이어 몸통 중앙을 목표로 발사 방향 재계산 (머리 위로 빗나가지 않도록)
				const float kPlayerBodyCenterY = playerPos.y - MAP_EYE_HEIGHT + 1.3f;
				XMFLOAT3 fireDir{ playerPos.x - muzzle.x,
				                  kPlayerBodyCenterY - muzzle.y,
				                  playerPos.z - muzzle.z };
				const float fl = sqrtf(fireDir.x * fireDir.x + fireDir.y * fireDir.y + fireDir.z * fireDir.z);
				if (fl > 1e-5f) { fireDir.x /= fl; fireDir.y /= fl; fireDir.z /= fl; }
				else            { fireDir = dir; }
				m_fnFire(muzzle, fireDir);
				m_fFireCooldown = RandFloat(2.0f, 3.0f);
				m_fAimFreeze = 0.3f;
			}
		}
		break;
	}
	}

	// 머리 위 마커 위치 동기화
	if (m_pMarker) {
		XMFLOAT3 markerPos = GetPosition();
		markerPos.y += m_xmf3AABBHalf.y + 7.0f;
		m_pMarker->SetPosition(markerPos);
	}

	// 소총 위치 동기화 (본체 facing 의 우측 어깨, 가슴 높이)
	if (m_pRifle) {
		const XMFLOAT3 myPos = GetPosition();
		const XMFLOAT3& facing = m_xmf3FacingDir;
		const XMFLOAT3 right{ -facing.z, 0.0f, facing.x };
		const XMFLOAT3 riflePos{
			myPos.x + right.x * 1.0f + facing.x * 0.3f,
			myPos.y - m_xmf3AABBHalf.y + 1.6f,
			myPos.z + right.z * 1.0f + facing.z * 0.3f };
		m_pRifle->SetWorldOrientation(facing, riflePos);
	}
}

void CEnemyObject::OnHit(CGameObject* pOther)
{
	// 사망 애니메이션 도입에 따라 즉시 Kill 대신 사망 타이머 시작
	if (IsDying()) return;
	if (m_nHP > 0) m_nHP -= 1;
	if (m_nHP <= 0) {
		m_nHP = 0;
		// 사망 애니메이션 시작 (Kill 호출 안 함 → 렌더/Animate 유지)
		m_fDeathTimer          = kEnemyDeathTotalDuration;
		m_xmf4x4DeathBaseWorld = m_xmf4x4World;
		m_xmf3DeathTipAxis     = GetLook();
		m_fDeathBaseY          = GetPosition().y;
		// 사망 시점 소총의 본체-로컬 행렬 캐시
		if (m_pRifle) {
			XMMATRIX matRifleWorld = XMLoadFloat4x4(&m_pRifle->GetWorldMatrixRef());
			XMMATRIX matBodyWorld  = XMLoadFloat4x4(&m_xmf4x4World);
			XMVECTOR vDet;
			XMMATRIX matBodyInv = XMMatrixInverse(&vDet, matBodyWorld);
			XMStoreFloat4x4(&m_xmf4x4DeathRifleLocal,
				XMMatrixMultiply(matRifleWorld, matBodyInv));
		}
		m_bMarkerVisible = false;
	}
}

void CEnemyObject::SetMarkerMesh(std::shared_ptr<CMesh> pMesh)
{
	// 마커는 적과 별개의 객체. Animate 에서 위치 동기화.
	m_pMarker = std::make_shared<CGameObject>();
	m_pMarker->SetMesh(std::move(pMesh));
}
