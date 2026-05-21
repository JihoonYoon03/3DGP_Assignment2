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

	// ��Ʈ �Ķ����: [0] ���� ��ȯ ��� (32��Ʈ ��� 16��)
	//                [1] ��/���� ��� (32��Ʈ ��� 32��)
	D3D12_ROOT_PARAMETER pd3dRootParameters[3];
	// [0] b0: 월드 변환 행렬 (VS 전용, 16 floats)
	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[0].Constants.Num32BitValues = 16;
	pd3dRootParameters[0].Constants.ShaderRegister = 0;
	pd3dRootParameters[0].Constants.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	// [1] b1: View+Projection (VS 전용, 32 floats)
	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[1].Constants.Num32BitValues = 32;
	pd3dRootParameters[1].Constants.ShaderRegister = 1;
	pd3dRootParameters[1].Constants.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	// [2] b2: 라이트 정보 (PS 에서 접근). float3 lightDir + float3 lightColor + float3 ambient
	//        16-byte 패딩 정렬을 맞추기 위해 각 float3 뒤에 pad float 를 두어 12 floats(=3*4) 사용.
	pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[2].Constants.Num32BitValues = 12;
	pd3dRootParameters[2].Constants.ShaderRegister = 2;
	pd3dRootParameters[2].Constants.RegisterSpace = 0;
	pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// PS 에서 b2(라이트 상수) 접근이 필요하므로 DENY_PIXEL_SHADER_ROOT_ACCESS 제거.
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
	// ��Ʈ �ñ׳�ó ����
	m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);

	// �� ���� ������ŭ ���̴��� ����� ������������ �����Ѵ�.
	m_vShaders.resize(static_cast<size_t>(SceneState::COUNT));
	for (CObjectsShader& shader : m_vShaders) {
		shader.CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature.Get());
	}

	// ���� ��(���� ȭ��) ������Ʈ ����
	{
		std::vector<std::shared_ptr<CGameObject>> vLandingObjects;
		BuildLandingObjects(pd3dDevice, pd3dCommandList, vLandingObjects, m_pStartButton);
		m_vShaders[static_cast<size_t>(SceneState::LANDING)].SetObjects(std::move(vLandingObjects));
	}

	// �� 1 (ȸû�� �� �̷�) ������Ʈ ����
	{
		std::vector<std::shared_ptr<CGameObject>> vMap1Objects;
		BuildMap1Objects(pd3dDevice, pd3dCommandList, vMap1Objects);
		m_vShaders[static_cast<size_t>(SceneState::MAP1)].SetObjects(std::move(vMap1Objects));
	}

	// �� 2 (���� ���� ����) ������Ʈ ����
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

	// �÷��̾� �𵨵� �Բ� ����.
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
	// �� ���� �̴Ͼ�ó�� ���� ȸ�� (���� ���¿��� ������, MAP_SELECT ������ ����).
	m_fMiniatureAngle += fTimeElapsed * 0.6f; // rad/sec

	// ���� ������ ���̴� ��ü���� �ִϸ��̼��Ѵ�.
	size_t idx = static_cast<size_t>(m_eCurrentState);
	if (idx < m_vShaders.size()) {
		m_vShaders[idx].AnimateObjects(fTimeElapsed);

		// Run the generalized collision pass against the active scene only.
		// Bullet vs. Enemy is wired up now so the future enemy actor needs
		// only a tag assignment and an AABB to participate. Bullet death
		// is set by the bullet's own animation when it hits a wall, so the
		// callback here just handles the enemy reaction.
		if (m_eCurrentState == SceneState::MAP1 || m_eCurrentState == SceneState::MAP2) {
			Collision::CheckCollisions(
				m_vShaders[idx].GetObjects(),
				EObjectTag::Bullet, EObjectTag::Enemy,
				[](CGameObject* a, CGameObject* b) {
					a->Kill();
					b->OnHit(a);
				});

			// EnemyBullet × Player 충돌.
			// m_pPlayerModel 은 렌더 이중화 방지를 위해 m_vObjects 에 넣지 않으므로
			// 여기서 직접 AABB 비교를 수행한다. 충돌 시 적 총알을 죽이고 라이프
			// 감소 콜백을 호출한다 (Bullet × EnemyBullet 페어는 등록하지 않아 통과).
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
	// MAP1/MAP2 슬롯의 동적 객체(Bullet/EnemyBullet/Enemy) 를 모두 제거한다.
	// 정적 미로 큐브들(태그가 Generic) 은 남겨두어 다음 게임 시작 시 재사용한다.
	for (size_t i : { static_cast<size_t>(SceneState::MAP1), static_cast<size_t>(SceneState::MAP2) }) {
		if (i >= m_vShaders.size()) continue;
		// PruneDead 와 동일한 erase-remove 패턴.
		// 직접 컨테이너를 만지기 위해 GetObjects() const 가 아닌
		// 객체 단위로 Kill() 후 PruneDead() 호출 방식을 사용.
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

namespace {
	// MAP_SELECT ȭ�鿡�� �� ���� �� �̴Ͼ�ó�� �¿�� ��ġ�� �� ����ϴ� �����.
	constexpr float kMiniOffsetX = 28.0f;       // �¿� �߽����κ����� X ������
	constexpr float kMiniScale   = 0.18f;       // 30x30 ��(120 ����)�� ~21 �������� ���
	constexpr float kHoverScale  = 1.18f;       // ȣ�� �� ũ�� ����
	constexpr float kMiniTiltDeg = 45.0f;       // X �� ���� (������ ���� ����)

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

	// ===== 라이트 상수 업로드 (b2) =====
	// HLSL cbLightInfo 와 동일한 12 float (3 * vec4 패딩) 레이아웃.
	// 방향광 진행 방향은 정규화하여 셰이더에서 dot 계산이 일관되도록 한다.
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
		// MAP_SELECT ������ ���� �ΰ��� ���� �׸��� �ʰ�, MAP1/MAP2 �� �̴Ͼ�ó�� �׸���.
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

	// �� �� ����: ���� ������ ���̴��� ������.
	size_t idx = static_cast<size_t>(m_eCurrentState);
	if (idx < m_vShaders.size()) {
		m_vShaders[idx].Render(pd3dCommandList, pCamera);
	}

	// �ΰ��� �ʿ��� TPS ���� ��۵Ǿ� ������ �÷��̾� ���� �Բ� �׸���.
	// ������ m_vShaders[MAP*].Render �� ������ ��Ʈ �ñ׳�ó�� PSO �� ���ε��� �����̹Ƿ�,
	// CGameObject::Render �� ��ü������ ���� ����� �����ϰ� �޽ø� �׸��� ����ϴ�.
	if (m_bPlayerVisible && m_pPlayerModel &&
		(m_eCurrentState == SceneState::MAP1 || m_eCurrentState == SceneState::MAP2)) {
		m_pPlayerModel->Render(pd3dCommandList, pCamera);
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
		// ���� ȭ���� ���� ��ư ��Ʈ �׽�Ʈ.
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
		// ȣ�� ���� �̴Ͼ�ó�� ���� �� ������ ����Ѵ�.
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
	// �� �̴Ͼ�ó�� �߽��� ȭ�� ��ǥ�� ������ ���콺���� �Ÿ��� ���Ѵ�.
	for (int i = 0; i < 2; ++i) {
		const float xOffset = (i == 0) ? -kMiniOffsetX : +kMiniOffsetX;
		XMMATRIX m = BuildMiniatureMatrix(xOffset, m_fMiniatureAngle, false);
		// ���� (0,0,0,1) �� m ���� ��ȯ�� �̴Ͼ�ó�� ���� ��ǥ �߽��� ��´�.
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
		// ȭ�� �ʺ��� 12% ������ ȣ�� �ݰ����� ���.
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
	::OutputDebugStringA("[Scene] Transitioned to new scene.\n");
}

CScene::~CScene()
{
}
