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

// 윈도우 초기화 시 D3D12 디바이스/스왑체인 등을 한 번에 셋업한다.
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
	// GPU 가 모든 작업을 종료할 때까지 대기한 후 자원을 해제.
	WaitForGPUComplete();

	// 명령 객체(할당자와 리스트) 해제.
	ReleaseObjects();

	::CloseHandle(m_hFenceEvent);

	// swapchain 자원과 깊이 스텐실 뷰 등 해제.
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

	// 디스플레이 모드를 토글한다 (윈도우/풀스크린).
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

	// 시스템에 존재하는 어댑터 중 D3D12 기능 수준 12.0 을 지원하는 첫 어댑터를 찾는다.
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != m_pdxgiFactory->EnumAdapters1(i, &pd3dAdapter); ++i)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
		pd3dAdapter->GetDesc1(&dxgiAdapterDesc);

		if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		// ID3D12Device 생성 시도.
		if (SUCCEEDED(D3D12CreateDevice(
			pd3dAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_pd3dDevice)
		)))
			break;
	}

	// 12.0 지원이 안 되면 WARP(소프트웨어) 어댑터를 사용.
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
	d3dMsaaQualityLevels.SampleCount = 4;	// MSAA 4x 샘플링
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;
	m_pd3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&d3dMsaaQualityLevels,
		sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS)
	);

	// 디바이스가 지원하는 다중 샘플링 품질 수준을 저장한다.
	m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;

	// 펜스 생성, 초깃값 0.
	hResult = m_pd3dDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pd3dFence)
	);
	m_nFenceValue = 0;

	// 펜스용 이벤트 핸들 생성 (수동 리셋 FALSE, 초깃값 FALSE).
	m_hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

void CGameFramework::CreateCommandQueueAndList()
{
	// Direct 명령 큐 생성.
	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hResult = m_pd3dDevice->CreateCommandQueue(
		&d3dCommandQueueDesc,
		IID_PPV_ARGS(&m_pd3dCommandQueue)
	);

	// 명령 할당자 생성.
	hResult = m_pd3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&m_pd3dCommandAllocator)
	);

	// 명령 리스트 생성.
	hResult = m_pd3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pd3dCommandAllocator.Get(),
		NULL,
		IID_PPV_ARGS(&m_pd3dCommandList)
	);

	// 명령 리스트는 처음 생성 시 Open 상태이므로 Close 로 닫아둔다.
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

	// 스왑체인 백 버퍼마다 RTV 디스크립터 뷰.
	HRESULT hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dRtvDescriptorHeap)
	);

	m_nRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// DSV 디스크립터 힙(1개).
	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dDsvDescriptorHeap)
	);

	m_nDsvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

// 백버퍼의 각 페이지에 대해 RTV 를 생성.
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

	// 깊이-스텐실 버퍼 리소스 생성.
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

	// 깊이-스텐실 뷰 생성.
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

	// 게임 씬과 카메라 - 시작은/메시/시그니처/적 빌드.
	m_pCamera = std::make_unique<CCamera>();
	m_pCamera->SetViewport(0, 0, m_nWndClientWidth, m_nWndClientHeight, 0.0f, 1.0f);
	m_pCamera->SetScissorRect(0, 0, m_nWndClientWidth, m_nWndClientHeight);
	// [Claude] 근평면을 1.0 → 0.1 로 변경. 플레이어 모델이 카메라 앞(가까운 위치 큐브 등)
	// 에 거의 닿을 때 잘려 보이는 현상을 막는다. 0.1 로 줄여도 far=500 이라
	// ratio 가 5000:1 → 24-bit depth buffer 정밀도에 문제가 없음.
	m_pCamera->GenerateProjectionMatrix(0.1f, FRUSTUM_FAR_PLANE,
		float(m_nWndClientWidth) / float(m_nWndClientHeight), 90.0f);
	m_pCamera->GenerateViewMatrix(XMFLOAT3(0.0f, 0.0f, -50.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f));

	// 씬 객체 빌드 후 각 카메라를 해당 씬에 맞게 셋업.
	m_pScene = std::make_unique<CScene>();
	m_pScene->BuildObjects(m_pd3dDevice.Get(), m_pd3dCommandList.Get());

	// TPS 미니어처를 위한 공유 메시 풀(맵용 큐브). 색/크기/위치는 호출 시점에서 결정한다.
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
	// 게임플레이 메시: 작은 적 큐브 (적의 본 메시)
	m_pBulletMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.4f, 0.4f, 0.4f, true, XMFLOAT4(1.0f, 0.9f, 0.2f, 1.0f));

	// 적 총알: 흰색 작은 큐브 같은 메시.
	m_pEnemyBulletMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.4f, 0.4f, 0.4f, true, XMFLOAT4(1.0f, 0.25f, 0.20f, 1.0f));

	// 적 마커 기둥: 머리 위 길쭉한 노란 큐브. 잔여수가(=3) 이 되면 표시 토글.
	m_pEnemyMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		1.2f, 2.6f, 1.2f, true, XMFLOAT4(0.45f, 0.15f, 0.55f, 1.0f));

	// 소총 메시: 외부에서 OBJ 로딩 (어두운 회색). 기본은 모델의 정면 = +Z 방향이 정점.
	// [Claude] 근평면을 0.1 로 줄였으므로 소총 메시 두께(1.2)/위치(1.7 forward)와
	// 거리 차가 충분해 자연스럽게 그려진다. 1.7 - 0.6 = 1.1 >> 0.1.
	// [Claude] 플레이어 소총 ? Resources/M16.obj 로드. 베이크 변환:
	//   1) 모델 중심을 원점으로 이동 (M16 X 중심 -1.81, Y 중심 0.17)
	//   2) Y축 +π/2 회전: 모델 -X(총구 Front 끝, X?-6.74) → 엔진 +Z(forward)
	//   3) 스케일 0.122: 모델 X-span 9.85 → 게임 단위 ~1.2 (기존 막대기와 동등)
	// ※ 방향/크기가 어긋나면 이 m16Xform 만 손보면 된다.
	XMFLOAT4X4 m16Xform;
	XMStoreFloat4x4(&m16Xform,
		XMMatrixTranslation(1.815f, -0.17f, 0.0f) *
		XMMatrixRotationY(+XM_PIDIV2) *
		XMMatrixScaling(0.24f, 0.24f, 0.24f));
	m_pRifleMesh = std::make_shared<CObjMesh>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		L"../Resources/M16.obj",
		XMFLOAT4(0.25f, 0.25f, 0.28f, 1.0f),
		m16Xform);

	// 플레이어 객체 생성. 소총 객체(CGameObject)에 매시 부착해
	// m_pPlayer->GetRifle() 을 통해 접근하도록 함.
	m_pPlayer = std::make_shared<CPlayer>();
	{
		auto pRifleObj = std::make_shared<CGameObject>();
		pRifleObj->SetMesh(m_pRifleMesh);
		m_pPlayer->SetRifleObject(std::move(pRifleObj));
	}
	m_pPlayer->SetPosition(XMFLOAT3{ 0.0f, MAP_EYE_HEIGHT, 0.0f });

	// 적 총알 발사: 플레이어 카메라 콜백을 람다/펑션. 적 인스턴스마다 주입한다.
	// [Claude] 적 소총 ? Resources/AK47.obj 로드. 베이크 변환:
	//   1) 모델 중심을 원점으로 이동 (AK47 X 중심 -0.02, Y 중심 -0.14)
	//   2) Y축 -π/2 회전: 모델 +X(총구 Front 끝, X?+5.06) → 엔진 +Z(forward)
	//      ※ M16 과 반대 방향임에 주의 (Blender export 시 모델별로 다름).
	//   3) 스케일 0.118: 모델 X-span 10.17 → 게임 단위 ~1.2
	// ※ 방향/크기가 어긋나면 이 ak47Xform 만 손보면 된다.
	XMFLOAT4X4 ak47Xform;
	XMStoreFloat4x4(&ak47Xform,
		XMMatrixTranslation(0.02f, 0.14f, 0.0f) *
		XMMatrixRotationY(-XM_PIDIV2) *
		XMMatrixScaling(0.24f, 0.24f, 0.24f));
	m_pEnemyRifleMesh = std::make_shared<CObjMesh>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		L"../Resources/AK47.obj",
		XMFLOAT4(0.20f, 0.20f, 0.22f, 1.0f),
		ak47Xform);

	// 적 마커 기둥 메시: 잔여수 3 이하 시 머리 위. 적당히 큰 길이 노출되도록 길게.
	// [Claude] 메시 4.0 → 14.0 으로 키워 3.5 만 보이게. 벽 높이가 8.0 이므로
	// 14.0 만큼의 마커 길이의 절반인 약 7.0 이 헤드보다 위 약 5.0 정도까지
	// 솟아 보이게 하여 벽 위로 빠져나오게 한다.
	m_pEnemyMarkerMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.15f, 14.0f, 0.15f, true, XMFLOAT4(1.0f, 0.85f, 0.1f, 1.0f));

	// HUD 셰이더 (조준점 + 라이프바 등 공유). 깊이 OFF, 컬링 OFF.
	// CreateShader 는 자체 오버라이드 base 호출이지만 CHudShader::CreateShader 가 적용된다.
	m_pHudShader = std::make_shared<CHudShader>();
	if (m_pScene) {
		m_pHudShader->CreateShader(m_pd3dDevice.Get(), m_pScene->GetGraphicsRootSignature());
	}

	// 피격 비네트 오버레이용 셰이더 + 별도의 NDC 쿼드. 깊이/컬링 OFF + alpha blend ON.
	// m_fHitFlash > 0 일 때만 1번 드로우.
	m_pHitOverlayShader = std::make_shared<CHitOverlayShader>();
	if (m_pScene) {
		m_pHitOverlayShader->CreateShader(m_pd3dDevice.Get(), m_pScene->GetGraphicsRootSignature());
	}
	{
		auto pOverlayMesh = std::make_shared<CHudQuadMesh>(
			m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
			-1.0f, 1.0f, 1.0f, -1.0f,                     // NDC 풀스크린 (PS 가 거의 외곽으로 vertex color 는 사용 X)
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		m_pHitOverlayQuad = std::make_shared<CGameObject>();
		m_pHitOverlayQuad->SetMesh(pOverlayMesh);
		// 별도로 바인딩할 모델은 없지만 일관성을 위해 m_pHitOverlayShader->Render 에서 호출
		// (b3 상수 매번 set 후 호출 한 번 한다). 외부 m_pHitOverlayQuad 가 SetShader 처럼 묶는다.
	}

	// 화면 중앙에 고정된 십자선(+) 조준점. 정점이 NDC(클립 공간) 좌표로 직접
	// 저장 되어있어 월드/투영/뷰를 거치지 않고 항상 화면 정중앙에 고정된다.
	{
		auto pCrosshairMesh = std::make_shared<CCrosshairMesh>(
			m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
			UINT(m_nWndClientWidth), UINT(m_nWndClientHeight),
			10u, 2u, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		m_pCrosshair = std::make_shared<CGameObject>();
		m_pCrosshair->SetMesh(pCrosshairMesh);
		m_pCrosshair->SetShader(m_pHudShader);
	}

	// 라이프 바 칸 10개 생성. 모든 칸을 미리 만들어(인스턴스)두고, 매 프레임 m_pPlayer->GetHP()
	// 개수만큼 앞에서부터 그린다고 가시화 제어한다.
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

	// 적 잔여 수 점 카운트 (좌상단). 최대 적 수 12 만큼 미리 생성하고 살아있는 수
	// 만큼만 앞에서부터 그린다. CHudQuadMesh 로 NDC 좌표에 사각 배치.
	{
		const float kPipSizePx = 14.0f;
		const float kGapPx     = 6.0f;
		const float kLeftPx    = 24.0f;
		const float kTopPx     = 24.0f;
		const float wPx = float(m_nWndClientWidth  > 0 ? m_nWndClientWidth  : 1);
		const float hPx = float(m_nWndClientHeight > 0 ? m_nWndClientHeight : 1);
		const float pipNdcW = kPipSizePx * 2.0f / wPx;
		const float pipNdcH = kPipSizePx * 2.0f / hPx;
		const float gapNdc  = kGapPx     * 2.0f / wPx;
		const float leftNdc = kLeftPx    * 2.0f / wPx;
		const float topNdc  = kTopPx     * 2.0f / hPx;
		const float xLBase = -1.0f + leftNdc;
		const float yT     =  1.0f - topNdc;
		const float yB     =  yT - pipNdcH;

		m_pCountPips.reserve(12);
		for (int i = 0; i < 12; ++i) {
			const float xL = xLBase + float(i) * (pipNdcW + gapNdc);
			const float xR = xL + pipNdcW;
			auto pPipMesh = std::make_shared<CHudQuadMesh>(
				m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
				xL, xR, yT, yB,
				XMFLOAT4(0.65f, 0.25f, 0.80f, 1.0f));
			auto pPipObj = std::make_shared<CGameObject>();
			pPipObj->SetMesh(pPipMesh);
			pPipObj->SetShader(m_pHudShader);
			m_pCountPips.push_back(std::move(pPipObj));
		}
	}

	// 승리 화면용 'WIN' ? 7-세그먼트 스타일로 W/I/N 3 글자를 NDC 직사각형 조합으로 표현.
	// 각 글자의 화면 중앙 가로/세로 픽셀을 정해 NDC 로 환산, 굵은 막대(굵은 선)로 표현한다.
	{
		const float wPx = float(m_nWndClientWidth  > 0 ? m_nWndClientWidth  : 1);
		const float hPx = float(m_nWndClientHeight > 0 ? m_nWndClientHeight : 1);
		const float kLetterW = 80.0f;       // 한 글자 폭(px)
		const float kLetterH = 120.0f;      // 한 글자 높이(px)
		const float kStroke  = 14.0f;       // ? ?β?(px)
		const float kLetterGap = 30.0f;     // 글자 사이 간격(px)
		const XMFLOAT4 col(1.0f, 0.85f, 0.20f, 1.0f);

		// 화면 중심(화면 좌표) 기준 시작 가로 x(px).
		const float totalW = kLetterW * 3.0f + kLetterGap * 2.0f;
		const float startXPx = (wPx - totalW) * 0.5f;
		const float topYPx   = hPx * 0.5f - kLetterH * 0.5f - hPx * 0.05f;

		auto pxToNdcX = [&](float px) { return px * 2.0f / wPx - 1.0f; };
		auto pxToNdcY = [&](float px) { return 1.0f - px * 2.0f / hPx; };

		auto addQuad = [&](float xLpx, float xRpx, float yTpx, float yBpx) {
			const float xL = pxToNdcX(xLpx);
			const float xR = pxToNdcX(xRpx);
			const float yT = pxToNdcY(yTpx);
			const float yB = pxToNdcY(yBpx);
			auto pMesh = std::make_shared<CHudQuadMesh>(
				m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
				xL, xR, yT, yB, col);
			auto pObj = std::make_shared<CGameObject>();
			pObj->SetMesh(pMesh);
			pObj->SetShader(m_pHudShader);
			m_pWinLetters.push_back(std::move(pObj));
		};

		// W (4 stroke): 좌세로, 우세로, 가운데 대각 두 개 ? 가는 세로 + 가운데 짧은 두 막대.
		{
			const float L = startXPx;
			const float R = L + kLetterW;
			const float T = topYPx;
			const float B = topYPx + kLetterH;
			// 좌 세로
			addQuad(L, L + kStroke, T, B);
			// 우 세로
			addQuad(R - kStroke, R, T, B);
			// 가운데 짧은 막대 (가로폭의 1/3)
			const float midX = (L + R) * 0.5f;
			addQuad(midX - kStroke * 0.5f, midX + kStroke * 0.5f, B - kLetterH * 0.55f, B);
			// 안쪽 막대
			addQuad(L, R, B - kStroke, B);
		}
		// I (3 stroke): 가운데 세로 + 상단 가로 + 하단 가로
		{
			const float L = startXPx + kLetterW + kLetterGap;
			const float R = L + kLetterW;
			const float T = topYPx;
			const float B = topYPx + kLetterH;
			const float midX = (L + R) * 0.5f;
			// 세로
			addQuad(midX - kStroke * 0.5f, midX + kStroke * 0.5f, T, B);
			// 상단 가로
			addQuad(L + kStroke, R - kStroke, T, T + kStroke);
			// 하단 가로
			addQuad(L + kStroke, R - kStroke, B - kStroke, B);
		}
		// N (3 stroke): 좌세로 + 우세로 + 대각선 ? 직사각형 두 두께를 이어서 표현
		{
			const float L = startXPx + (kLetterW + kLetterGap) * 2.0f;
			const float R = L + kLetterW;
			const float T = topYPx;
			const float B = topYPx + kLetterH;
			// 좌 세로
			addQuad(L, L + kStroke, T, B);
			// 우 세로
			addQuad(R - kStroke, R, T, B);
			// 가운데 대각으로 보이는 두 가는 막대 + 빈칸 두 줄로 마지막에는 짧은 가로 가로
			addQuad(L, R, T, T + kStroke);
		}
	}

	// 플레이어 피격 시 콜백. Scene 에서 EnemyBullet 이 Player 충돌 시 호출한다.
	if (m_pScene) {
		m_pScene->SetOnPlayerHit([this]() {
			if (!m_pPlayer) return;
			// [Claude] 이미 사망 시퀀스 진행 중이면 추가 피격 무시.
			if (m_fPlayerDeathTimer >= 0.0f) return;
			if (m_pPlayer->GetHP() > 0) m_pPlayer->TakeDamage(1);
			// [Claude] HP 0 시 즉시 reset 대신 사망 타이머 시작 - 카메라 기울임 + 붉은 화면.
			if (m_pPlayer->GetHP() <= 0) m_fPlayerDeathTimer = kPlayerDeathDuration;
			// 이미 죽음이 진행 중이라면 무시한 채 FrameAdvance 가 매 프레임 카운트다운한다.
			m_fHitFlash = 1.0f;
		});
	}

	// 게임의 명령 리스트를 닫고 명령 큐에 제출해 GPU 가 처리 가능한 상태로 만든다.
	m_pd3dCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	// GPU 가 종료할 때 까지 메인 스레드가 대기한다.
	WaitForGPUComplete();

	// 업로드용 임시 리소스를 해제(이제 메모리 부족 해소).
	if (m_pScene) m_pScene->ReleaseUploadBuffers();
	if (m_pBulletMesh) m_pBulletMesh->ReleaseUploadBuffers();
	if (m_pEnemyBulletMesh) m_pEnemyBulletMesh->ReleaseUploadBuffers();
	if (m_pEnemyMesh) m_pEnemyMesh->ReleaseUploadBuffers();
	if (m_pCrosshair) m_pCrosshair->ReleaseUploadBuffers();
	for (auto& pSeg : m_pLifeBarSegments) {
		if (pSeg) pSeg->ReleaseUploadBuffers();
	}
	if (m_pRifleMesh) m_pRifleMesh->ReleaseUploadBuffers();
	if (m_pEnemyMarkerMesh) m_pEnemyMarkerMesh->ReleaseUploadBuffers();
	for (auto& pPip : m_pCountPips) {
		if (pPip) pPip->ReleaseUploadBuffers();
	}
	for (auto& pLetter : m_pWinLetters) {
		if (pLetter) pLetter->ReleaseUploadBuffers();
	}
	if (m_pHitOverlayQuad) m_pHitOverlayQuad->ReleaseUploadBuffers();

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
				// [Claude] 사망 시퀀스 중엔 발사 차단.
				if (m_fPlayerDeathTimer < 0.0f) FireBullet();
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
			// F9: 윈도우드/풀화면 토글.
			ChangeSwapChainState();
			break;
		case 'V':
			// V: FPS / TPS 모드 토글 (요구사항 C). 게임플레이 씬에서만 효과가 발생한다.
			// [Claude] 사망 시퀀스 중엔 시점 전환 금지 (애니메이션 방해 방지).
			if (m_pCamera && m_pScene && m_fPlayerDeathTimer < 0.0f) {
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

	// [Claude] 사망 시퀀스 중엔 모든 입력(이동/시점/사격) 차단. 마우스 캡쳐는
	// 그대로 두어 화면이 튀지 않게 한다. 사격 쿨다운만 계속 진행시켜 복귀 후
	// 첫 클릭이 자연스럽게 발사되도록 함.
	if (m_fPlayerDeathTimer >= 0.0f) {
		if (m_fFireCooldown > 0.0f) m_fFireCooldown -= m_GameTimer.GetTimeElapsed();
		return;
	}

	const SceneState state = m_pScene->GetCurrentState();
	// 1인칭/3인칭 입력(스페이스 점프, WASD, 마우스) 처리는 MAP1/MAP2 에서만 동작한다.
	const bool bGameplay = (state == SceneState::MAP1 || state == SceneState::MAP2);
	if (!bGameplay) {
		// LANDING/MAP_SELECT: 스페이스 키는 무시, 클릭만 받음.
		if (m_bMouseCaptured) {
			::ShowCursor(TRUE);
			m_bMouseCaptured = false;
		}
		return;
	}

	// Tick the shoot cooldown so the next click can fire when this hits 0.
	if (m_fFireCooldown > 0.0f) m_fFireCooldown -= m_GameTimer.GetTimeElapsed();

	// 입력 시작 시점에 발 위치를 CPlayer 에 동기화.
	if (m_pPlayer) m_pPlayer->TickRecoil(m_GameTimer.GetTimeElapsed(), kRecoilDuration);

	const bool bForeground = (::GetForegroundWindow() == m_hWnd);

	// =============== 스페이스 키 ===============
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
				const float fYaw   = XMConvertToRadians(static_cast<float>(dx)  * kSensitivityDeg);
				// 마우스 Y 델타(아래 양수=양수)와 pitch 부호(위=양수)가 반대이므로 부호 반전.
				// 우리는 Rotate(-fPitch, ...) 로 호출하여 위로 보면 시야가 위로 향하게 한다.
				const float fPitch = XMConvertToRadians(static_cast<float>(-dy) * kSensitivityDeg);
				m_pCamera->Rotate(fPitch, fYaw);
				::SetCursorPos(m_ptWndCenterScreen.x, m_ptWndCenterScreen.y);
			}
			m_ptWndCenterScreen = ptCenter;
		}
	}

	// =============== WASD + 카메라 룩 (XZ 평면) ===============
	// 게임의 좌표 계는 모두 m_pPlayer 가 보관 (GetPosition/SetPosition). TPS 일 때
	// 카메라는 그 위치 뒤로 어깨너머 거리에 있지만, 이동/충돌/점프는 모두 플레이어가
	// 기준이 되어 동작한다.
	const float dt = m_GameTimer.GetTimeElapsed();
	const float kMoveSpeed = 6.0f;
	const float kStep = kMoveSpeed * dt;

	float fForward = 0.0f, fStrafe = 0.0f;
	if (::GetAsyncKeyState('W') & 0x8000) fForward += 1.0f;
	if (::GetAsyncKeyState('S') & 0x8000) fForward -= 1.0f;
	if (::GetAsyncKeyState('D') & 0x8000) fStrafe  += 1.0f;
	if (::GetAsyncKeyState('A') & 0x8000) fStrafe  -= 1.0f;

	XMFLOAT3 pos = m_pPlayer->GetPosition();
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

	// =============== 점프 + 중력 (Y) ===============
	// 점프 정점: 3 * STEP_H (~2.1) 단차 통과. v0 = sqrt(2 * g * h).
	const float kGravity = 30.0f;
	const float kJumpApex = 3.0f * 0.7f; // STEP_H = 0.7
	const float kJumpV0 = sqrtf(2.0f * kGravity * kJumpApex);

	const bool bSpaceNow = (::GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	if (bSpaceNow && !m_bPrevSpacePressed && m_pPlayer->IsGrounded()) {
		m_pPlayer->SetVerticalVelocity(kJumpV0);
		m_pPlayer->SetGrounded(false);
	}
	m_bPrevSpacePressed = bSpaceNow;

	const float floorYAtNext = GetFloorHeightAt(state, next.x, next.z);
	const float groundY = floorYAtNext + MAP_EYE_HEIGHT;

	if (!m_pPlayer->IsGrounded()) {
		// 공중: 중력 적용, y 업데이트.
		float vy = m_pPlayer->GetVerticalVelocity() - kGravity * dt;
		m_pPlayer->SetVerticalVelocity(vy);
		next.y = pos.y + vy * dt;
		if (next.y <= groundY) {
			next.y = groundY;
			m_pPlayer->SetVerticalVelocity(0.0f);
			m_pPlayer->SetGrounded(true);
		}
	}
	else {
		// 지상: 바닥 높이 추적 적용 (단차 자동올림은 미동/충돌).
		next.y = groundY;
	}

	// 점프 물리 누적값을 CPlayer 에 동기화한다.
	m_pPlayer->SetPosition(next);

	// 카메라 모드 별로 위치 갱신.
	const XMFLOAT3 playerPos = m_pPlayer->GetPosition();
	if (m_pCamera->GetMode() == ECameraMode::FPS) {
		// FPS: 카메라 위치 = 플레이어의 눈 높이.
		m_pCamera->SetPosition(playerPos);
		// Keep the (invisible) player model aligned with camera yaw so that
		// flipping to TPS looks consistent immediately.
		if (m_pPlayerModel) {
			XMFLOAT3 modelCenter{
				playerPos.x,
				playerPos.y - MAP_EYE_HEIGHT + 1.3f,
				playerPos.z };
			m_pPlayerModel->SetWorldYawAndPosition(m_pCamera->GetYaw(), modelCenter);
		}
	}
	else {
		// TPS: 플레이어의 뒤쪽 kBack 거리, 위로 kUp 만큼 떨어진 어깨너머 위치 계산.
		// Spring-arm: clamp the camera-to-player distance if a wall is in
		// the way so the camera does not pop through level geometry.
		// TPS sphere orbit: yaw=수평, pitch=수직 각도 누적. 그리고 좌측 어깨너머에 배치한다.
		// TPS: 어깨 너머(over-the-shoulder) 시점.
		// 카메라의 yaw/pitch 누적은 Rotate() 처럼 자체 FPS 와 비슷하게 사용하되,
		// look 벡터는 직접 다시는 셋팅하지 않는다. 카메라가 플레이어 모델 너머를 본다.
		// TPS 카메라 궤도: yaw=수평, pitch=수직 각도 누적. 카메라는 항상 플레이어
		// 모델보다 뒤쪽에서 플레이어를 본다(어깨 너머 시점). 발사 방향은 카메라의
		// yaw/pitch 로부터 직접 계산한 카메라 정면 벡터를 사용한다(FireBullet 참조).
		const float kBack = 3.0f;   // 플레이어 뒤 카메라 거리 오프셋
		const float kUp   = 1.2f;   // 어깨 높이 오프셋 (눈 약간 위)

		const float yaw   = m_pCamera->GetYaw();
		const float pitch = m_pCamera->GetPitch();

		// 카메라 궤도 계산용 벡터 (수평 평면용). pitch 는 별도이고 yaw 만 사용.
		const XMFLOAT3 backDir{ -sinf(yaw), 0.0f, -cosf(yaw) };

		// 수평 거리 분량 중 가로 분량(원거리 * cos pitch), 수직 분량(원거리 * sin pitch).
		// [Claude] vertBack 부호 주의. 카메라가 마우스를 위로 올리면(pitch +) 시점이
		// 플레이어보다 위로 가야 SetPositionAndTarget 으로 플레이어를 내려다보는
		// 모드, 그렇게 보이도록 카메라가 위로 올라가게 한다. -sin(pitch) 을 빼서
		// 플레이어 모델을 부드럽게 따라 도는 듯한 자연스러운 FPS 의 효과를 흉내낸다.
		const float horizBack = kBack * cosf(pitch);
		const float vertBack  = -kBack * sinf(pitch);
		const float eyeY      = playerPos.y + vertBack + kUp;

		// 모델-플레이어 사이 가는 컬러를 보고 카메라가 플레이어를 향하도록 만든다.
		const float dist = ClampDistanceAgainstWalls(
			state, playerPos, backDir, horizBack, eyeY);

		XMFLOAT3 tpsEye{
			playerPos.x + backDir.x * dist,
			eyeY,
			playerPos.z + backDir.z * dist };

		// [Claude] 직접 forward 와 m_fYaw/m_fPitch 누적 계산은 카메라가 처리한다.
		// SetPositionAndTarget(tpsEye, playerPos) 로 카메라가 직접 플레이어
		// 모델을 보고 있게 만든 다음 카메라 자체의 방향(내부 forward) 도 다시
		// 갱신해 미세한 누적값(yaw/pitch) 도 동기화한다. SetPosition 후 호출하는
		// RegenerateViewMatrix 가 yaw/pitch 로부터 look 을 재계산하지 않으므로
		// 별도 보정 단계가 필요하다 (플레이어 위치 추적 보정 두 단계로 분리).
		m_pCamera->SetPosition(tpsEye);

		if (m_pPlayerModel) {
			XMFLOAT3 modelCenter{
				playerPos.x,
				playerPos.y - MAP_EYE_HEIGHT + 1.3f,
				playerPos.z };
			m_pPlayerModel->SetWorldYawAndPosition(m_pCamera->GetYaw(), modelCenter);
		}
	}

	// 카메라 위치/look 을 시작점으로 광선 transform 을 따라간다.
	UpdateRifleTransform();
}

XMFLOAT3 CGameFramework::GetAimTargetPoint() const
{
	// 원점 좌표 + look 방향으로 직선을 쏴 첫 충돌점을 찾는다.
	// 벽: ClampDistanceAgainstWalls 재사용 (수평 마칭으로 벽이 첫 3D 충돌점 반환).
	// 적 AABB: 슬랩 라인테스트 (3축 동시평가 ? Real-Time Rendering 식 적용).
	// 두 결과 중에서 가까운 것 채택. 둘 다 거리 안에 없으면 kMaxAimDist 폴백.
	constexpr float kMaxAimDist = 200.0f;
	if (!m_pCamera || !m_pScene) {
		return XMFLOAT3{ 0.0f, 0.0f, 0.0f };
	}
	const XMFLOAT3 origin = m_pCamera->GetPosition();
	const float yaw = m_pCamera->GetYaw();
	const float pitch = m_pCamera->GetPitch();
	const float cp = cosf(pitch);
	const XMFLOAT3 dir{ sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp };
	const SceneState st = m_pScene->GetCurrentState();

	// (1) 벽까지의 거리. ClampDistanceAgainstWalls 는 수평 마칭이라 거리는 XZ 평면의
	// 길이를 기준으로 한 3D 거리 길이로 잰다. 카메라가 수직 발사라면(horizLen ? 0) 은
	// 충돌이 거의 없으므로 kMaxAimDist 폴백.
	float wallDist = kMaxAimDist;
	{
		const float horizLen = sqrtf(dir.x * dir.x + dir.z * dir.z);
		if (horizLen > 1e-3f) {
			const XMFLOAT3 dirXZ{ dir.x / horizLen, 0.0f, dir.z / horizLen };
			const float horizClamped = ClampDistanceAgainstWalls(
				st, origin, dirXZ, kMaxAimDist * horizLen, origin.y);
			wallDist = horizClamped / horizLen;
		}
	}

	// (2) 적 AABB 별로 광선 첫 충돌 비교한다.
	float enemyDist = kMaxAimDist;
	if (st == SceneState::MAP1 || st == SceneState::MAP2) {
		const auto enemyAABBs = m_pScene->GetAliveEnemyAABBs();
		const float o[3] = { origin.x, origin.y, origin.z };
		const float d[3] = { dir.x,   dir.y,   dir.z };
		for (const auto& cb : enemyAABBs) {
			const XMFLOAT3& c = cb.first;
			const XMFLOAT3& h = cb.second;
			const float bmin[3] = { c.x - h.x, c.y - h.y, c.z - h.z };
			const float bmax[3] = { c.x + h.x, c.y + h.y, c.z + h.z };
			float tmin = 0.0f;
			float tmax = kMaxAimDist;
			bool hit = true;
			for (int i = 0; i < 3; ++i) {
				if (fabsf(d[i]) < 1e-6f) {
					if (o[i] < bmin[i] || o[i] > bmax[i]) { hit = false; break; }
				}
				else {
					float t1 = (bmin[i] - o[i]) / d[i];
					float t2 = (bmax[i] - o[i]) / d[i];
					if (t1 > t2) std::swap(t1, t2);
					if (t1 > tmin) tmin = t1;
					if (t2 < tmax) tmax = t2;
					if (tmin > tmax) { hit = false; break; }
				}
			}
			if (hit && tmin < enemyDist) enemyDist = tmin;
		}
	}

	const float t = (wallDist < enemyDist) ? wallDist : enemyDist;
	return XMFLOAT3{
		origin.x + dir.x * t,
		origin.y + dir.y * t,
		origin.z + dir.z * t };
}

void CGameFramework::UpdateRifleTransform()
{
	if (!m_pPlayer || !m_pCamera) return;
	CGameObject* pRifle = m_pPlayer->GetRifle();
	if (!pRifle) return;
	if (!m_pScene) return;
	const SceneState st = m_pScene->GetCurrentState();
	if (st != SceneState::MAP1 && st != SceneState::MAP2) return;

	const float yaw   = m_pCamera->GetYaw();
	const float pitch = m_pCamera->GetPitch();
	// 소총은 모델 좌측 +Z 축이 총구 방향이 된다.
	const float cp = cosf(pitch);
	const XMFLOAT3 aim{ sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp };

	// 반동 애니메이션 적용. CPlayer 가 보관한 타이머는 발사시 0, 발사 후 3 키프레임
	// 누적값으로 소총 메시의 forward 방향 오프셋(앞뒤 흔들림 양 보정) 사용.
	// 키프레임:
	//   (0.00 s, 0.00)   → 시작
	//   (0.06 s, -0.30)  → 최대 후진
	//   (0.14 s, -0.10)  → 절반 복귀
	//   (0.25 s, 0.00)   → 원위치
	auto computeRecoilOffset = [this]() -> float {
		const float fRecoil = m_pPlayer->GetRecoilTimer();
		if (fRecoil < 0.0f) return 0.0f;
		const float t = fRecoil;
		const float k0t = 0.00f, k0v = 0.00f;
		const float k1t = 0.06f, k1v = -0.30f;
		const float k2t = 0.14f, k2v = -0.10f;
		const float k3t = 0.25f, k3v = 0.00f;
		if (t <= k1t) {
			const float a = (t - k0t) / (k1t - k0t);
			return k0v + (k1v - k0v) * a;
		} else if (t <= k2t) {
			const float a = (t - k1t) / (k2t - k1t);
			return k1v + (k2v - k1v) * a;
		} else if (t <= k3t) {
			const float a = (t - k2t) / (k3t - k2t);
			return k2v + (k3v - k2v) * a;
		}
		return 0.0f;
	};
	const float fRecoilOffset = computeRecoilOffset();

	XMFLOAT3 pos;

	if (m_pCamera->GetMode() == ECameraMode::FPS) {
		// FPS: 화면 우측 하단 위치(어깨총처럼 화면 한쪽에 고정시킨다).
		// [Claude] 근평면을 0.1 로 줄여 1.7 forward 와 거리가 충분 (1.1 >> 0.1).
		const XMFLOAT3 camPos   = m_pCamera->GetPosition();
		const XMFLOAT3 camRight = m_pCamera->GetRight();
		const float kForward = 0.5f;
		const float kSide    = 0.55f;
		const float kDown    = 0.45f;
		// [Claude] 가능 한 경우에는 aim 방향(=forward) 으로 소총 회전 → 플레이어가 보고 추론한다.
		pos.x = camPos.x + camRight.x * kSide + aim.x * (kForward + fRecoilOffset);
		pos.y = camPos.y - kDown              + aim.y * (kForward + fRecoilOffset);
		pos.z = camPos.z + camRight.z * kSide + aim.z * (kForward + fRecoilOffset);
	}
	else {
		// TPS: 플레이어 모델 우측에서 어깨 옆. 모델 중심 좌표를 modelCenter 로 잡음.
		const XMFLOAT3 playerYawRight{ cosf(yaw), 0.0f, -sinf(yaw) };
		const XMFLOAT3 playerPos = m_pPlayer->GetPosition();
		const float modelCenterY = playerPos.y - MAP_EYE_HEIGHT + 1.3f;
		// [Claude] TPS 카메라 모드 회전과 일치하는 yaw 회전(=모델 회전) 으로 정렬.
		const float kBaseForward = 0.3f;
		pos.x = playerPos.x + playerYawRight.x * 1.0f + sinf(yaw) * (kBaseForward + fRecoilOffset);
		pos.y = modelCenterY;
		pos.z = playerPos.z + playerYawRight.z * 1.0f + cosf(yaw) * (kBaseForward + fRecoilOffset);
	}

	// 조준 대상점(없을 경우엔 폴백 직선상의 한 충돌점) 을 LookAt 으로 forward 정렬.
	// FPS/TPS 모두에서 동일한 한 줄로 처리해 코드 분기를 두지않으면 양쪽 모델이 분리된다.
	const XMFLOAT3 target = GetAimTargetPoint();
	XMFLOAT3 fwd{ target.x - pos.x, target.y - pos.y, target.z - pos.z };
	const float fl = sqrtf(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
	if (fl > 1e-5f) { fwd.x /= fl; fwd.y /= fl; fwd.z /= fl; }
	else            { fwd = aim; } // 폴백: 카메라 look 그대로

	pRifle->SetWorldOrientation(fwd, pos);
}

void CGameFramework::FireBullet()
{
	if (!m_pScene || !m_pCamera || !m_pBulletMesh) return;
	const SceneState state = m_pScene->GetCurrentState();
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return;
	if (m_fFireCooldown > 0.0f) return;

	// 카메라 방향 직접(외부 인자가 없음): 카메라 yaw/pitch 로부터 계산.
	const float yaw   = m_pCamera->GetYaw();
	const float pitch = m_pCamera->GetPitch();
	const float cp = cosf(pitch);
	const XMFLOAT3 camAim{ sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp };

	// 적의 자세/방향: 적이 보유. 카메라 transform 도 UpdateRifleTransform 도 이 시점에
	// 조준점(GetAimTargetPoint) 로 LookAt 으로 정렬하지만, 카메라의 forward 는 늘
	// '플레이어가 바라보는 방향' 이다. 단지 적의 시각적 보정 정보만 들고 있다.
	XMFLOAT3 origin;
	XMFLOAT3 aim;
	CGameObject* pRifle = m_pPlayer ? m_pPlayer->GetRifle() : nullptr;
	if (pRifle) {
		const XMFLOAT3 rpos = pRifle->GetPosition();
		const XMFLOAT3 rfwd = pRifle->GetLook(); // SetWorldOrientation 로부터 _31..33 = forward
		const float kMuzzle = 1.25f;
		origin = XMFLOAT3{
			rpos.x + rfwd.x * kMuzzle,
			rpos.y + rfwd.y * kMuzzle,
			rpos.z + rfwd.z * kMuzzle };
		aim = rfwd;
	}
	else {
		const XMFLOAT3 playerPos = m_pPlayer ? m_pPlayer->GetPosition() : XMFLOAT3{ 0, 0, 0 };
		origin = XMFLOAT3{
			playerPos.x + camAim.x * 0.8f,
			playerPos.y + 0.2f,
			playerPos.z + camAim.z * 0.8f };
		aim = camAim;
	}

	const float kBulletSpeed = 50.0f;
	auto pBullet = std::make_shared<CBulletObject>(origin, aim, kBulletSpeed, EObjectTag::Bullet);
	pBullet->SetMesh(m_pBulletMesh);
	pBullet->SetSceneState(state);
	m_pScene->AddObjectToCurrentMap(pBullet);

	m_fFireCooldown = 0.18f;

	// 적 총알 발사 콜백. UpdateRifleTransform 의 CPlayer 와 별개 형태로 사용되는
	// 적의 자체적 forward-기반 발사기(루트)로 동작한다. 적의 자세/방향과 직결 관계.
	if (m_pPlayer) m_pPlayer->SetRecoilTimer(0.0f);
}

void CGameFramework::SpawnEnemyBullet(const XMFLOAT3& xmf3Origin, const XMFLOAT3& xmf3Dir)
{
	if (!m_pScene || !m_pEnemyBulletMesh) return;
	const SceneState state = m_pScene->GetCurrentState();
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return;

	// 적 스폰은 플레이어와 너무 가깝지 않은 무작위 셀 위치에서 시작한다.
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

	// 적 AABB half y = 1.3 (전체 높이 2.6). 플레이어 발의 5타일 이상에서 안전 거리 보장.
	const float kEnemyHalfY = 1.3f;
	const XMFLOAT3 playerPos = m_pPlayer ? m_pPlayer->GetPosition() : XMFLOAT3{ 0, 0, 0 };
	std::vector<XMFLOAT3> spawns = PickEnemySpawnPositions(state, playerPos, 12, kEnemyHalfY);

	std::mt19937 seedGen{ std::random_device{}() };
	for (const XMFLOAT3& pos : spawns) {
		// 인스턴스마다 별도 RNG 분기 시드.
		const unsigned int nSeed = seedGen();
		auto pEnemy = std::make_shared<CEnemyObject>(state, nSeed);
		pEnemy->SetMesh(m_pEnemyMesh);
		pEnemy->SetPosition(pos);
		// 적이 발사할 때마다 이 람다가 호출 → GameFramework 의 SpawnEnemyBullet 을 호출.
		pEnemy->SetFireCallback([this](const XMFLOAT3& xmf3Origin, const XMFLOAT3& xmf3Dir) {
			SpawnEnemyBullet(xmf3Origin, xmf3Dir);
		});
		// 시야 판정 / 추적 로직에 사용할 현재 플레이어의 위치 게터를 등록한다.
		pEnemy->SetPlayerPosGetter([this]() {
			return m_pPlayer ? m_pPlayer->GetPosition() : XMFLOAT3{ 0, 0, 0 };
		});
		// 적의 적 마커 메시 주입. 가시성은 AnimateObjects 가 잔여 적 수로 결정 처리한다.
		if (m_pEnemyMarkerMesh) {
			pEnemy->SetMarkerMesh(m_pEnemyMarkerMesh);
		}
		// 적 소총 메시 주입. CEnemyObject 의 Animate 에서 본체와 함께 위치 동기화한다.
		if (m_pEnemyRifleMesh) {
			pEnemy->SetRifleMesh(m_pEnemyRifleMesh);
		}
		m_pScene->AddObjectToCurrentMap(pEnemy);
	}
}

void CGameFramework::AnimateObjects()
{
	if (!m_pScene) return;
	m_pScene->AnimateObjects(m_GameTimer.GetTimeElapsed());

	// 게임플레이 업데이트: 적 잔여 수에 따른 마커 토글 + 승리 조건 검사.
	{
		const SceneState curState = m_pScene->GetCurrentState();
		if (curState == SceneState::MAP1 || curState == SceneState::MAP2) {
			const int nAlive = m_pScene->CountAliveEnemies();

			// [Claude] 마커: 잔여 1~5 마리일 때 표시. 0 또는 6+ 일 때는 숨김 (디자인 3~5 만 표시).
			m_pScene->SetEnemyMarkersVisible(nAlive > 0 && nAlive <= 5);

			// 승리 조건: 적 0 마리이면서 아직 승리 시퀀스 중이 아닐 때 시작.
			if (nAlive == 0 && m_fVictoryTimer < 0.0f) {
				m_fVictoryTimer = 2.0f;
			}
			// 승리하면이면 타이머가 감소 후 만료시 시작. 0 되면 곧 LANDING 으로 복귀.
			if (m_fVictoryTimer >= 0.0f) {
				m_fVictoryTimer -= m_GameTimer.GetTimeElapsed();
				if (m_fVictoryTimer <= 0.0f) {
					m_fVictoryTimer = -1.0f;
					m_bResetPending = true;
				}
			}
		}
	}

	// 시작 화면의 GAME START 버튼을 눌렸으면 즉 시작 화면(MAP_SELECT) 으로 진입.
	if (m_pScene->IsGameStartRequested() && m_pScene->GetCurrentState() == SceneState::LANDING) {
		m_pScene->TransitionToScene(SceneState::MAP_SELECT);
		m_pScene->ClearGameStartRequest();
		SetupMapSelectCamera();
	}

	// 맵 선택 화면에 선택된 미니어처 클릭 결과를 비동기로 처리.
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

	// 라이프가 0 으로 떨어 또는 죽음타이머가 종료 직후 시 LANDING 화면으로 복귀.
	// 라이프가 다시 GAME START 를 눌렀을때 MAP_SELECT 와 MAP1/2 로직을 위 흐름과 합쳐진다.
	// [Claude] 플레이어 사망 시퀀스 진행. 활성화되면 매 프레임 카메라를 앞으로
	// 기울이고 hit-flash 를 1.0 으로 유지한다. 타이머 만료 시 기존 m_bResetPending
	// 경로로 위임하여 LANDING 으로 복귀.
	if (m_fPlayerDeathTimer >= 0.0f) {
		const float dt = m_GameTimer.GetTimeElapsed();
		m_fPlayerDeathTimer -= dt;
		if (m_pCamera) m_pCamera->Rotate(kPlayerDeathPitchRate * dt, 0.0f);
		m_fHitFlash = 1.0f;
		if (m_fPlayerDeathTimer <= 0.0f) {
			m_fPlayerDeathTimer = -1.0f;
			m_bResetPending     = true;
		}
	}

	if (m_bResetPending) {
		m_bResetPending = false;
		m_pScene->ResetGameplayState();
		if (m_pPlayer) m_pPlayer->SetHP(10);
		// 일관성을위해 승리 타이머도 정리 (사망 처리 이후 -1, 기존 reset 처리 진행).
		m_fVictoryTimer = -1.0f;
		// [Claude] 사망 시퀀스가 다른 경로로 reset 을 트리거했더라도 방어적으로 -1 복귀.
		m_fPlayerDeathTimer = -1.0f;
		m_pScene->TransitionToScene(SceneState::LANDING);
		// 마우스 캡쳐 / 사망 카운터 / 적의 카운터 정리 후 씬 전환까지 일괄로 정리.
		if (m_bMouseCaptured) {
			::ShowCursor(TRUE);
			m_bMouseCaptured = false;
		}
		m_fFireCooldown = 0.0f;
		if (m_pPlayer) {
			m_pPlayer->SetVerticalVelocity(0.0f);
			m_pPlayer->SetGrounded(true);
		}
		// 게임 씬과 카메라 초기화로 진입한다.
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
	// 각 씬에 1인칭 카메라 위치/방향을 그 맵의 MapInfo 에서 초기화한다.
	MapInfo info;
	switch (state) {
	case SceneState::MAP1: info = GetMap1Info(); break;
	case SceneState::MAP2: info = GetMap2Info(); break;
	default: return; // LANDING 등 기본은 따로 처리하지 않음.
	}
	m_pCamera->GenerateViewMatrix(info.cameraPosition, info.cameraLookAt, XMFLOAT3(0.0f, 1.0f, 0.0f));
	// 입력 시 플레이어가 시점을 갖는 사람(눈은 이마에 있으므로) 의 위치으로 정렬.
	if (m_pPlayer) m_pPlayer->SetPosition(info.cameraPosition);
	// 시점이 카메라 모드 별 안 다른 FPS 로 시작.
	m_pCamera->SetMode(ECameraMode::FPS);
	if (m_pScene) m_pScene->SetPlayerVisible(false);
	// 직후 ProcessInput 에서 마우스 캡쳐와 모드 토글이 이어지도록 한다.
	m_bMouseCaptured = false;
	// 적의 시야 정리.
	if (m_pPlayer) {
		m_pPlayer->SetVerticalVelocity(0.0f);
		m_pPlayer->SetGrounded(true);
	}
	// Bullets fired on the previous map should not carry over; the cooldown
	// is per-life, so reset it too.
	m_fFireCooldown = 0.0f;

	// 게임플레이 / 사망 / 승리 처리 정리 직전. 입력 정리는 모두 시점이 어두워야지
	// 적 입력 토글이 깨끗 정리 시점에 동작한다.
	if (m_pPlayer) m_pPlayer->SetHP(10);
	m_bResetPending = false;
	// 승리 시각 종료 직후 한 번 다시 시작 시 정리되도록/리셋도 모두 함께 동작.
	m_fVictoryTimer = -1.0f;
	if (m_pScene) m_pScene->ResetGameplayState();
	SpawnEnemiesForMap(state);

	// 플레이어 위 등에는 적의 적자가 모두 죽어있는걸데, 이 시점에 충돌 처리에서
	// 죄가 진행이 노출되지 않을 한다 (Scene::AnimateObjects 의 EnemyBullet vs Player).
	if (m_pPlayerModel && m_pPlayer) {
		const XMFLOAT3 playerPos = m_pPlayer->GetPosition();
		const XMFLOAT3 modelCenter{
			playerPos.x,
			playerPos.y - MAP_EYE_HEIGHT + 1.3f,
			playerPos.z };
		m_pPlayerModel->SetWorldYawAndPosition(m_pCamera->GetYaw(), modelCenter);
	}
}

void CGameFramework::SetupMapSelectCamera()
{
	if (!m_pCamera) return;
	// 맵 선택 화면 카메라 셋업: 정면 약 멀리서 미니어처들이 보인다.
	m_pCamera->GenerateViewMatrix(
		XMFLOAT3(0.0f, 5.0f, -55.0f),
		XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f));
	// 마우스 캡쳐를 풀어 클릭으로 진입하기 (사진 같음 좋다).
	if (m_bMouseCaptured) {
		::ShowCursor(TRUE);
		m_bMouseCaptured = false;
	}
	// 적의 시야 정리.
	if (m_pPlayer) {
		m_pPlayer->SetVerticalVelocity(0.0f);
		m_pPlayer->SetGrounded(true);
	}
}

void CGameFramework::WaitForGPUComplete()
{
	// 다음 프레임 시작 Fence Value 를 1 증가.
	UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];

	// cmdQueue 에 시그널을 보내, GPU 가 처리하는 모든 명령이 종료 후에 nFenceValue 로 셋팅됨.
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);

	// 메인 스레드가 nFenceValue 가 도달하기를 기다리며 대기한다.
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
	// 프레임 단위 갱신: 명령/할당자/스왑체인 갱신.
	m_GameTimer.Tick(0.0f);

	// 피격 비네트 감쇠: elapsed 시간에 비례해 m_fHitFlash 를 0 으로 점차 감쇠.
	if (m_fHitFlash > 0.0f) {
		m_fHitFlash -= kHitFlashDecayRate * m_GameTimer.GetTimeElapsed();
		if (m_fHitFlash < 0.0f) m_fHitFlash = 0.0f;
	}

	ProcessInput();

	AnimateObjects();

	// 메인의 할당자/리스트 리셋.
	HRESULT hResult = m_pd3dCommandAllocator->Reset();
	hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL);

	// 현재 백버퍼를 PRESENT 상태에서 RENDER_TARGET 상태로 전환.
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

	// 출력시할 RTV CPU 핸들로 한정시킨다.
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (m_nSwapChainBufferIndex * m_nRtvDescriptorIncrementSize);

	// 깊이-스텐실 디스크립터의 CPU 핸들로 한정시킨다.
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =
		m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	m_pd3dCommandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, FALSE,
		&d3dDsvCPUDescriptorHandle);

	float pfClearColor[4] = { 0.55f, 0.78f, 0.92f, 1.0f };
	m_pd3dCommandList->ClearRenderTargetView(
		d3dRtvCPUDescriptorHandle,
		pfClearColor,
		0,
		NULL
	);

	// 뷰포트/시저렉트 설정.
	m_pd3dCommandList->ClearDepthStencilView(
		d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		NULL
	);

	// 씬 렌더링.
	if (m_pScene) m_pScene->Render(m_pd3dCommandList.Get(), m_pCamera.get());

	// 주의: 게임플레이 화면의 FPS/TPS 시점 모델 그리기. Scene::Render 안에서는
	// CObjectsShader 의 PSO 가 매번 바인딩되는 모드 변경 PSO 라는 자기 자체 적용을
	// 어쩌면시키는 방식 인 수도. 단지 GameObject 의 shader 오버라이드만이 m_xmf4x4World 를
	// 바인딩하는 방식이다.
	if (m_pPlayer && m_pPlayer->GetRifle() && m_pScene && m_pCamera) {
		const SceneState st = m_pScene->GetCurrentState();
		if (st == SceneState::MAP1 || st == SceneState::MAP2) {
			m_pPlayer->GetRifle()->Render(m_pd3dCommandList.Get(), m_pCamera.get());
		}
	}

	// 십자선은 게임 플레이 씬(MAP1/MAP2)에서만 그린다.
	// CGameObject::Render 가 CHudShader 의 PSO 로 바인딩 한 뒤 메시를 그리므로
	// 자기 셰이더 없이 같은 메시 같은 셰이더 슬롯 하나 묶어 한 번에 처리한다.
	if (m_pCrosshair && m_pScene && m_pCamera) {
		const SceneState st = m_pScene->GetCurrentState();
		if (st == SceneState::MAP1 || st == SceneState::MAP2) {
			m_pCrosshair->Render(m_pd3dCommandList.Get(), m_pCamera.get());

			// 라이프 바 칸: 살아 있는 갯수. m_pPlayer->GetHP() 개수만큼 앞에서부터 렌더한다.
			// 라이프 바와 적의 카운트 점도 모두 같은 방식. CHudShader 의 깊이없는 PSO 라 항상 통과.
			const int nLife = m_pPlayer ? m_pPlayer->GetHP() : 0;
			const int nDraw = (nLife < 0) ? 0 : (nLife > 10 ? 10 : nLife);
			for (int i = 0; i < nDraw && i < static_cast<int>(m_pLifeBarSegments.size()); ++i) {
				if (m_pLifeBarSegments[i]) {
					m_pLifeBarSegments[i]->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}

			// 적 잔여 수 점 카운트(좌상단). 살아있는 적 마릿수만큼 앞에서부터 그린다.
			const int nAlive = m_pScene->CountAliveEnemies();
			const int nDrawPips = (nAlive < 0) ? 0
				: (nAlive > static_cast<int>(m_pCountPips.size()) ? static_cast<int>(m_pCountPips.size()) : nAlive);
			for (int i = 0; i < nDrawPips; ++i) {
				if (m_pCountPips[i]) {
					m_pCountPips[i]->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}

			// 승리 화면용 글자 세그먼트 표시는 모든 적이 죽었을 때 'WIN' 표시.
			if (m_fVictoryTimer >= 0.0f) {
				for (auto& pLetter : m_pWinLetters) {
					if (pLetter) pLetter->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}

			// 피격 비네트 가 강도 큰값보다 화면이 빨간 UI 외곽 비네트가 그리진다.
			// CHudShader 의 별도 PSO(alpha blend ON) 로 풀스크린 쿼드한번을 사용해서 바인딩하고
			// b3 상수 슬롯에 현재 flash 강도를 보내서 그 강도에 따라 1번 드로우한다.
			if (m_fHitFlash > 0.0f && m_pHitOverlayShader && m_pHitOverlayQuad) {
				m_pHitOverlayShader->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				m_pd3dCommandList->SetGraphicsRoot32BitConstants(3, 1, &m_fHitFlash, 0);
				m_pHitOverlayQuad->Render(m_pd3dCommandList.Get(), m_pCamera.get());
			}
		}
	}

	// PRESENT 상태로 다시 전환.
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	// 명령 리스트 제출.
	hResult = m_pd3dCommandList->Close();

	// 화면 출력 전환.
	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(
		_countof(ppd3dCommandLists),
		ppd3dCommandLists
	);

	// GPU 동기화 대기.
	WaitForGPUComplete();

	// 현재까지 측정된 평균 FPS 값을 출력.
	m_pdxgiSwapChain->Present(0, 0);
	MoveToNextFrame();
	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}

CGameFramework::~CGameFramework()
{

}
