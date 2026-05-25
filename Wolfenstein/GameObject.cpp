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
	// [Claude] 이전: 여기서 m_pShader->ReleaseShaderVariables() 를 호출했으나
	// m_pShader 는 여러 GameObject 가 공유하는 shared_ptr 이다 (예: m_pHudShader
	// 는 십자선/라이프 바/카운트 점/WIN 글자 30+ 개가 공유). 호출이 base 에선
	// no-op 라 현재는 무해하지만, 후속 셰이더 서브클래스가 일회용 자원 해제를
	// 여기 구현하면 동일 셰이더에 대해 30+ 번 호출되어 더블-프리 위험이 생긴다.
	// 셰이더 자체 자원은 shared_ptr 의 마지막 참조가 풀릴 때 CShader::~CShader
	// 가 책임지면 충분하므로 여기서는 호출하지 않는다.
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
	// 정점 버퍼를 위한 업로드 버퍼를 소멸시킨다.
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

	// (로컬 행렬 * 부모 행렬) 의 전치를 32-bit constants 슬롯 0 에 업로드한다.
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

	// 객체의 정보를 셰이더 변수(cBuffer)에 갱신한다.
	UpdateShaderVariables(pd3dCommandList);

	// 게임 객체의 월드 변환 행렬을 셰이더의 상수 버퍼로 복사(갱신)한다.
	if (m_pShader) m_pShader->Render(pd3dCommandList, pCamera);

	// 게임 객체의 메시가 설정되어 있으면 메시를 렌더링한다.
	if (m_pMesh) m_pMesh->Render(pd3dCommandList);
}

void CGameObject::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
}

void CGameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4World;
	XMStoreFloat4x4(&xmf4x4World, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4World)));
	// 객체의 월드 변환 행렬의 전치 행렬을 루트 상수로 셰이더 변수(cBuffer)에 갱신한다.
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

void CGameObject::SetWorldOrientation(const XMFLOAT3& xmf3Forward, const XMFLOAT3& xmf3Position)
{
	// row-major: 1행 = right, 2행 = up, 3행 = forward, 4행 = position
	XMVECTOR vFwd = XMVector3Normalize(XMLoadFloat3(&xmf3Forward));

	// world-up 후보. forward 가 +Y 또는 -Y 와 거의 평행하면 +Z 를 보조 up 으로 사용.
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
// CCharacter
// ==========================================================================
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
	// 소총은 캐릭터와 별도의 CGameObject ? 서브클래스 Animate 에서 매 프레임
	// 위치/회전을 동기화한다. 메시만 부착하고 트랜스폼은 외부에서 세팅.
	m_pRifle = std::make_shared<CGameObject>();
	m_pRifle->SetMesh(std::move(pMesh));
}

void CCharacter::OnHit(CGameObject* /*pOther*/)
{
	// 기본 피격 처리: 체력 1 감산. 서브클래스가 추가 연출(반동/사망 음향) 을
	// 입히려면 오버라이드.
	TakeDamage(1);
}

void CCharacter::TryMoveXZ(const XMFLOAT3& xmf3Dir, float fStep, float fRadius)
{
	// X/Z 축 분리 프로브 이동 ? 한 축이 벽에 막혀도 다른 축은 통과시켜 벽에
	// 비스듬히 다가갈 때 미끄러지듯 이동한다. 단차에 막히면 자동 점프 시작.
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

// ==========================================================================
// CEnemyObject
// ==========================================================================
namespace {
	// [Claude] 적 사망 애니메이션 타이밍/형상 상수.
	// 정지 0.2초 후 0.8초 동안 forward 축 기준 90° 옆으로 회전. 회전 완료 시
	// AABB half (0.6, 1.3, 0.6) 의 수직 half 가 1.3 → 0.6 으로 줄어드므로
	// 부유 방지를 위해 Y 를 0.7 만큼 lerp 로 하강시킨다.
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
	m_nHP = 2; // 적 기본 체력
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

void CEnemyObject::Animate(float fTimeElapsed)
{
	if (!m_bAlive) return;

	// [Claude] 사망 애니메이션 ? AI/이동/사격/소총/마커 동기화 모두 스킵하고
	// 베이스 월드행렬에 사망 시점 forward 축 기준 회전을 적용한다. 0.2초 정지
	// 구간 후 0.8초간 0°→90° 보간, 동시에 Y 도 0→-0.7 만큼 lerp 하여 바닥에
	// 자연스럽게 안착시킨다. 만료 시 Kill() 로 PruneDead 회수.
	if (IsDying()) {
		m_fDeathTimer -= fTimeElapsed;

		// 회전 진행률 (0=시작/정지구간, 1=완전히 누움).
		float tipNormalized;
		if (m_fDeathTimer > kEnemyDeathTipDuration) {
			tipNormalized = 0.0f;  // 아직 정지 구간
		}
		else if (m_fDeathTimer > 0.0f) {
			tipNormalized = 1.0f - (m_fDeathTimer / kEnemyDeathTipDuration);
		}
		else {
			tipNormalized = 1.0f;
		}

		// 사망 시점 월드행렬에 forward 축 회전 적용 (translation 분리 후 재결합).
		const float angleRad = XMConvertToRadians(90.0f) * tipNormalized;
		XMVECTOR vAxis = XMLoadFloat3(&m_xmf3DeathTipAxis);
		XMMATRIX matRot  = XMMatrixRotationAxis(vAxis, angleRad);
		XMMATRIX matBase = XMLoadFloat4x4(&m_xmf4x4DeathBaseWorld);
		XMVECTOR vPos = matBase.r[3];
		matBase.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMMATRIX matFinal = XMMatrixMultiply(matBase, matRot);
		// Y 하강 보정 + translation 복원.
		XMFLOAT3 pos; XMStoreFloat3(&pos, vPos);
		pos.y = m_fDeathBaseY - kEnemyDeathYDrop * tipNormalized;
		matFinal.r[3] = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);
		XMStoreFloat4x4(&m_xmf4x4World, matFinal);

		// [Claude] 소총도 본체와 함께 회전·하강하도록 강체 부착 효과 적용.
		// OnHit 에서 캐시해 둔 본체-로컬 행렬을 새 본체 월드행렬에 곱해 소총의
		// 월드행렬을 직접 갱신한다 (Animate 후반의 SetWorldOrientation 동기화는
		// IsDying() return 으로 스킵되므로 여기서 처리해야 한다).
		if (m_pRifle) {
			XMMATRIX matRifleLocal = XMLoadFloat4x4(&m_xmf4x4DeathRifleLocal);
			XMFLOAT4X4 xmf4x4Rifle;
			XMStoreFloat4x4(&xmf4x4Rifle, XMMatrixMultiply(matRifleLocal, matFinal));
			m_pRifle->SetWorldMatrix(xmf4x4Rifle);
		}

		if (m_fDeathTimer <= 0.0f) {
			Kill();   // 다음 PruneDead 에서 객체 회수
		}
		return;
	}

	if (!m_fnGetPlayer) return; // 콜백 주입 전이면 아무것도 하지 않음

	// 1. 타이머 감산
	if (m_fStateTimer > 0.0f) m_fStateTimer -= fTimeElapsed;
	if (m_fFireCooldown > 0.0f) m_fFireCooldown -= fTimeElapsed;
	if (m_fAimFreeze > 0.0f) m_fAimFreeze -= fTimeElapsed;
	TickJumpCooldown(fTimeElapsed);

	// 1b. 점프 물리는 CCharacter 가 통합 처리 (점프 중일 때만 동작).
	ApplyJumpPhysics(fTimeElapsed);

	// 2. 시야 판정 ? 한 번 본 적은 영구 추적 (사용자 결정: 영구 추적)
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
		// 플레이어 방향 단위 벡터 (XZ 만 사용 ? 발사 방향은 수평 기준).
		XMFLOAT3 dir{ playerPos.x - myPos.x, 0.0f, playerPos.z - myPos.z };
		const float len = sqrtf(dir.x * dir.x + dir.z * dir.z);
		if (len > 1e-5f) {
			dir.x /= len; dir.z /= len;

			// A* 경로 갱신: 타이머 만료 또는 경로 소진 시 재계산.
			m_fPathTimer -= fTimeElapsed;
			if (m_fPathTimer <= 0.0f || m_vPath.empty() || m_nPathIndex >= m_vPath.size()) {
				const float kFeetY = myPos.y - m_xmf3AABBHalf.y;
				m_vPath     = FindPathAStar(m_eSceneState, myPos, playerPos);
				m_nPathIndex = 0;
				m_fPathTimer = kPathRecalcInterval;
			}

			// 이동 방향: A* 경로가 있으면 다음 웨이포인트, 없으면 직선 추적으로 폴백.
			XMFLOAT3 moveDir = dir;  // 기본값: 플레이어 직선 방향
			if (!m_vPath.empty() && m_nPathIndex < m_vPath.size()) {
				const XMFLOAT3& wp = m_vPath[m_nPathIndex];
				XMFLOAT3 toWp{ wp.x - myPos.x, 0.0f, wp.z - myPos.z };
				const float wpDist = sqrtf(toWp.x * toWp.x + toWp.z * toWp.z);
				// 웨이포인트 절반 셀 이내 진입 시 다음으로 전진
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

			// facing 결정: 매 프레임 LOS 재평가. 시야가 열려 있으면 플레이어를 향하고,
			// 벽 등으로 막혀 있으면 이번 프레임의 실제 이동 방향(= moveDir, A* 다음
			// 웨이포인트 방향) 을 향한다. 본체와 소총 모두 같은 방향을 공유해 일관성 유지.
			const bool bLos = HasLineOfSight(m_eSceneState, myPos, playerPos, MAP_EYE_HEIGHT);
			m_xmf3FacingDir = bLos ? dir : moveDir;
			SetWorldOrientation(m_xmf3FacingDir, GetPosition());

			// 발사 직후 m_fAimFreeze 동안은 정지 (발사 연출).
			if (m_fAimFreeze <= 0.0f) {
				TryMoveXZ(moveDir, kStep);
			}

			// 발사 쿨다운이 끝나면 총알 발사. muzzle 은 소총 총구(없으면 가슴 높이 폴백)에서
			// 출발하지만, 총알 방향은 항상 muzzle → 플레이어 몸통 중앙으로 재계산해 오프셋 때문에
			// 비껴 지나가지 않게 한다. playerPos.y 는 시점(MAP_EYE_HEIGHT) 이지만 모델 AABB
			// 는 그 아래(중심 Y = playerPos.y - MAP_EYE_HEIGHT + 1.3) 에 있으므로 머리 위로
			// 빗나가지 않도록 몸통 중앙 Y 를 목표로 삼는다.
			if (m_fFireCooldown <= 0.0f && m_fnFire) {
				XMFLOAT3 muzzle;
				if (m_pRifle) {
					const XMFLOAT3 rpos = m_pRifle->GetPosition();
					const XMFLOAT3 rfwd = m_pRifle->GetLook(); // +Z = 총구 방향
					constexpr float kMuzzle = 1.25f; // 메시 깊이 2.4 의 절반 + 약간 (플레이어와 동일)
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

	// 머리 위 마커 기둥 위치 동기화. 가시 여부와 무관하게 매 프레임 갱신해
	// 토글 시 즉시 올바른 위치에 나타나도록 한다. (Animate 마지막에 GetPosition()
	// 으로 최신 좌표를 읽은 뒤 마커 중심을 머리보다 살짝 위로 끌어올린다)
	if (m_pMarker) {
		XMFLOAT3 markerPos = GetPosition();
		// [Claude] 마커 메시 높이가 4.0 → 14.0 으로 증가했으므로 절반(=7.0)을 머리 위로
		// 올린다. 결과: 마커 바닥이 머리 바로 위, 마커 윗부분이 ~14 m 상공까지 솟아
		// 벽(높이 8) 위로도 노란 막대가 보인다.
		markerPos.y += m_xmf3AABBHalf.y + 7.0f;
		m_pMarker->SetPosition(markerPos);
	}

	// 오른손 소총 transform 동기화. 본체와 동일한 m_xmf3FacingDir 을 forward 로 사용해
	// (Pursue 진입 전엔 기본 +Z, Pursue 중엔 LOS 면 플레이어 / 막히면 이동 방향) 그 우측
	// 0.85, forward 0.4 떨어진 가슴 높이에 배치한다. 적이 보이면 총도 같이 보임 ?
	// 가시성 토글 없이 항상 위치만 동기화.
	if (m_pRifle) {
		const XMFLOAT3 myPos = GetPosition();
		const XMFLOAT3& facing = m_xmf3FacingDir;
		// facing 의 우측 (월드 +Y 기준 외적: (facing × up) 는 facing 의 우측)
		const XMFLOAT3 right{ -facing.z, 0.0f, facing.x };
		const XMFLOAT3 riflePos{
			myPos.x + right.x * 1.0f + facing.x * 0.3f,
			myPos.y - m_xmf3AABBHalf.y + 1.6f, // 가슴 높이 (몸 바닥 + 1.6)
			myPos.z + right.z * 1.0f + facing.z * 0.3f };
		m_pRifle->SetWorldOrientation(facing, riflePos); // +Z = 총구 방향 = 본체 facing
	}
}

void CEnemyObject::OnHit(CGameObject* pOther)
{
	// [Claude] 사망 애니메이션 도입에 따라 CCharacter::OnHit 의 자동 Kill 경로를
	// 우회한다. HP 만 직접 감산하고, 치사 시 m_bAlive 는 유지한 채 사망 타이머만
	// 시작 ? Animate 분기에서 옆으로 누운 후 만료 시점에 Kill() 호출.
	if (IsDying()) return;            // 이미 죽는 중이면 추가 피격 무시
	if (m_nHP > 0) m_nHP -= 1;
	if (m_nHP <= 0) {
		m_nHP = 0;
		// 사망 애니메이션 시작 ? Kill() 호출하지 않아 렌더/애니메이트 유지.
		m_fDeathTimer          = kEnemyDeathTotalDuration;
		m_xmf4x4DeathBaseWorld = m_xmf4x4World;
		m_xmf3DeathTipAxis     = GetLook();      // 사망 시점 forward 축
		m_fDeathBaseY          = GetPosition().y;
		// [Claude] 사망 시점 소총의 본체-로컬 행렬 캐시: R_local = R_world * inverse(B_world).
		// 이후 사망 애니메이션 매 프레임마다 R_world(t) = R_local * B_world(t) 로 복원해
		// 소총이 본체에 강체 부착된 것처럼 같이 회전·하강하도록 한다.
		if (m_pRifle) {
			XMMATRIX matRifleWorld = XMLoadFloat4x4(&m_pRifle->GetWorldMatrixRef());
			XMMATRIX matBodyWorld  = XMLoadFloat4x4(&m_xmf4x4World);
			XMVECTOR vDet;
			XMMATRIX matBodyInv = XMMatrixInverse(&vDet, matBodyWorld);
			XMStoreFloat4x4(&m_xmf4x4DeathRifleLocal,
				XMMatrixMultiply(matRifleWorld, matBodyInv));
		}
		// 머리 위 마커 즉시 숨김 (사망 중인 적은 카운트/마커 대상 제외).
		m_bMarkerVisible       = false;
	}
}

void CEnemyObject::SetMarkerMesh(std::shared_ptr<CMesh> pMesh)
{
	// 마커는 적과 별도의 CGameObject ? Animate 에서 위치만 적 머리 위로 동기화.
	// 가시성은 m_bMarkerVisible 플래그로 토글되며 Scene::Render 가 분기 처리.
	m_pMarker = std::make_shared<CGameObject>();
	m_pMarker->SetMesh(std::move(pMesh));
}
