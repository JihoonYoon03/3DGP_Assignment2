#include "stdafx.h"
#include <windowsx.h>
#include "GameFramework.h"
#include "Camera.h"
#include "Maps.h"
#include "GameObject.h"
#include "Mesh.h"

CGameFramework::CGameFramework()
{
	m_nRtvDescriptorIncrementSize = 0;

	m_nDsvDescriptorIncrementSize = 0;

	m_nSwapChainBufferIndex = 0;

	m_nFenceValue = 0;

	m_nWndClientWidth = FRAME_BUFFER_WIDTH;
	m_nWndClientHeight = FRAME_BUFFER_HEIGHT;

	for (int i = 0; i < m_nSwapChainBuffers; i++) {
		m_nFenceValues[i] = 0;
	}

	m_pScene = NULL;

	_tcscpy_s(m_pszFrameRate, _T("LapProject ("));
}

// ���� ���α׷��� D3D12 ����̽�/����ü�� ���� ��� �ʱ�ȭ�ϴ� ������.
bool CGameFramework::OnCreate(HINSTANCE hInstance, HWND hMainWnd)
{
	m_hInstance = hInstance;
	m_hWnd = hMainWnd;

	CreateDirect3DDevice();
	CreateCommandQueueAndList();
	CreateRtvAndDsvDescriptorHeaps();
	CreateSwapChain();
	CreateDepthStencilView();

	BuildObjects();

	return true;
}

void CGameFramework::OnDestroy()
{
	// GPU�� ��� �۾��� ����ĥ ������ ����� �� �ڿ� ����.
	WaitForGPUComplete();

	// ���� ��ü(������ ���ҽ� ����) ����.
	ReleaseObjects();

	::CloseHandle(m_hFenceEvent);

	// swapchain ������ ���� ���� ��ȯ �� ����.
	m_pdxgiSwapChain->SetFullscreenState(FALSE, NULL);
}

void CGameFramework::CreateSwapChain()
{
	RECT rcClient;
	::GetClientRect(m_hWnd, &rcClient);
	m_nWndClientWidth = rcClient.right - rcClient.left;
	m_nWndClientHeight = rcClient.bottom - rcClient.top;

	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	::ZeroMemory(&dxgiSwapChainDesc, sizeof(dxgiSwapChainDesc));
	dxgiSwapChainDesc.BufferCount = m_nSwapChainBuffers;
	dxgiSwapChainDesc.BufferDesc.Width = m_nWndClientWidth;
	dxgiSwapChainDesc.BufferDesc.Height = m_nWndClientHeight;
	dxgiSwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.OutputWindow = m_hWnd;
	dxgiSwapChainDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	dxgiSwapChainDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels - 1) : 0;
	dxgiSwapChainDesc.Windowed = TRUE;

	// Ǯ��ũ�� ����� �����ϵ��� ��� ���� ��� �÷���.
	dxgiSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ComPtr<IDXGISwapChain> pSwapChain;

	HRESULT hResult = m_pdxgiFactory->CreateSwapChain
	(
		m_pd3dCommandQueue.Get(),
		&dxgiSwapChainDesc,
		pSwapChain.GetAddressOf()
	);
	pSwapChain.As(&m_pdxgiSwapChain);

	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	hResult = m_pdxgiFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);

#ifndef _WITH_SWAPCHAIN_FULLSCREEN_STATE
	CreateRenderTargetViews();
#endif
}

void CGameFramework::CreateDirect3DDevice()
{
	HRESULT hResult;

	UINT nDXGIFactoryFlags = 0;

#if defined(_DEBUG)
	ComPtr<ID3D12Debug> pd3dDebugController;
	hResult = D3D12GetDebugInterface(IID_PPV_ARGS(&pd3dDebugController));

	if (pd3dDebugController)
	{
		pd3dDebugController->EnableDebugLayer();
	}
	nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	hResult = ::CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&m_pdxgiFactory));

	ComPtr<IDXGIAdapter1> pd3dAdapter = NULL;

	// �ý����� ����͵��� ��ȸ�ϸ鼭 D3D12 ��� ���� 12.0 �� �����ϴ� ù ����͸� ã�´�.
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != m_pdxgiFactory->EnumAdapters1(i, &pd3dAdapter); ++i)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
		pd3dAdapter->GetDesc1(&dxgiAdapterDesc);

		if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		// ID3D12Device ���� �κ�.
		if (SUCCEEDED(D3D12CreateDevice(
			pd3dAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_pd3dDevice)
		)))
			break;
	}

	// 12.0 ����͸� �� ã���� WARP(����Ʈ����) ����ͷ� ����.
	if (!pd3dAdapter)
	{
		m_pdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pd3dAdapter));

		D3D12CreateDevice(
			pd3dAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_pd3dDevice)
		);
	}

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS d3dMsaaQualityLevels;
	d3dMsaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dMsaaQualityLevels.SampleCount = 4;	// MSAA 4x ��Ƽ���ø�
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;
	m_pd3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&d3dMsaaQualityLevels,
		sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS)
	);

	// ����̽��� �����ϴ� ���� ���� ǰ�� ���� ���� �����Ѵ�.
	m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;

	// �潺 ����, �ʱⰪ 0.
	hResult = m_pd3dDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pd3dFence)
	);
	m_nFenceValue = 0;

	// �潺 ��ȣ ���� �̺�Ʈ ��ü ���� (���� ���� FALSE, �ʱⰪ FALSE).
	m_hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

void CGameFramework::CreateCommandQueueAndList()
{
	// Direct Ŀ�ǵ� ť ����.
	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hResult = m_pd3dDevice->CreateCommandQueue(
		&d3dCommandQueueDesc,
		IID_PPV_ARGS(&m_pd3dCommandQueue)
	);

	// Ŀ�ǵ� �Ҵ��� ����.
	hResult = m_pd3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&m_pd3dCommandAllocator)
	);

	// Ŀ�ǵ� ����Ʈ ����.
	hResult = m_pd3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pd3dCommandAllocator.Get(),
		NULL,
		IID_PPV_ARGS(&m_pd3dCommandList)
	);

	// ���� ����Ʈ�� ���� ���� Open �����̹Ƿ� Close �� �ݾƵд�.
	hResult = m_pd3dCommandList->Close();
}

void CGameFramework::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	d3dDescriptorHeapDesc.NumDescriptors = m_nSwapChainBuffers;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	d3dDescriptorHeapDesc.NodeMask = 0;

	// ����ü�� ���� ����ŭ�� RTV ��ũ���� ��.
	HRESULT hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dRtvDescriptorHeap)
	);

	m_nRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// DSV ��ũ���� ��(1��).
	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dDsvDescriptorHeap)
	);

	m_nDsvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

// ����ü�� �� ���ۿ� ���� RTV �� �����.
void CGameFramework::CreateRenderTargetViews()
{
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	for (UINT i = 0; i < m_nSwapChainBuffers; ++i)
	{
		m_pdxgiSwapChain->GetBuffer(
			i,
			IID_PPV_ARGS(&m_ppd3dSwapChainBackBuffers[i])
		);
		m_pd3dDevice->CreateRenderTargetView(
			m_ppd3dSwapChainBackBuffers[i].Get(),
			NULL,
			d3dRtvCPUDescriptorHandle
		);
		d3dRtvCPUDescriptorHandle.ptr += m_nRtvDescriptorIncrementSize;
	}
}

void CGameFramework::CreateDepthStencilView()
{
	D3D12_RESOURCE_DESC d3dResourceDesc;
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	// Buffer or Tex
	d3dResourceDesc.Alignment = 0;	// 64KB
	d3dResourceDesc.Width = m_nWndClientWidth;
	d3dResourceDesc.Height = m_nWndClientHeight;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dResourceDesc.SampleDesc.Count = (m_bMsaa4xEnable) ? 4 : 1;
	d3dResourceDesc.SampleDesc.Quality = (m_bMsaa4xEnable) ? (m_nMsaa4xQualityLevels - 1) : 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES d3dHeapProperties;
	::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;

	// ����-���ٽ� ���� Ŭ���� ��.
	D3D12_CLEAR_VALUE d3dClearValue;
	d3dClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dClearValue.DepthStencil.Depth = 1.0f;
	d3dClearValue.DepthStencil.Stencil = 0;
	m_pd3dDevice->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&d3dClearValue,
		IID_PPV_ARGS(&m_pd3dDepthStencilBuffer)
	);

	// ����-���ٽ� �� ����.
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_pd3dDevice->CreateDepthStencilView(
		m_pd3dDepthStencilBuffer.Get(),
		NULL,
		d3dDsvCPUDescriptorHandle
	);
}

void CGameFramework::BuildObjects()
{
	m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL);

	// ī�޶� ��ü �ʱ�ȭ - ����Ʈ/������Ʈ/�������/�ʱ� �����.
	m_pCamera = std::make_unique<CCamera>();
	m_pCamera->SetViewport(0, 0, m_nWndClientWidth, m_nWndClientHeight, 0.0f, 1.0f);
	m_pCamera->SetScissorRect(0, 0, m_nWndClientWidth, m_nWndClientHeight);
	m_pCamera->GenerateProjectionMatrix(1.0f, FRUSTUM_FAR_PLANE,
		float(m_nWndClientWidth) / float(m_nWndClientHeight), 90.0f);
	m_pCamera->GenerateViewMatrix(XMFLOAT3(0.0f, 0.0f, -50.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f));

	// �� ���� �� �� ������ ��� ���� ��ü ����.
	m_pScene = std::make_unique<CScene>();
	m_pScene->BuildObjects(m_pd3dDevice.Get(), m_pd3dCommandList.Get());

	// TPS ���������� ���̴� �÷��̾� ��(���� ť��). ��/����/���̴� ��� ������ ���� ������.
	{
		auto pPlayerMesh = std::make_shared<CCubeMeshDiffused>(
			m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
			1.2f, 2.6f, 1.2f, true, XMFLOAT4(0.85f, 0.25f, 0.25f, 1.0f));
		m_pPlayerModel = std::make_shared<CGameObject>();
		m_pPlayerModel->SetMesh(pPlayerMesh);
		m_pPlayerModel->SetTag(EObjectTag::Player);
		m_pPlayerModel->SetAABBHalf(XMFLOAT3(0.6f, 1.3f, 0.6f));
		if (m_pScene) m_pScene->SetPlayerModel(m_pPlayerModel);
	}

	// Shared mesh for bullets and the crosshair so spawning new ones is cheap.
	// 플레이어 총알: 노란 큐브 (기존 색 유지)
	m_pBulletMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.4f, 0.4f, 0.4f, true, XMFLOAT4(1.0f, 0.9f, 0.2f, 1.0f));

	// 적 총알: 시각 구분을 위해 붉은 큐브.
	m_pEnemyBulletMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.4f, 0.4f, 0.4f, true, XMFLOAT4(1.0f, 0.25f, 0.20f, 1.0f));

	// 적 공유 메시: 어두운 보라 계열 큐브. 플레이어(붉은색) 와 시각 구분.
	m_pEnemyMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		1.2f, 2.6f, 1.2f, true, XMFLOAT4(0.45f, 0.15f, 0.55f, 1.0f));

	// HUD 셰이더 (십자선 + 라이프 바 공용). 깊이 OFF, 컬링 OFF.
	// CreateShader 는 가상 함수이므로 base 포인터로 호출해도 CHudShader::CreateShader 가 디스패치된다.
	m_pHudShader = std::make_shared<CHudShader>();
	if (m_pScene) {
		m_pHudShader->CreateShader(m_pd3dDevice.Get(), m_pScene->GetGraphicsRootSignature());
	}

	// 화면 정중앙 고정 십자선(+) 조준점. 정점이 NDC(클립 공간) 좌표로 미리
	// 박혀 있으므로 카메라 회전/이동에 무관하게 항상 화면 정가운데에 그려진다.
	{
		auto pCrosshairMesh = std::make_shared<CCrosshairMesh>(
			m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
			UINT(m_nWndClientWidth), UINT(m_nWndClientHeight),
			10u, 2u, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		m_pCrosshair = std::make_shared<CGameObject>();
		m_pCrosshair->SetMesh(pCrosshairMesh);
		m_pCrosshair->SetShader(m_pHudShader);
	}

	// 라이프 바 칸 10개 생성. 모두 같은 색(붉은)으로 만들고, 매 프레임 m_nPlayerLife
	// 개수만큼 앞에서부터만 렌더하여 잃은 칸은 그리지 않는다.
	m_pLifeBarSegments.reserve(10);
	for (UINT i = 0; i < 10u; ++i) {
		auto pSegMesh = std::make_shared<CLifeBarMesh>(
			m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
			UINT(m_nWndClientWidth), UINT(m_nWndClientHeight),
			i, 10u, 22u, 8u, 4u, 24u,
			XMFLOAT4(0.95f, 0.25f, 0.30f, 1.0f));
		auto pSegObj = std::make_shared<CGameObject>();
		pSegObj->SetMesh(pSegMesh);
		pSegObj->SetShader(m_pHudShader);
		m_pLifeBarSegments.push_back(std::move(pSegObj));
	}

	// 라이프 감소 콜백 등록. Scene 에서 EnemyBullet × Player 충돌 시 호출된다.
	if (m_pScene) {
		m_pScene->SetOnPlayerHit([this]() {
			if (m_nPlayerLife > 0) --m_nPlayerLife;
			if (m_nPlayerLife <= 0) m_bResetPending = true;
		});
	}

	// Ŀ�ǵ� ����Ʈ�� �ݰ� ť�� �����Ͽ� GPU �� ��� ���ε带 �����ϰ� �Ѵ�.
	m_pd3dCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	// GPU �� ���ε带 �� ó���� ������ ��ٸ���.
	WaitForGPUComplete();

	// ���ε�� �ӽ� ���۵��� ����(���� ���۴� ����).
	if (m_pScene) m_pScene->ReleaseUploadBuffers();
	if (m_pBulletMesh) m_pBulletMesh->ReleaseUploadBuffers();
	if (m_pEnemyBulletMesh) m_pEnemyBulletMesh->ReleaseUploadBuffers();
	if (m_pEnemyMesh) m_pEnemyMesh->ReleaseUploadBuffers();
	if (m_pCrosshair) m_pCrosshair->ReleaseUploadBuffers();
	for (auto& pSeg : m_pLifeBarSegments) {
		if (pSeg) pSeg->ReleaseUploadBuffers();
	}

	m_GameTimer.Reset();
}

void CGameFramework::ReleaseObjects()
{

}

void CGameFramework::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_LBUTTONDOWN:
	{
		int nMouseX = GET_X_LPARAM(lParam);
		int nMouseY = GET_Y_LPARAM(lParam);
		if (m_pScene && m_pCamera) {
			// In gameplay scenes the left click is "shoot". UI scenes
			// (landing button, map select tiles) still go through the
			// scene's hit-test path.
			const SceneState st = m_pScene->GetCurrentState();
			if (st == SceneState::MAP1 || st == SceneState::MAP2) {
				FireBullet();
			}
			else {
				m_pScene->HandleLeftClick(nMouseX, nMouseY, m_nWndClientWidth, m_nWndClientHeight, m_pCamera.get());
			}
		}
		break;
	}
	case WM_RBUTTONDOWN:
		break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		break;
	case WM_MOUSEMOVE:
	{
		int nMouseX = GET_X_LPARAM(lParam);
		int nMouseY = GET_Y_LPARAM(lParam);
		if (m_pScene && m_pCamera && m_pScene->GetCurrentState() == SceneState::MAP_SELECT) {
			m_pScene->UpdateMapSelectHover(nMouseX, nMouseY, m_nWndClientWidth, m_nWndClientHeight, m_pCamera.get());
		}
		break;
	}	default:
		break;
	}
}

void CGameFramework::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_KEYUP:
		switch (wParam)
		{
		case VK_ESCAPE:
			::PostQuitMessage(0);
			break;
		case VK_F9:
			// F9: Ǯ��ũ��/â��� ���.
			ChangeSwapChainState();
			break;
		case 'V':
			// V: FPS / TPS ���� ��� (�䱸 C). �ΰ��� ������ ���� ȿ���� ���δ�.
			if (m_pCamera && m_pScene) {
				const SceneState st = m_pScene->GetCurrentState();
				if (st == SceneState::MAP1 || st == SceneState::MAP2) {
					ECameraMode cur = m_pCamera->GetMode();
					ECameraMode nxt = (cur == ECameraMode::FPS) ? ECameraMode::TPS : ECameraMode::FPS;
					m_pCamera->SetMode(nxt);
					m_pScene->SetPlayerVisible(nxt == ECameraMode::TPS);
				}
			}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}
LRESULT CALLBACK CGameFramework::OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	switch (nMessageID)
	{
	case WM_SIZE:
	{
		m_nWndClientWidth = LOWORD(lParam);
		m_nWndClientHeight = HIWORD(lParam);
		break;
	}
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
		OnProcessingMouseMessage(hWnd, nMessageID, wParam, lParam);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		OnProcessingKeyboardMessage(hWnd, nMessageID, wParam, lParam);
		break;
	}
	return 0;
}

void CGameFramework::ChangeSwapChainState()
{
	WaitForGPUComplete();

	BOOL bFullScreenState = FALSE;
	m_pdxgiSwapChain->GetFullscreenState(&bFullScreenState, NULL);
	// TRUE = fullscreen
	m_pdxgiSwapChain->SetFullscreenState(FALSE, NULL);

	DXGI_MODE_DESC dxgiTargetParameters;
	dxgiTargetParameters.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiTargetParameters.Width = m_nWndClientWidth;
	dxgiTargetParameters.Height = m_nWndClientHeight;
	dxgiTargetParameters.RefreshRate.Numerator = 60;
	dxgiTargetParameters.RefreshRate.Denominator = 1;
	dxgiTargetParameters.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiTargetParameters.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	m_pdxgiSwapChain->ResizeTarget(&dxgiTargetParameters);

	for (int i = 0; i < m_nSwapChainBuffers; i++) {
		if (m_ppd3dSwapChainBackBuffers[i])
			m_ppd3dSwapChainBackBuffers[i].Reset();
	}

	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	m_pdxgiSwapChain->GetDesc(&dxgiSwapChainDesc);
	m_pdxgiSwapChain->ResizeBuffers(
		m_nSwapChainBuffers,
		m_nWndClientWidth,
		m_nWndClientHeight,
		dxgiSwapChainDesc.BufferDesc.Format,
		dxgiSwapChainDesc.Flags
	);
	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();
	CreateRenderTargetViews();
}


void CGameFramework::ProcessInput()
{
	if (!m_pScene || !m_pCamera) return;

	const SceneState state = m_pScene->GetCurrentState();
	// 1��Ī/3��Ī �̵�(���콺 ��, WASD, ����) �Է��� MAP1/MAP2 ������ ó���Ѵ�.
	const bool bGameplay = (state == SceneState::MAP1 || state == SceneState::MAP2);
	if (!bGameplay) {
		// LANDING/MAP_SELECT: ���콺 Ŀ�� ǥ��, ĸó ����.
		if (m_bMouseCaptured) {
			::ShowCursor(TRUE);
			m_bMouseCaptured = false;
		}
		return;
	}

	// Tick the shoot cooldown so the next click can fire when this hits 0.
	if (m_fFireCooldown > 0.0f) m_fFireCooldown -= m_GameTimer.GetTimeElapsed();

	const bool bForeground = (::GetForegroundWindow() == m_hWnd);

	// =============== ���콺 �� ===============
	if (bForeground) {
		RECT rcClient;
		::GetClientRect(m_hWnd, &rcClient);
		POINT ptCenter{ (rcClient.right - rcClient.left) / 2, (rcClient.bottom - rcClient.top) / 2 };
		::ClientToScreen(m_hWnd, &ptCenter);

		if (!m_bMouseCaptured) {
			::ShowCursor(FALSE);
			::SetCursorPos(ptCenter.x, ptCenter.y);
			m_ptWndCenterScreen = ptCenter;
			m_bMouseCaptured = true;
		}
		else {
			POINT pt;
			::GetCursorPos(&pt);
			const int dx = pt.x - m_ptWndCenterScreen.x;
			const int dy = pt.y - m_ptWndCenterScreen.y;
			if (dx != 0 || dy != 0) {
				const float kSensitivityDeg = 0.15f;
				const float fYaw   = XMConvertToRadians(static_cast<float>(dx) * kSensitivityDeg);
				const float fPitch = XMConvertToRadians(static_cast<float>(dy) * kSensitivityDeg);
				m_pCamera->Rotate(-fPitch, fYaw);
				::SetCursorPos(m_ptWndCenterScreen.x, m_ptWndCenterScreen.y);
			}
			m_ptWndCenterScreen = ptCenter;
		}
	}

	// =============== WASD + �浹 (XZ) ===============
	// ���� �ִ� �÷��̾� ��ġ�� m_xmf3PlayerPos. ī�޶� TPS �� �� �ڷ� ���� ��ǥ�� �Űܰ�����,
	// �̵�/�浹/�߷� ������ ��� m_xmf3PlayerPos �� ���� �����Ѵ�.
	const float dt = m_GameTimer.GetTimeElapsed();
	const float kMoveSpeed = 6.0f;
	const float kStep = kMoveSpeed * dt;

	float fForward = 0.0f, fStrafe = 0.0f;
	if (::GetAsyncKeyState('W') & 0x8000) fForward += 1.0f;
	if (::GetAsyncKeyState('S') & 0x8000) fForward -= 1.0f;
	if (::GetAsyncKeyState('D') & 0x8000) fStrafe  += 1.0f;
	if (::GetAsyncKeyState('A') & 0x8000) fStrafe  -= 1.0f;

	XMFLOAT3 pos = m_xmf3PlayerPos;
	XMFLOAT3 next = pos;
	const float kPlayerRadius = 1.0f;
	const float kFeetY = pos.y - MAP_EYE_HEIGHT;

	if (fForward != 0.0f || fStrafe != 0.0f) {
		const XMFLOAT3 look = m_pCamera->GetLook();
		const XMFLOAT3 right = m_pCamera->GetRight();
		XMFLOAT3 fwdXZ{ look.x, 0.0f, look.z };
		XMFLOAT3 rgtXZ{ right.x, 0.0f, right.z };
		float fwdLen = sqrtf(fwdXZ.x * fwdXZ.x + fwdXZ.z * fwdXZ.z);
		float rgtLen = sqrtf(rgtXZ.x * rgtXZ.x + rgtXZ.z * rgtXZ.z);
		if (fwdLen > 1e-5f) { fwdXZ.x /= fwdLen; fwdXZ.z /= fwdLen; }
		if (rgtLen > 1e-5f) { rgtXZ.x /= rgtLen; rgtXZ.z /= rgtLen; }

		XMFLOAT3 delta{
			(fwdXZ.x * fForward + rgtXZ.x * fStrafe) * kStep,
			0.0f,
			(fwdXZ.z * fForward + rgtXZ.z * fStrafe) * kStep
		};

		if (delta.x != 0.0f) {
			const float probeX = next.x + delta.x + (delta.x > 0.0f ? kPlayerRadius : -kPlayerRadius);
			if (!IsBlockedInMap(state, probeX, next.z, kFeetY)) next.x += delta.x;
		}
		if (delta.z != 0.0f) {
			const float probeZ = next.z + delta.z + (delta.z > 0.0f ? kPlayerRadius : -kPlayerRadius);
			if (!IsBlockedInMap(state, next.x, probeZ, kFeetY)) next.z += delta.z;
		}
	}

	// =============== ���� + �߷� (Y) ===============
	// ���� ����: 3 * STEP_H (~2.1) ���� ���. v0 = sqrt(2 * g * h).
	const float kGravity = 30.0f;
	const float kJumpApex = 3.0f * 0.7f; // STEP_H = 0.7
	const float kJumpV0 = sqrtf(2.0f * kGravity * kJumpApex);

	const bool bSpaceNow = (::GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	if (bSpaceNow && !m_bPrevSpacePressed && m_bGrounded) {
		m_fVerticalVelocity = kJumpV0;
		m_bGrounded = false;
	}
	m_bPrevSpacePressed = bSpaceNow;

	const float floorYAtNext = GetFloorHeightAt(state, next.x, next.z);
	const float groundY = floorYAtNext + MAP_EYE_HEIGHT;

	if (!m_bGrounded) {
		// ����: �߷� ����, y ����.
		m_fVerticalVelocity -= kGravity * dt;
		next.y = pos.y + m_fVerticalVelocity * dt;
		if (next.y <= groundY) {
			next.y = groundY;
			m_fVerticalVelocity = 0.0f;
			m_bGrounded = true;
		}
	}
	else {
		// ����: �� ���̿� ī�޶� �� ��ġ ���� (��� �ڿ������� ��������).
		next.y = groundY;
	}

	// ���� �ִ� �÷��̾� ��ġ ����.
	m_xmf3PlayerPos = next;

	// ī�޶� ��ġ�� ��忡 ���� �б�.
	if (m_pCamera->GetMode() == ECameraMode::FPS) {
		// FPS: ī�޶� ��ġ = �÷��̾� �� ��ġ.
		m_pCamera->SetPosition(m_xmf3PlayerPos);
		// Keep the (invisible) player model aligned with camera yaw so that
		// flipping to TPS looks consistent immediately.
		if (m_pPlayerModel) {
			XMFLOAT3 modelCenter{
				m_xmf3PlayerPos.x,
				m_xmf3PlayerPos.y - MAP_EYE_HEIGHT + 1.3f,
				m_xmf3PlayerPos.z };
			m_pPlayerModel->SetWorldYawAndPosition(m_pCamera->GetYaw(), modelCenter);
		}
	}
	else {
		// TPS: �÷��̾� �ڷ� kBack ��ŭ, ���� kUp ��ŭ ������ ��ġ�� ī�޶� �д�.
		// Spring-arm: clamp the camera-to-player distance if a wall is in
		// the way so the camera does not pop through level geometry.
		const float kBack = 7.0f, kUp = 3.0f;
		const XMFLOAT3 look = m_pCamera->GetLook();
		const XMFLOAT3 backDir{ -look.x, 0.0f, -look.z };
		const float eyeY = m_xmf3PlayerPos.y + kUp;
		const float dist = ClampDistanceAgainstWalls(state, m_xmf3PlayerPos, backDir, kBack, eyeY);
		const float backLen = sqrtf(backDir.x * backDir.x + backDir.z * backDir.z);
		const float invLen = (backLen > 1e-5f) ? (1.0f / backLen) : 0.0f;
		XMFLOAT3 tpsEye{
			m_xmf3PlayerPos.x + backDir.x * invLen * dist,
			eyeY,
			m_xmf3PlayerPos.z + backDir.z * invLen * dist };
		m_pCamera->SetPosition(tpsEye);

		// Rotate the player model with the camera yaw so it visibly faces
		// the same direction the view is aimed at.
		if (m_pPlayerModel) {
			XMFLOAT3 modelCenter{
				m_xmf3PlayerPos.x,
				m_xmf3PlayerPos.y - MAP_EYE_HEIGHT + 1.3f,
				m_xmf3PlayerPos.z };
			m_pPlayerModel->SetWorldYawAndPosition(m_pCamera->GetYaw(), modelCenter);
		}
	}

	// 십자선은 NDC 정점을 그대로 사용하는 CCrosshairMesh + CHudShader 조합으로
	// 그려지므로 별도의 위치/회전 갱신이 필요 없다. FrameAdvance 의 Render 단계에서
	// 마지막에 한 번 호출되면 항상 화면 정중앙에 회전 없이 표시된다.
}

void CGameFramework::FireBullet()
{
	if (!m_pScene || !m_pCamera || !m_pBulletMesh) return;
	const SceneState state = m_pScene->GetCurrentState();
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return;
	if (m_fFireCooldown > 0.0f) return;

	const XMFLOAT3 look = m_pCamera->GetLook();
	// Always spawn ahead of the player's head so a TPS bullet does not
	// originate visibly behind the player model, and FPS bullets do not
	// immediately self-collide with the camera's host cell.
	XMFLOAT3 origin{
		m_xmf3PlayerPos.x + look.x * 0.8f,
		m_xmf3PlayerPos.y + 0.2f,
		m_xmf3PlayerPos.z + look.z * 0.8f };
	const float kBulletSpeed = 25.0f;
	auto pBullet = std::make_shared<CBulletObject>(origin, look, kBulletSpeed, EObjectTag::Bullet);
	pBullet->SetMesh(m_pBulletMesh);
	pBullet->SetSceneState(state);
	m_pScene->AddObjectToCurrentMap(pBullet);

	m_fFireCooldown = 0.18f;
}

void CGameFramework::SpawnEnemyBullet(const XMFLOAT3& xmf3Origin, const XMFLOAT3& xmf3Dir)
{
	if (!m_pScene || !m_pEnemyBulletMesh) return;
	const SceneState state = m_pScene->GetCurrentState();
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return;

	// 적 총알은 플레이어 총알보다 조금 느려 회피 가능성을 남긴다.
	const float kEnemyBulletSpeed = 18.0f;
	auto pBullet = std::make_shared<CBulletObject>(xmf3Origin, xmf3Dir, kEnemyBulletSpeed, EObjectTag::EnemyBullet);
	pBullet->SetMesh(m_pEnemyBulletMesh);
	pBullet->SetSceneState(state);
	m_pScene->AddObjectToCurrentMap(pBullet);
}

void CGameFramework::SpawnEnemiesForMap(SceneState state)
{
	if (!m_pScene || !m_pEnemyMesh) return;
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return;

	// 적은 AABB half y = 1.3 (몸체 높이 2.6). 플레이어 반경 5타일 바깥에서 무작위 추출.
	const float kEnemyHalfY = 1.3f;
	std::vector<XMFLOAT3> spawns = PickEnemySpawnPositions(state, m_xmf3PlayerPos, 10, kEnemyHalfY);

	std::mt19937 seedGen{ std::random_device{}() };
	for (const XMFLOAT3& pos : spawns) {
		// 인스턴스별 시드로 RNG 독립성 확보.
		const unsigned int nSeed = seedGen();
		auto pEnemy = std::make_shared<CEnemyObject>(state, nSeed);
		pEnemy->SetMesh(m_pEnemyMesh);
		pEnemy->SetPosition(pos);
		// 적이 총알을 발사할 때 호출되는 콜백 — GameFramework 의 SpawnEnemyBullet 로 위임.
		pEnemy->SetFireCallback([this](const XMFLOAT3& xmf3Origin, const XMFLOAT3& xmf3Dir) {
			SpawnEnemyBullet(xmf3Origin, xmf3Dir);
		});
		// 시야 판정 / 추적 방향 계산을 위해 플레이어의 현재 위치를 가져온다.
		pEnemy->SetPlayerPosGetter([this]() {
			return m_xmf3PlayerPos;
		});
		m_pScene->AddObjectToCurrentMap(pEnemy);
	}
}

void CGameFramework::AnimateObjects()
{
	if (!m_pScene) return;
	m_pScene->AnimateObjects(m_GameTimer.GetTimeElapsed());

	// ���� ȭ�鿡�� GAME START ��ư�� ������ �� ���� ȭ��(MAP_SELECT) ���� ��ȯ.
	if (m_pScene->IsGameStartRequested() && m_pScene->GetCurrentState() == SceneState::LANDING) {
		m_pScene->TransitionToScene(SceneState::MAP_SELECT);
		m_pScene->ClearGameStartRequest();
		SetupMapSelectCamera();
	}

	// �� ���� ȭ�鿡�� �̴Ͼ�ó Ŭ���� ���� �ΰ��� ������ ��ȯ.
	if (m_pScene->GetCurrentState() == SceneState::MAP_SELECT) {
		int n = m_pScene->ConsumeSelectedMap();
		if (n == 1) {
			m_pScene->TransitionToScene(SceneState::MAP1);
			SetupGameCamera(SceneState::MAP1);
		}
		else if (n == 2) {
			m_pScene->TransitionToScene(SceneState::MAP2);
			SetupGameCamera(SceneState::MAP2);
		}
	}

	// 라이프 0 으로 사망 → 게임플레이 객체 정리 후 LANDING 화면으로 복귀.
	// 사용자가 다시 GAME START 를 누르면 MAP_SELECT → MAP1/2 흐름으로 새 게임이 시작된다.
	if (m_bResetPending) {
		m_bResetPending = false;
		m_pScene->ResetGameplayState();
		m_nPlayerLife = 10;
		m_pScene->TransitionToScene(SceneState::LANDING);
		// 마우스 캡처 / 발사 쿨다운 / 점프 상태 초기화 — 다음 게임에서 깨끗하게 시작.
		if (m_bMouseCaptured) {
			::ShowCursor(TRUE);
			m_bMouseCaptured = false;
		}
		m_fFireCooldown = 0.0f;
		m_fVerticalVelocity = 0.0f;
		m_bGrounded = true;
		// 카메라를 랜딩 화면 시점으로 되돌린다.
		if (m_pCamera) {
			m_pCamera->GenerateViewMatrix(
				XMFLOAT3(0.0f, 0.0f, -50.0f),
				XMFLOAT3(0.0f, 0.0f, 0.0f),
				XMFLOAT3(0.0f, 1.0f, 0.0f));
			m_pCamera->SetMode(ECameraMode::FPS);
		}
		if (m_pScene) m_pScene->SetPlayerVisible(false);
		::OutputDebugStringA("[GameFramework] Player died - returning to LANDING.\n");
	}
}

void CGameFramework::SetupGameCamera(SceneState state)
{
	if (!m_pCamera) return;
	// �� ���� 1��Ī ���� ��ġ/������ �� ���� MapInfo ���� �����´�.
	MapInfo info;
	switch (state) {
	case SceneState::MAP1: info = GetMap1Info(); break;
	case SceneState::MAP2: info = GetMap2Info(); break;
	default: return; // LANDING �� ������ ī�޶� �����Ѵ�.
	}
	m_pCamera->GenerateViewMatrix(info.cameraPosition, info.cameraLookAt, XMFLOAT3(0.0f, 1.0f, 0.0f));
	// ���� �ִ� �÷��̾� ��ġ�� ī�޶� ���� ��ġ�� �ʱ�ȭ.
	m_xmf3PlayerPos = info.cameraPosition;
	// ������ �� �� ���� �� �׻� FPS �� ����.
	m_pCamera->SetMode(ECameraMode::FPS);
	if (m_pScene) m_pScene->SetPlayerVisible(false);
	// ���� ProcessInput ���� �ٽ� ���콺 ĸó�� ���۵ǰ� �Ѵ�.
	m_bMouseCaptured = false;
	// ���� ���� �ʱ�ȭ.
	m_fVerticalVelocity = 0.0f;
	m_bGrounded = true;
	// Bullets fired on the previous map should not carry over; the cooldown
	// is per-life, so reset it too.
	m_fFireCooldown = 0.0f;

	// 라이프 / 적 / 동적 객체 리셋. 이전 게임의 잔존 객체를 정리하고 새 적을 스폰한다.
	// LANDING → MAP_SELECT → MAP1/2 로 들어오는 모든 경로에서 호출되므로,
	// 죽고 다시 시작하는 흐름과 첫 게임 시작 흐름이 동일하게 동작한다.
	m_nPlayerLife = 10;
	m_bResetPending = false;
	if (m_pScene) m_pScene->ResetGameplayState();
	SpawnEnemiesForMap(state);

	// 플레이어 모델의 위치를 새 스폰 위치로 즉시 동기화하여, 첫 프레임 충돌 판정이
	// 이전 위치를 참조하지 않게 한다 (Scene::AnimateObjects 의 EnemyBullet × Player).
	if (m_pPlayerModel) {
		const XMFLOAT3 modelCenter{
			m_xmf3PlayerPos.x,
			m_xmf3PlayerPos.y - MAP_EYE_HEIGHT + 1.3f,
			m_xmf3PlayerPos.z };
		m_pPlayerModel->SetWorldYawAndPosition(m_pCamera->GetYaw(), modelCenter);
	}
}

void CGameFramework::SetupMapSelectCamera()
{
	if (!m_pCamera) return;
	// �� ���� ȭ�� ī�޶�: ȭ�� �ణ ������ �̴Ͼ�ó���� ����.
	m_pCamera->GenerateViewMatrix(
		XMFLOAT3(0.0f, 5.0f, -55.0f),
		XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f));
	// ���콺 ĸó�� �����ϰ� Ŀ���� ���̰� �Ѵ�.
	if (m_bMouseCaptured) {
		::ShowCursor(TRUE);
		m_bMouseCaptured = false;
	}
	// ���� ���� �ʱ�ȭ.
	m_fVerticalVelocity = 0.0f;
	m_bGrounded = true;
}

void CGameFramework::WaitForGPUComplete()
{
	// ���� ����ۿ� ���� Fence Value �� 1 ����.
	UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];

	// cmdQueue �� �ñ׳��� �ɾ�, GPU �� �ű���� ��� ó���ϸ� �潺 ���� nFenceValue �� �ǵ���.
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);

	// ���� �潺�� nFenceValue �� �������� ���ߴٸ� �̺�Ʈ�� ���.
	if (m_pd3dFence->GetCompletedValue() < nFenceValue) {
		hResult = m_pd3dFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

void CGameFramework::MoveToNextFrame()
{
	m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

	UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);

	if (m_pd3dFence->GetCompletedValue() < nFenceValue) {
		hResult = m_pd3dFence->SetEventOnCompletion(nFenceValue, m_hFenceEvent);
		::WaitForSingleObject(m_hFenceEvent, INFINITE);
	}
}

void CGameFramework::FrameAdvance()
{
	// ������ �ð� ����, �Է�/�ִϸ��̼�/������ ����.
	m_GameTimer.Tick(0.0f);

	ProcessInput();

	AnimateObjects();

	// Ŀ�ǵ� �Ҵ���/����Ʈ ����.
	HRESULT hResult = m_pd3dCommandAllocator->Reset();
	hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL);

	// ���� ����۸� PRESENT ���¿��� RENDER_TARGET ���·� ��ȯ.
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource =
		m_ppd3dSwapChainBackBuffers[m_nSwapChainBufferIndex].Get();
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	// ������� RTV CPU �ڵ��� �����´�.
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (m_nSwapChainBufferIndex * m_nRtvDescriptorIncrementSize);

	// ����-���ٽ� ��ũ������ CPU �ڵ��� �����´�.
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_pd3dCommandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, FALSE,
		&d3dDsvCPUDescriptorHandle);

	float pfClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	m_pd3dCommandList->ClearRenderTargetView(
		d3dRtvCPUDescriptorHandle,
		pfClearColor,
		0,
		NULL
	);

	// ����/���ٽ� Ŭ����.
	m_pd3dCommandList->ClearDepthStencilView(
		d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		NULL
	);

	// �� ������.
	if (m_pScene) m_pScene->Render(m_pd3dCommandList.Get(), m_pCamera.get());

	// 십자선은 실제 플레이 씬(MAP1/MAP2)에서만 그린다.
	// CGameObject::Render 가 CHudShader 의 PSO 로 전환한 뒤 메시를 그리므로
	// 깊이 테스트가 꺼져 있어 항상 다른 모든 픽셀 위에 표시된다.
	if (m_pCrosshair && m_pScene && m_pCamera) {
		const SceneState st = m_pScene->GetCurrentState();
		if (st == SceneState::MAP1 || st == SceneState::MAP2) {
			m_pCrosshair->Render(m_pd3dCommandList.Get(), m_pCamera.get());

			// 라이프 바: 화면 중앙 하단. m_nPlayerLife 개수만큼 앞에서부터 렌더한다.
			// 잃은 칸은 그리지 않는 단순 방식. CHudShader 를 공유하므로 PSO 전환 비용 없음.
			const int nDraw = (m_nPlayerLife < 0) ? 0 : (m_nPlayerLife > 10 ? 10 : m_nPlayerLife);
			for (int i = 0; i < nDraw && i < static_cast<int>(m_pLifeBarSegments.size()); ++i) {
				if (m_pLifeBarSegments[i]) {
					m_pLifeBarSegments[i]->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}
		}
	}

	// PRESENT ���·� �ٽ� ��ȯ.
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	// ���� ����Ʈ ����.
	hResult = m_pd3dCommandList->Close();

	// ���� ť ����.
	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(
		_countof(ppd3dCommandLists),
		ppd3dCommandLists
	);

	// GPU �Ϸ� ���.
	WaitForGPUComplete();

	// ������ ǥ�� �� FPS ĸ�� ����.
	m_pdxgiSwapChain->Present(0, 0);
	MoveToNextFrame();
	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}

CGameFramework::~CGameFramework()
{

}
