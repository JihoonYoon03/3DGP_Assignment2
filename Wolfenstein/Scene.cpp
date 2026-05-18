#include "stdafx.h"
#include "Scene.h"
#include "Shader.h"
#include "Camera.h"
#include "Landing.h"
#include "Maps.h"

CScene::CScene()
{
}

ID3D12RootSignature* CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;

	// 루트 파라미터: [0] 월드 변환 행렬 (32비트 상수 16개)
	//                [1] 뷰/투영 행렬 (32비트 상수 32개)
	D3D12_ROOT_PARAMETER pd3dRootParameters[2];
	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[0].Constants.Num32BitValues = 16;
	pd3dRootParameters[0].Constants.ShaderRegister = 0;
	pd3dRootParameters[0].Constants.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[1].Constants.Num32BitValues = 32;
	pd3dRootParameters[1].Constants.ShaderRegister = 1;
	pd3dRootParameters[1].Constants.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

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
	// 루트 시그너처 생성
	m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);

	// 씬 상태 개수만큼 셰이더를 만들고 파이프라인을 생성한다.
	m_vShaders.resize(static_cast<size_t>(SceneState::COUNT));
	for (CObjectsShader& shader : m_vShaders) {
		shader.CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature.Get());
	}

	// 랜딩 씬(시작 화면) 오브젝트 구성
	{
		std::vector<std::shared_ptr<CGameObject>> vLandingObjects;
		BuildLandingObjects(pd3dDevice, pd3dCommandList, vLandingObjects, m_pStartButton);
		m_vShaders[static_cast<size_t>(SceneState::LANDING)].SetObjects(std::move(vLandingObjects));
	}

	// 맵 1 (붉은 성채) 오브젝트 구성
	{
		std::vector<std::shared_ptr<CGameObject>> vMap1Objects;
		BuildMap1Objects(pd3dDevice, pd3dCommandList, vMap1Objects);
		m_vShaders[static_cast<size_t>(SceneState::MAP1)].SetObjects(std::move(vMap1Objects));
	}

	// 맵 2 (푸른 정찰소) 오브젝트 구성
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
	// ???? ??u ????: ????? ??? (???o? ???¿? ?????, MAP_SELECT???? ??? ???).
	m_fMiniatureAngle += fTimeElapsed * 0.6f; // rad/sec - oo?? ???

	// ???? ??? ???? ????????? ??????????.
	size_t idx = static_cast<size_t>(m_eCurrentState);
	if (idx < m_vShaders.size()) {
		m_vShaders[idx].AnimateObjects(fTimeElapsed);
	}
}

namespace {
	// MAP_SELECT ????? ????? ?? ???? ?ð??????? ??????? ????? ??? ??????.
	constexpr float kMiniOffsetX = 28.0f;       // ?¿? ??????? X ???
	constexpr float kMiniScale   = 0.18f;       // 30x30 ??(120????)?? ~21???? ????
	constexpr float kHoverScale  = 1.18f;       // ??? ?? ??? ???
	constexpr float kMiniTiltDeg = 45.0f;       // X?? ?? (???? ???? ??)

	XMMATRIX BuildMiniatureMatrix(float xOffset, float fSpinAngle, bool bHovered)
	{
		const float s = kMiniScale * (bHovered ? kHoverScale : 1.0f);
		XMMATRIX matScale     = XMMatrixScaling(s, s, s);
		XMMATRIX matTilt      = XMMatrixRotationX(XMConvertToRadians(kMiniTiltDeg));
		XMMATRIX matSpin      = XMMatrixRotationY(fSpinAngle);
		XMMATRIX matTranslate = XMMatrixTranslation(xOffset, 0.0f, 0.0f);
		// row-major: v * (S * Rx * Ry * T)
		return XMMatrixMultiply(XMMatrixMultiply(XMMatrixMultiply(matScale, matTilt), matSpin), matTranslate);
	}
}

void CScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());
	pCamera->UpdateShaderVariables(pd3dCommandList);

	if (m_eCurrentState == SceneState::MAP_SELECT) {
		// MAP_SELECT?? ?????? ????????? ??? ???, MAP1?? MAP2?? ?????? ??????.
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

	// ????? ??: ???? ??? ???? ?????????.
	size_t idx = static_cast<size_t>(m_eCurrentState);
	if (idx < m_vShaders.size()) {
		m_vShaders[idx].Render(pd3dCommandList, pCamera);
	}
}

void CScene::ReleaseUploadBuffers()
{
	for (CObjectsShader& shader : m_vShaders) {
		shader.ReleaseUploadBuffers();
	}
}

void CScene::HandleLeftClick(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight, const CCamera* pCamera)
{
	if (!pCamera) return;

	if (m_eCurrentState == SceneState::LANDING) {
		// ???? ??? ????? ???? ??????? ???? ???.
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
		// ??? ???? ??????? ?????? ??? ???? ??u???.
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
	// ????? ????? ????? ????? ???????????? ???콺???? ????? ?????.
	for (int i = 0; i < 2; ++i) {
		const float xOffset = (i == 0) ? -kMiniOffsetX : +kMiniOffsetX;
		XMMATRIX m = BuildMiniatureMatrix(xOffset, m_fMiniatureAngle, false);
		// ???? ??? (0,0,0,1)?? m?? ??? ?? ????? ??? ????? ??.
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
		// ?????? ????? ?????? ???. ????? ????? ~12%?? ??? ???.
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

void CScene::TransitionToScene(SceneState newState)
{
	if (m_eCurrentState == newState) return;
	m_eCurrentState = newState;
	::OutputDebugStringA("[Scene] Transitioned to new scene.\n");
}

CScene::~CScene()
{
}
