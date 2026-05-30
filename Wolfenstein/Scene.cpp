#include "stdafx.h"
#include "Scene.h"
#include "Shader.h"
#include "Camera.h"
#include "Landing.h"
#include "Maps.h"
#include "GameObject.h"
#include "Collision.h"

CScene::CScene()
{
}

ID3D12RootSignature* CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;

	D3D12_ROOT_PARAMETER pd3dRootParameters[4];
	// b0: 월드 변환 행렬 (VS 전용)
	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[0].Constants.Num32BitValues = 16;
	pd3dRootParameters[0].Constants.ShaderRegister = 0;
	pd3dRootParameters[0].Constants.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	// b1: 뷰+투영 행렬 (VS 전용)
	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[1].Constants.Num32BitValues = 32;
	pd3dRootParameters[1].Constants.ShaderRegister = 1;
	pd3dRootParameters[1].Constants.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	// b2: 라이트 정보 (PS 전용)
	pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[2].Constants.Num32BitValues = 12;
	pd3dRootParameters[2].Constants.ShaderRegister = 2;
	pd3dRootParameters[2].Constants.RegisterSpace = 0;
	pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// b3: 피격 비네트 강도 (PS 전용)
	pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[3].Constants.Num32BitValues = 1;
	pd3dRootParameters[3].Constants.ShaderRegister = 3;
	pd3dRootParameters[3].Constants.RegisterSpace = 0;
	pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
	::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
	d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
	d3dRootSignatureDesc.pParameters = pd3dRootParameters;
	d3dRootSignatureDesc.NumStaticSamplers = 0;
	d3dRootSignatureDesc.pStaticSamplers = NULL;
	d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;

	if (SUCCEEDED(::D3D12SerializeRootSignature(
		&d3dRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&pd3dSignatureBlob,
		&pd3dErrorBlob
	)))
	{
		pd3dDevice->CreateRootSignature(
			0, pd3dSignatureBlob->GetBufferPointer(),
			pd3dSignatureBlob->GetBufferSize(),
			IID_PPV_ARGS(&pd3dGraphicsRootSignature));
	}
	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
	if (pd3dErrorBlob) pd3dErrorBlob->Release();

	return pd3dGraphicsRootSignature;
}


ID3D12RootSignature* CScene::GetGraphicsRootSignature()
{
	return m_pd3dGraphicsRootSignature.Get();
}

void CScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);

	// 각 씬마다 별도의 셰이더 인스턴스
	m_vShaders.resize(static_cast<size_t>(SceneState::COUNT));
	for (CObjectsShader& shader : m_vShaders) {
		shader.CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature.Get());
	}

	// 메인화면 오브젝트
	{
		std::vector<std::shared_ptr<CGameObject>> vLandingObjects;
		BuildLandingObjects(pd3dDevice, pd3dCommandList, vLandingObjects, m_pStartButton);
		m_vShaders[static_cast<size_t>(SceneState::LANDING)].SetObjects(std::move(vLandingObjects));
	}

	// 맵 1
	{
		std::vector<std::shared_ptr<CGameObject>> vMap1Objects;
		BuildMap1Objects(pd3dDevice, pd3dCommandList, vMap1Objects);
		m_vShaders[static_cast<size_t>(SceneState::MAP1)].SetObjects(std::move(vMap1Objects));
	}

	// 맵 2
	{
		std::vector<std::shared_ptr<CGameObject>> vMap2Objects;
		BuildMap2Objects(pd3dDevice, pd3dCommandList, vMap2Objects);
		m_vShaders[static_cast<size_t>(SceneState::MAP2)].SetObjects(std::move(vMap2Objects));
	}
}

void CScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature) m_pd3dGraphicsRootSignature.Reset();

	for (CObjectsShader& shader : m_vShaders) {
		shader.ReleaseShaderVariables();
		shader.ReleaseObjects();
	}

	m_vShaders.clear();

	m_pPlayerModel.reset();
}


bool CScene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	return(false);
}

bool CScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	return(false);
}

bool CScene::ProcessInput(UCHAR* pKeysBuffer)
{
	return(false);
}

void CScene::AnimateObjects(float fTimeElapsed)
{
	// 맵 선택 미니어처 회전 각도 갱신
	m_fMiniatureAngle += fTimeElapsed * 0.6f;

	// 현재 활성 씬의 객체만 애니메이션
	size_t idx = static_cast<size_t>(m_eCurrentState);
	if (idx < m_vShaders.size()) {
		m_vShaders[idx].AnimateObjects(fTimeElapsed);

		// 게임플레이 씬에서만 충돌 검사
		if (m_eCurrentState == SceneState::MAP1 || m_eCurrentState == SceneState::MAP2) {
			// 플레이어 총알 - 적
			Collision::CheckCollisions(
				m_vShaders[idx].GetObjects(),
				EObjectTag::Bullet, EObjectTag::Enemy,
				[](CGameObject* a, CGameObject* b) {
					// 사망 애니메이션 중인 적은 무적
					auto* pEnemy = static_cast<CEnemyObject*>(b);
					if (pEnemy->IsDying()) return;
					a->Kill();
					b->OnHit(a);
				});

			// 적 총알 - 플레이어
			if (m_pPlayerModel) {
				for (auto& pObj : m_vShaders[idx].GetObjects()) {
					if (!pObj || !pObj->IsAlive()) continue;
					if (pObj->GetTag() != EObjectTag::EnemyBullet) continue;
					if (Collision::AABBOverlap(*pObj, *m_pPlayerModel)) {
						pObj->Kill();
						if (m_fnOnPlayerHit) m_fnOnPlayerHit();
					}
				}
			}

			m_vShaders[idx].PruneDead();
		}
	}
}

void CScene::ResetGameplayState()
{
	for (size_t i : { static_cast<size_t>(SceneState::MAP1), static_cast<size_t>(SceneState::MAP2) }) {
		if (i >= m_vShaders.size()) continue;
		const auto& vObjs = m_vShaders[i].GetObjects();
		for (auto& pObj : vObjs) {
			if (!pObj) continue;
			const EObjectTag tag = pObj->GetTag();
			if (tag == EObjectTag::Bullet || tag == EObjectTag::EnemyBullet || tag == EObjectTag::Enemy) {
				pObj->Kill();
			}
		}
		m_vShaders[i].PruneDead();
	}
}

int CScene::CountAliveEnemies() const
{
	if (m_eCurrentState != SceneState::MAP1 && m_eCurrentState != SceneState::MAP2) return 0;
	const size_t idx = static_cast<size_t>(m_eCurrentState);
	if (idx >= m_vShaders.size()) return 0;
	int n = 0;
	for (const auto& p : m_vShaders[idx].GetObjects()) {
		if (!p || !p->IsAlive()) continue;
		if (p->GetTag() != EObjectTag::Enemy) continue;
		// 사망 중인 적은 카운트 제외
		if (static_cast<const CEnemyObject*>(p.get())->IsDying()) continue;
		++n;
	}
	return n;
}

std::vector<std::pair<XMFLOAT3, XMFLOAT3>> CScene::GetAliveEnemyAABBs() const
{
	std::vector<std::pair<XMFLOAT3, XMFLOAT3>> out;

	if (m_eCurrentState != SceneState::MAP1 && m_eCurrentState != SceneState::MAP2) return out;

	const size_t idx = static_cast<size_t>(m_eCurrentState);

	if (idx >= m_vShaders.size()) return out;

	for (const auto& p : m_vShaders[idx].GetObjects()) {
		if (!p || !p->IsAlive()) continue;
		if (p->GetTag() != EObjectTag::Enemy) continue;
		if (static_cast<const CEnemyObject*>(p.get())->IsDying()) continue;

		const XMFLOAT3 c = const_cast<CGameObject*>(p.get())->GetPosition();
		const XMFLOAT3 h = p->GetAABBHalf();
		out.emplace_back(c, h);
	}

	return out;
}

void CScene::SetEnemyMarkersVisible(bool bVisible)
{
	if (m_eCurrentState != SceneState::MAP1 && m_eCurrentState != SceneState::MAP2) return;
	const size_t idx = static_cast<size_t>(m_eCurrentState);
	if (idx >= m_vShaders.size()) return;
	for (auto& p : m_vShaders[idx].GetObjects()) {
		if (!p || !p->IsAlive()) continue;
		if (p->GetTag() != EObjectTag::Enemy) continue;
		auto* pEnemy = static_cast<CEnemyObject*>(p.get());
		pEnemy->SetMarkerVisible(bVisible);
	}
}

namespace {
	// 맵 선택 화면의 미로 미니어처 배치용 상수
	constexpr float kMiniOffsetX = 28.0f;
	constexpr float kMiniScale   = 0.25f;
	constexpr float kHoverScale  = 1.18f;
	constexpr float kMiniTiltDeg = 45.0f;

	XMMATRIX BuildMiniatureMatrix(float xOffset, float fSpinAngle, bool bHovered)
	{
		const float s = kMiniScale * (bHovered ? kHoverScale : 1.0f);
		XMMATRIX matScale     = XMMatrixScaling(s, s, s);
		XMMATRIX matTilt      = XMMatrixRotationX(XMConvertToRadians(kMiniTiltDeg));
		XMMATRIX matSpin      = XMMatrixRotationY(fSpinAngle);
		XMMATRIX matTranslate = XMMatrixTranslation(xOffset, 0.0f, 0.0f);
		return XMMatrixMultiply(XMMatrixMultiply(XMMatrixMultiply(matScale, matTilt), matSpin), matTranslate);
	}
}

void CScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());
	pCamera->UpdateShaderVariables(pd3dCommandList);

	// 라이트 상수 업로드 (b2)
	{
		XMVECTOR vDir = XMVector3Normalize(XMLoadFloat3(&m_xmf3LightDir));
		XMFLOAT3 lightDirN; XMStoreFloat3(&lightDirN, vDir);
		float lightConstants[12] = {
			lightDirN.x, lightDirN.y, lightDirN.z, 0.0f,
			m_xmf3LightColor.x, m_xmf3LightColor.y, m_xmf3LightColor.z, 0.0f,
			m_xmf3Ambient.x,    m_xmf3Ambient.y,    m_xmf3Ambient.z,    0.0f
		};
		pd3dCommandList->SetGraphicsRoot32BitConstants(2, 12, lightConstants, 0);
	}

	if (m_eCurrentState == SceneState::MAP_SELECT) {
		const size_t map1Idx = static_cast<size_t>(SceneState::MAP1);
		const size_t map2Idx = static_cast<size_t>(SceneState::MAP2);

		XMMATRIX mLeft  = BuildMiniatureMatrix(-kMiniOffsetX, m_fMiniatureAngle, m_nHoveredMiniIndex == 0);
		XMMATRIX mRight = BuildMiniatureMatrix(+kMiniOffsetX, m_fMiniatureAngle, m_nHoveredMiniIndex == 1);

		XMFLOAT4X4 xmf4x4Left, xmf4x4Right;
		XMStoreFloat4x4(&xmf4x4Left,  mLeft);
		XMStoreFloat4x4(&xmf4x4Right, mRight);

		if (map1Idx < m_vShaders.size()) m_vShaders[map1Idx].RenderInParent(pd3dCommandList, pCamera, xmf4x4Left);
		if (map2Idx < m_vShaders.size()) m_vShaders[map2Idx].RenderInParent(pd3dCommandList, pCamera, xmf4x4Right);
		return;
	}

	// 일반 씬 렌더
	size_t idx = static_cast<size_t>(m_eCurrentState);
	if (idx < m_vShaders.size()) {
		m_vShaders[idx].Render(pd3dCommandList, pCamera);
	}

	// 메인화면의 글자 아래에 맵1 미니어처 추가 렌더
	if (m_eCurrentState == SceneState::LANDING) {
		constexpr float kPreviewScale   = 0.18f;
		constexpr float kPreviewTiltDeg = 0.0f;
		constexpr float kPreviewYawDeg  = 15.0f;
		constexpr float kPreviewOffsetX = -5.0f;
		constexpr float kPreviewOffsetY = -5.0f;
		constexpr float kPreviewOffsetZ = -38.0f;

		XMMATRIX mScale = XMMatrixScaling(kPreviewScale, kPreviewScale, kPreviewScale);
		XMMATRIX mTilt  = XMMatrixRotationX(XMConvertToRadians(kPreviewTiltDeg));
		XMMATRIX mYaw   = XMMatrixRotationY(XMConvertToRadians(kPreviewYawDeg));
		XMMATRIX mTrans = XMMatrixTranslation(kPreviewOffsetX, kPreviewOffsetY, kPreviewOffsetZ);
		XMMATRIX mPreview = XMMatrixMultiply(XMMatrixMultiply(XMMatrixMultiply(mScale, mTilt), mYaw), mTrans);

		XMFLOAT4X4 xmf4x4Preview;
		XMStoreFloat4x4(&xmf4x4Preview, mPreview);
		const size_t map1Idx = static_cast<size_t>(SceneState::MAP1);
		if (map1Idx < m_vShaders.size()) {
			m_vShaders[map1Idx].RenderInParent(pd3dCommandList, pCamera, xmf4x4Preview);
		}
	}

	// TPS 모드에서 플레이어 모델 렌더링
	if (m_bPlayerVisible && m_pPlayerModel &&
		(m_eCurrentState == SceneState::MAP1 || m_eCurrentState == SceneState::MAP2)) {
		m_pPlayerModel->Render(pd3dCommandList, pCamera);
	}

	// 적의 마커와 소총 추가 렌더
	if ((m_eCurrentState == SceneState::MAP1 || m_eCurrentState == SceneState::MAP2)
		&& idx < m_vShaders.size()) {
		for (auto& p : m_vShaders[idx].GetObjects()) {
			if (!p || !p->IsAlive()) continue;
			if (p->GetTag() != EObjectTag::Enemy) continue;
			auto* pEnemy = static_cast<CEnemyObject*>(p.get());
			if (pEnemy->IsMarkerVisible()) {
				if (CGameObject* pMarker = pEnemy->GetMarker()) {
					pMarker->Render(pd3dCommandList, pCamera);
				}
			}
			if (CGameObject* pRifle = pEnemy->GetRifle()) {
				pRifle->Render(pd3dCommandList, pCamera);
			}
		}
	}
}

void CScene::ReleaseUploadBuffers()
{
	for (CObjectsShader& shader : m_vShaders) {
		shader.ReleaseUploadBuffers();
	}
	if (m_pPlayerModel) m_pPlayerModel->ReleaseUploadBuffers();
}

void CScene::HandleLeftClick(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight, const CCamera* pCamera)
{
	if (!pCamera) return;

	if (m_eCurrentState == SceneState::LANDING) {
		if (!m_pStartButton) return;
		if (m_bGameStartRequested) return;
		const XMFLOAT4X4& view = pCamera->GetViewMatrix();
		const XMFLOAT4X4& proj = pCamera->GetProjectionMatrix();
		if (m_pStartButton->HitTest(nMouseX, nMouseY, nScreenWidth, nScreenHeight, view, proj)) {
			m_pStartButton->SetPressed(true);
			m_bGameStartRequested = true;
			::OutputDebugStringA("[Landing] Game Start button clicked.\n");
		}
		return;
	}

	if (m_eCurrentState == SceneState::MAP_SELECT) {
		UpdateMapSelectHover(nMouseX, nMouseY, nScreenWidth, nScreenHeight, pCamera);
		if (m_nHoveredMiniIndex == 0) {
			m_nRequestedMap = 1;
			::OutputDebugStringA("[MapSelect] Map 1 chosen.\n");
		}
		else if (m_nHoveredMiniIndex == 1) {
			m_nRequestedMap = 2;
			::OutputDebugStringA("[MapSelect] Map 2 chosen.\n");
		}
		return;
	}
}

void CScene::UpdateMapSelectHover(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight, const CCamera* pCamera)
{
	if (m_eCurrentState != SceneState::MAP_SELECT || !pCamera) {
		m_nHoveredMiniIndex = -1;
		return;
	}

	XMMATRIX matView = XMLoadFloat4x4(&pCamera->GetViewMatrix());
	XMMATRIX matProj = XMLoadFloat4x4(&pCamera->GetProjectionMatrix());
	XMMATRIX matVP = XMMatrixMultiply(matView, matProj);

	int hover = -1;
	float bestDist = 1e9f;
	// 두 미니어처 중심의 화면 좌표를 구해 마우스와의 거리 비교
	for (int i = 0; i < 2; ++i) {
		const float xOffset = (i == 0) ? -kMiniOffsetX : +kMiniOffsetX;
		XMMATRIX m = BuildMiniatureMatrix(xOffset, m_fMiniatureAngle, false);
		XMVECTOR vCenter = XMVector3Transform(XMVectorZero(), m);
		XMVECTOR vCenterH = XMVectorSetW(vCenter, 1.0f);
		XMVECTOR vClip = XMVector4Transform(vCenterH, matVP);
		
		float w = XMVectorGetW(vClip);
		if (w <= 0.0f) continue;

		float ndcX = XMVectorGetX(vClip) / w;
		float ndcY = XMVectorGetY(vClip) / w;
		float sx = (ndcX * 0.5f + 0.5f) * nScreenWidth;
		float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * nScreenHeight;
		float dx = sx - nMouseX;
		float dy = sy - nMouseY;
		float dist = sqrtf(dx * dx + dy * dy);
		float radius = nScreenWidth * 0.12f;
		
		if (dist < radius && dist < bestDist) {
			hover = i;
			bestDist = dist;
		}
	}
	m_nHoveredMiniIndex = hover;
}

int CScene::ConsumeSelectedMap()
{
	int n = m_nRequestedMap;
	m_nRequestedMap = 0;
	return n;
}

bool CScene::AddObjectToCurrentMap(std::shared_ptr<CGameObject> pObject)
{
	if (!pObject) return false;
	if (m_eCurrentState != SceneState::MAP1 && m_eCurrentState != SceneState::MAP2) return false;
	size_t idx = static_cast<size_t>(m_eCurrentState);
	if (idx >= m_vShaders.size()) return false;
	m_vShaders[idx].AddObject(std::move(pObject));
	return true;
}

void CScene::TransitionToScene(SceneState newState)
{
	if (m_eCurrentState == newState) return;
	m_eCurrentState = newState;
	::OutputDebugStringA("Transitioned to new scene.\n");
}

CScene::~CScene()
{
}
