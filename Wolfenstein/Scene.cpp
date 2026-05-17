#include "stdafx.h"
#include "Scene.h"
#include "Shader.h"
#include "Camera.h"
#include "Landing.h"

CScene::CScene()
{
}

ID3D12RootSignature* CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;

	// ???? ???, ??/???? ??? ????????????? ??? ???????
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
	// ????? ??? ?n???©§? ???????.
	m_pd3dGraphicsRootSignature = CreateGraphicsRootSignature(pd3dDevice);

	m_vShaders.emplace_back();
	m_vShaders.back().CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature.Get());
	m_vShaders.back().BuildObjects(pd3dDevice, pd3dCommandList);

	std::vector<std::shared_ptr<CGameObject>> vLandingObjects;
	BuildLandingObjects(pd3dDevice, pd3dCommandList, vLandingObjects, m_pStartButton);
	m_vShaders.back().SetObjects(std::move(vLandingObjects));
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
	for (CObjectsShader& shader : m_vShaders) {
		shader.AnimateObjects(fTimeElapsed);
	}
}

void CScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());
	pCamera->UpdateShaderVariables(pd3dCommandList);

	for (CObjectsShader& shader : m_vShaders) {
		shader.Render(pd3dCommandList, pCamera);
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
	if (!m_pStartButton || !pCamera) return;
	if (m_bGameStartRequested) return;

	const XMFLOAT4X4& view = pCamera->GetViewMatrix();
	const XMFLOAT4X4& proj = pCamera->GetProjectionMatrix();

	if (m_pStartButton->HitTest(nMouseX, nMouseY, nScreenWidth, nScreenHeight, view, proj))
	{
		m_pStartButton->SetPressed(true);
		m_bGameStartRequested = true;
		::OutputDebugStringA("[Landing] Game Start button clicked.\n");
	}
}

CScene::~CScene()
{
}