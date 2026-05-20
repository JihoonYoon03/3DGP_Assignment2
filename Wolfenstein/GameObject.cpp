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
// eTag 에 EObjectTag::Bullet 또는 EObjectTag::EnemyBullet 을 전달하여 같은
// 클래스 본체로 플레이어 총알과 적 총알을 모두 표현한다. 충돌 페어 등록은
// Scene::AnimateObjects 에서 태그 단위로 분리된다.
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

// ==========================================================================
// CEnemyObject
// ==========================================================================
CEnemyObject::CEnemyObject(SceneState eSceneState, unsigned int nSeed)
	: m_eAIState(EEnemyAIState::Idle_Pause)
	, m_fStateTimer(0.0f)
	, m_xmf3IdleDir{ 0.0f, 0.0f, 0.0f }
	, m_fFireCooldown(0.0f)
	, m_fAimFreeze(0.0f)
	, m_nHP(2)
	, m_bAware(false)
	, m_eSceneState(eSceneState)
	, m_rng(nSeed)
{
	m_eTag = EObjectTag::Enemy;
	// AABB 는 플레이어 모델과 동일한 크기로 잡아 충돌이 자연스럽게 처리되게 한다.
	m_xmf3AABBHalf = XMFLOAT3(0.6f, 1.3f, 0.6f);
	// 초기엔 잠시 정지하며 시작 (스폰 직후 즉시 추격 시야가 트이는 것을 약간 늦춤).
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
	// 4방향(+X, -X, +Z, -Z) 중 막힌 곳을 제외하고 랜덤 선택.
	// 인접 셀의 벽/높은 단차를 IsBlockedInMap 으로 판정한다.
	const XMFLOAT3 dirs[4] = {
		{ 1.0f, 0.0f, 0.0f },
		{-1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.0f,-1.0f },
	};
	XMFLOAT3 pos = GetPosition();
	const float kProbe = 1.2f; // AABB half + 약간 여유로 다음 셀의 시작을 확인
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

void CEnemyObject::TryMoveXZ(const XMFLOAT3& xmf3Dir, float fStep)
{
	XMFLOAT3 pos = GetPosition();
	const float kRadius = 0.7f;
	const float kFeetY = pos.y - m_xmf3AABBHalf.y;
	const float kGravity = 20.0f;

	const float dx = xmf3Dir.x * fStep;
	const float dz = xmf3Dir.z * fStep;
	bool bBlockedByStep = false;

	if (dx != 0.0f) {
		const float probeX = pos.x + dx + (dx > 0.0f ? kRadius : -kRadius);
		if (!IsBlockedInMap(m_eSceneState, probeX, pos.z, kFeetY)) {
			pos.x += dx;
		} else {
			const float nextFloor = GetFloorHeightAt(m_eSceneState, probeX, pos.z);
			if (nextFloor > kFeetY + 0.05f) bBlockedByStep = true;
		}
	}
	if (dz != 0.0f) {
		const float probeZ = pos.z + dz + (dz > 0.0f ? kRadius : -kRadius);
		if (!IsBlockedInMap(m_eSceneState, pos.x, probeZ, kFeetY)) {
			pos.z += dz;
		} else {
			const float nextFloor = GetFloorHeightAt(m_eSceneState, pos.x, probeZ);
			if (nextFloor > kFeetY + 0.05f) bBlockedByStep = true;
		}
	}

	// 단차에 막혔고 아직 점프 중이 아닐 때만 점프 시작.
	if (bBlockedByStep && !m_bJumping && m_fJumpCooldown <= 0.0f) {
		float stepHeight = 0.0f;
		if (dx != 0.0f) {
			const float px = pos.x + dx + (dx > 0.0f ? kRadius : -kRadius);
			const float h = GetFloorHeightAt(m_eSceneState, px, pos.z);
			if (h > stepHeight) stepHeight = h;
		}
		if (dz != 0.0f) {
			const float pz = pos.z + dz + (dz > 0.0f ? kRadius : -kRadius);
			const float h = GetFloorHeightAt(m_eSceneState, pos.x, pz);
			if (h > stepHeight) stepHeight = h;
		}
		const float jumpHRaw = stepHeight - kFeetY + 0.3f;
		const float jumpH = (jumpHRaw > 0.5f) ? jumpHRaw : 0.5f;
		m_fVerticalVelocity = sqrtf(2.0f * kGravity * jumpH);
		m_bJumping = true;
		m_fJumpCooldown = 2.0f;
	}

	// 점프 중이 아닐 때만 Y 를 바닥에 스냅.
	if (!m_bJumping) {
		const float floorY = GetFloorHeightAt(m_eSceneState, pos.x, pos.z);
		pos.y = floorY + m_xmf3AABBHalf.y;
	}
	SetPosition(pos);
}

void CEnemyObject::Animate(float fTimeElapsed)
{
	if (!m_bAlive) return;
	if (!m_fnGetPlayer) return; // 콜백 주입 전이면 아무것도 하지 않음

	// 1. 타이머 감산
	if (m_fStateTimer > 0.0f) m_fStateTimer -= fTimeElapsed;
	if (m_fFireCooldown > 0.0f) m_fFireCooldown -= fTimeElapsed;
	if (m_fAimFreeze > 0.0f) m_fAimFreeze -= fTimeElapsed;
	if (m_fJumpCooldown > 0.0f) m_fJumpCooldown -= fTimeElapsed;

	// 1b. 점프 물리 (상태머신과 독립적으로 매 프레임 처리)
	if (m_bJumping) {
		const float kGravity = 20.0f;
		m_fVerticalVelocity -= kGravity * fTimeElapsed;
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

	// 2. 시야 판정 — 한 번 본 적은 영구 추적 (사용자 결정: 영구 추적)
	const XMFLOAT3 myPos = GetPosition();
	const XMFLOAT3 playerPos = m_fnGetPlayer();
	if (!m_bAware) {
		// eyeY 는 적/플레이어 모두에게 동일하게 적용하기 위해 MAP_EYE_HEIGHT 사용.
		// 단차가 최대 3 * STEP_H = 2.1 로 eyeY=3.5 보다 낮아 실질적으론 W(벽)만 시야를 가린다.
		if (HasLineOfSight(m_eSceneState, myPos, playerPos, MAP_EYE_HEIGHT)) {
			m_bAware = true;
			m_eAIState = EEnemyAIState::Pursue;
		}
	}
	else {
		m_eAIState = EEnemyAIState::Pursue;
	}

	const float kSpeed = 1.8f;             // 플레이어 6.0 * 0.3
	const float kStep = kSpeed * fTimeElapsed;

	// 3. 상태별 동작
	switch (m_eAIState) {
	case EEnemyAIState::Idle_Move:
	{
		const XMFLOAT3 oldPos = GetPosition();
		TryMoveXZ(m_xmf3IdleDir, kStep);
		const XMFLOAT3 newPos = GetPosition();
		const bool bMoved = (oldPos.x != newPos.x) || (oldPos.z != newPos.z);

		if (!bMoved) {
			// 벽 충돌: 즉시 새 방향 선택(긴 정지 없이 순찰 지속).
			const XMFLOAT3 newDir = PickRandomFreeDirection();
			const float newLen = sqrtf(newDir.x * newDir.x + newDir.z * newDir.z);
			if (newLen > 1e-5f) {
				m_xmf3IdleDir = newDir;
				m_fStateTimer = RandFloat(1.0f, 2.0f);
				// Idle_Move 유지, 방향만 변경
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
			// 4방향 중 벽 없는 방향을 무작위 선택해 패트롤 재개.
			const XMFLOAT3 dir = PickRandomFreeDirection();
			const float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
			if (len > 1e-5f) {
				m_xmf3IdleDir = dir;
				m_eAIState = EEnemyAIState::Idle_Move;
				m_fStateTimer = RandFloat(1.0f, 2.0f);
			}
			else {
				// 모든 방향이 막혀 있으면 잠깐 더 정지 후 재시도.
				m_fStateTimer = RandFloat(1.0f, 2.0f);
			}
		}
		break;
	}
	case EEnemyAIState::Pursue:
	{
		// 플레이어 방향 단위 벡터 (XZ 만 사용 — 총알은 수평으로 비행).
		XMFLOAT3 dir{ playerPos.x - myPos.x, 0.0f, playerPos.z - myPos.z };
		const float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
		if (len > 1e-5f) {
			dir.x /= len; dir.z /= len;

			// 발사 직후 m_fAimFreeze 동안은 멈춤 (사용자 결정: 발사 시 잠시 멈춤).
			if (m_fAimFreeze <= 0.0f) {
				const XMFLOAT3 prePos = GetPosition();
				TryMoveXZ(dir, kStep);
				const XMFLOAT3 postPos = GetPosition();
				const bool bStuck = (fabsf(postPos.x - prePos.x) < 1e-4f &&
				                     fabsf(postPos.z - prePos.z) < 1e-4f);
				if (bStuck) {
					// 완전히 막혔을 때 플레이어 방향에 가장 가까운 빈 방향으로 우회.
					const XMFLOAT3 avoidDir = BestFreeDirectionToward(dir);
					const float avoidLen = sqrtf(avoidDir.x * avoidDir.x + avoidDir.z * avoidDir.z);
					if (avoidLen > 1e-5f) TryMoveXZ(avoidDir, kStep);
				}
			}

			// 발사 쿨다운이 끝나면 총알 발사.
			if (m_fFireCooldown <= 0.0f && m_fnFire) {
				const XMFLOAT3 muzzle{
					myPos.x + dir.x * 0.8f,
					myPos.y + 0.2f,
					myPos.z + dir.z * 0.8f };
				m_fnFire(muzzle, dir);
				m_fFireCooldown = RandFloat(2.0f, 3.0f);
				m_fAimFreeze = 0.3f;
			}
		}
		break;
	}
	}
}

void CEnemyObject::OnHit(CGameObject* /*pOther*/)
{
	// Scene::AnimateObjects 의 충돌 콜백이 b->OnHit(a) 형태로 호출한다.
	// 플레이어 총알(a) 은 콜백 안에서 이미 Kill() 되므로 여기서는 체력만 감산.
	--m_nHP;
	if (m_nHP <= 0) {
		Kill();
	}
}
