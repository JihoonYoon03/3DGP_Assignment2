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
	WaitForGPUComplete();
	ReleaseObjects();
	::CloseHandle(m_hFenceEvent);
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

	// 12.0 을 지원하는 첫 어댑터 탐색
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != m_pdxgiFactory->EnumAdapters1(i, &pd3dAdapter); ++i)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
		pd3dAdapter->GetDesc1(&dxgiAdapterDesc);

		if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		if (SUCCEEDED(D3D12CreateDevice(
			pd3dAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_pd3dDevice)
		)))
			break;
	}

	// 12.0 이 안 되면 WARP 어댑터 사용
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
	d3dMsaaQualityLevels.SampleCount = 4;
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;
	m_pd3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&d3dMsaaQualityLevels,
		sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS)
	);

	m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;

	// 펜스/이벤트 생성
	hResult = m_pd3dDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pd3dFence)
	);
	m_nFenceValue = 0;

	m_hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

void CGameFramework::CreateCommandQueueAndList()
{
	// 명령 큐 생성
	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hResult = m_pd3dDevice->CreateCommandQueue(
		&d3dCommandQueueDesc,
		IID_PPV_ARGS(&m_pd3dCommandQueue)
	);

	// 명령 할당자 생성
	hResult = m_pd3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&m_pd3dCommandAllocator)
	);

	// 명령 리스트 생성
	hResult = m_pd3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pd3dCommandAllocator.Get(),
		NULL,
		IID_PPV_ARGS(&m_pd3dCommandList)
	);

	// 명령 리스트는 처음 Open 상태이므로 Close 해 둔다
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

	HRESULT hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dRtvDescriptorHeap)
	);

	m_nRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// DSV 디스크립터 힙 (1개)
	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dDsvDescriptorHeap)
	);

	m_nDsvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

// 백버퍼별로 RTV 를 생성한다
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
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
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

	// 카메라 생성
	m_pCamera = std::make_unique<CCamera>();
	m_pCamera->SetViewport(0, 0, m_nWndClientWidth, m_nWndClientHeight, 0.0f, 1.0f);
	m_pCamera->SetScissorRect(0, 0, m_nWndClientWidth, m_nWndClientHeight);
	m_pCamera->GenerateProjectionMatrix(0.1f, FRUSTUM_FAR_PLANE,
		float(m_nWndClientWidth) / float(m_nWndClientHeight), 90.0f);
	m_pCamera->GenerateViewMatrix(XMFLOAT3(0.0f, 0.0f, -50.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f));

	// 씬 객체 빌드
	m_pScene = std::make_unique<CScene>();
	m_pScene->BuildObjects(m_pd3dDevice.Get(), m_pd3dCommandList.Get());

	// TPS 모드에서 보이는 플레이어 모델 (빨간 큐브)
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

	// 플레이어 총알 메시 (노란 작은 큐브)
	m_pBulletMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.4f, 0.4f, 0.4f, true, XMFLOAT4(1.0f, 0.9f, 0.2f, 1.0f));

	// 적 총알 메시 (빨간 작은 큐브)
	m_pEnemyBulletMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.4f, 0.4f, 0.4f, true, XMFLOAT4(1.0f, 0.25f, 0.20f, 1.0f));

	// 적 큐브 메시 (보라색)
	m_pEnemyMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		1.2f, 2.6f, 1.2f, true, XMFLOAT4(0.45f, 0.15f, 0.55f, 1.0f));

	// 플레이어용 M16 소총 메시. 모델 좌표계를 엔진 좌표계에 맞추는 베이크 변환을 적용한다.
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

	// 플레이어 객체 생성과 소총 부착
	m_pPlayer = std::make_shared<CPlayer>();
	{
		auto pRifleObj = std::make_shared<CGameObject>();
		pRifleObj->SetMesh(m_pRifleMesh);
		m_pPlayer->SetRifleObject(std::move(pRifleObj));
	}
	m_pPlayer->SetPosition(XMFLOAT3{ 0.0f, MAP_EYE_HEIGHT, 0.0f });

	// 적용 AK47 소총 메시
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

	// 적 머리 위 노란 마커 기둥 메시
	m_pEnemyMarkerMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.15f, 14.0f, 0.15f, true, XMFLOAT4(1.0f, 0.85f, 0.1f, 1.0f));

	// HUD 셰이더 (조준점, 라이프바 공용)
	m_pHudShader = std::make_shared<CHudShader>();
	if (m_pScene) {
		m_pHudShader->CreateShader(m_pd3dDevice.Get(), m_pScene->GetGraphicsRootSignature());
	}

	// 피격 비네트 셰이더와 풀스크린 쿼드
	m_pHitOverlayShader = std::make_shared<CHitOverlayShader>();
	if (m_pScene) {
		m_pHitOverlayShader->CreateShader(m_pd3dDevice.Get(), m_pScene->GetGraphicsRootSignature());
	}
	{
		auto pOverlayMesh = std::make_shared<CHudQuadMesh>(
			m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
			-1.0f, 1.0f, 1.0f, -1.0f,
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		m_pHitOverlayQuad = std::make_shared<CGameObject>();
		m_pHitOverlayQuad->SetMesh(pOverlayMesh);
	}

	// 화면 정중앙 십자선
	{
		auto pCrosshairMesh = std::make_shared<CCrosshairMesh>(
			m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
			UINT(m_nWndClientWidth), UINT(m_nWndClientHeight),
			10u, 2u, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		m_pCrosshair = std::make_shared<CGameObject>();
		m_pCrosshair->SetMesh(pCrosshairMesh);
		m_pCrosshair->SetShader(m_pHudShader);
	}

	// 라이프 바 10칸 생성. HP 값에 맞춰 매 프레임 앞에서부터 일부만 그린다.
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

	// 좌상단 적 잔여 수 점 12개 생성
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

	// 승리 시 표시되는 'WIN' 글자를 NDC 사각 막대 조합으로 만든다
	{
		const float wPx = float(m_nWndClientWidth  > 0 ? m_nWndClientWidth  : 1);
		const float hPx = float(m_nWndClientHeight > 0 ? m_nWndClientHeight : 1);
		const float kLetterW = 80.0f;
		const float kLetterH = 120.0f;
		const float kStroke  = 14.0f;
		const float kLetterGap = 30.0f;
		const XMFLOAT4 col(1.0f, 0.85f, 0.20f, 1.0f);

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

		// W 글자
		{
			const float L = startXPx;
			const float R = L + kLetterW;
			const float T = topYPx;
			const float B = topYPx + kLetterH;
			addQuad(L, L + kStroke, T, B);
			addQuad(R - kStroke, R, T, B);
			const float midX = (L + R) * 0.5f;
			addQuad(midX - kStroke * 0.5f, midX + kStroke * 0.5f, B - kLetterH * 0.55f, B);
			addQuad(L, R, B - kStroke, B);
		}
		// I 글자
		{
			const float L = startXPx + kLetterW + kLetterGap;
			const float R = L + kLetterW;
			const float T = topYPx;
			const float B = topYPx + kLetterH;
			const float midX = (L + R) * 0.5f;
			addQuad(midX - kStroke * 0.5f, midX + kStroke * 0.5f, T, B);
			addQuad(L + kStroke, R - kStroke, T, T + kStroke);
			addQuad(L + kStroke, R - kStroke, B - kStroke, B);
		}
		// N 글자
		{
			const float L = startXPx + (kLetterW + kLetterGap) * 2.0f;
			const float R = L + kLetterW;
			const float T = topYPx;
			const float B = topYPx + kLetterH;
			addQuad(L, L + kStroke, T, B);
			addQuad(R - kStroke, R, T, B);
			addQuad(L, R, T, T + kStroke);
		}
	}

	// 플레이어 피격 콜백
	if (m_pScene) {
		m_pScene->SetOnPlayerHit([this]() {
			if (!m_pPlayer) return;
			if (m_fPlayerDeathTimer >= 0.0f) return;
			if (m_pPlayer->GetHP() > 0) m_pPlayer->TakeDamage(1);
			if (m_pPlayer->GetHP() <= 0) m_fPlayerDeathTimer = kPlayerDeathDuration;
			m_fHitFlash = 1.0f;
		});
	}

	// 명령 리스트 닫고 큐에 제출
	m_pd3dCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	WaitForGPUComplete();

	// 업로드용 임시 버퍼 해제
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
			// 게임플레이 씬에서는 좌클릭 = 발사. UI 씬은 씬의 hit-test 경로로.
			const SceneState st = m_pScene->GetCurrentState();
			if (st == SceneState::MAP1 || st == SceneState::MAP2) {
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
			// F9: 풀스크린 토글
			ChangeSwapChainState();
			break;
		case 'V':
			// V: FPS / TPS 시점 토글
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

	// 사망 시퀀스 중이면 입력 차단
	if (m_fPlayerDeathTimer >= 0.0f) {
		if (m_fFireCooldown > 0.0f) m_fFireCooldown -= m_GameTimer.GetTimeElapsed();
		return;
	}

	const SceneState state = m_pScene->GetCurrentState();
	const bool bGameplay = (state == SceneState::MAP1 || state == SceneState::MAP2);
	if (!bGameplay) {
		// LANDING/MAP_SELECT 에서는 마우스만 풀어둔다
		if (m_bMouseCaptured) {
			::ShowCursor(TRUE);
			m_bMouseCaptured = false;
		}
		return;
	}

	// 발사 쿨다운
	if (m_fFireCooldown > 0.0f) m_fFireCooldown -= m_GameTimer.GetTimeElapsed();

	// 반동 애니메이션 진행
	if (m_pPlayer) m_pPlayer->TickRecoil(m_GameTimer.GetTimeElapsed(), kRecoilDuration);

	const bool bForeground = (::GetForegroundWindow() == m_hWnd);

	// 마우스 룩 (화면 중앙 캡쳐 + delta 측정)
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
				const float fPitch = XMConvertToRadians(static_cast<float>(-dy) * kSensitivityDeg);
				m_pCamera->Rotate(fPitch, fYaw);
				::SetCursorPos(m_ptWndCenterScreen.x, m_ptWndCenterScreen.y);
			}
			m_ptWndCenterScreen = ptCenter;
		}
	}

	// WASD + 카메라 룩 (XZ 평면)
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

	// 점프 + 중력
	const float kGravity = 30.0f;
	const float kJumpApex = 3.0f * 0.7f;
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
		// 공중: 중력 적용
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
		// 지상: 바닥 높이 추적
		next.y = groundY;
	}

	m_pPlayer->SetPosition(next);

	// 카메라 모드별 위치 갱신
	const XMFLOAT3 playerPos = m_pPlayer->GetPosition();
	if (m_pCamera->GetMode() == ECameraMode::FPS) {
		// FPS: 카메라 위치 = 플레이어 눈
		m_pCamera->SetPosition(playerPos);
		if (m_pPlayerModel) {
			XMFLOAT3 modelCenter{
				playerPos.x,
				playerPos.y - MAP_EYE_HEIGHT + 1.3f,
				playerPos.z };
			m_pPlayerModel->SetWorldYawAndPosition(m_pCamera->GetYaw(), modelCenter);
		}
	}
	else {
		// TPS: 플레이어 뒤 어깨너머 시점
		const float kBack = 3.0f;
		const float kUp   = 1.2f;

		const float yaw   = m_pCamera->GetYaw();
		const float pitch = m_pCamera->GetPitch();

		const XMFLOAT3 backDir{ -sinf(yaw), 0.0f, -cosf(yaw) };

		const float horizBack = kBack * cosf(pitch);
		const float vertBack  = -kBack * sinf(pitch);
		const float eyeY      = playerPos.y + vertBack + kUp;

		// 벽이 있으면 카메라 거리를 줄여 벽 통과를 방지
		const float dist = ClampDistanceAgainstWalls(
			state, playerPos, backDir, horizBack, eyeY);

		XMFLOAT3 tpsEye{
			playerPos.x + backDir.x * dist,
			eyeY,
			playerPos.z + backDir.z * dist };

		m_pCamera->SetPosition(tpsEye);

		if (m_pPlayerModel) {
			XMFLOAT3 modelCenter{
				playerPos.x,
				playerPos.y - MAP_EYE_HEIGHT + 1.3f,
				playerPos.z };
			m_pPlayerModel->SetWorldYawAndPosition(m_pCamera->GetYaw(), modelCenter);
		}
	}

	UpdateRifleTransform();
}

XMFLOAT3 CGameFramework::GetAimTargetPoint() const
{
	// 카메라 정면으로 광선을 쏴 벽이나 적과의 첫 충돌점을 찾는다
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

	// 벽까지의 거리 (수평 마칭으로 측정 후 3D 길이로 환산)
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

	// 적 AABB 까지의 거리 (슬랩 라인테스트)
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
	const float cp = cosf(pitch);
	const XMFLOAT3 aim{ sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp };

	// 발사 후 반동 키프레임 보간 (0.06s 후진, 0.14s 절반 복귀, 0.25s 원위치)
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
		// FPS: 화면 우측 하단에 어깨총처럼 고정
		const XMFLOAT3 camPos   = m_pCamera->GetPosition();
		const XMFLOAT3 camRight = m_pCamera->GetRight();
		const float kForward = 0.5f;
		const float kSide    = 0.55f;
		const float kDown    = 0.45f;
		pos.x = camPos.x + camRight.x * kSide + aim.x * (kForward + fRecoilOffset);
		pos.y = camPos.y - kDown              + aim.y * (kForward + fRecoilOffset);
		pos.z = camPos.z + camRight.z * kSide + aim.z * (kForward + fRecoilOffset);
	}
	else {
		// TPS: 플레이어 모델 우측 어깨 옆
		const XMFLOAT3 playerYawRight{ cosf(yaw), 0.0f, -sinf(yaw) };
		const XMFLOAT3 playerPos = m_pPlayer->GetPosition();
		const float modelCenterY = playerPos.y - MAP_EYE_HEIGHT + 1.3f;
		const float kBaseForward = 0.3f;
		pos.x = playerPos.x + playerYawRight.x * 1.0f + sinf(yaw) * (kBaseForward + fRecoilOffset);
		pos.y = modelCenterY;
		pos.z = playerPos.z + playerYawRight.z * 1.0f + cosf(yaw) * (kBaseForward + fRecoilOffset);
	}

	// 조준점을 LookAt 으로 forward 정렬
	const XMFLOAT3 target = GetAimTargetPoint();
	XMFLOAT3 fwd{ target.x - pos.x, target.y - pos.y, target.z - pos.z };
	const float fl = sqrtf(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
	if (fl > 1e-5f) { fwd.x /= fl; fwd.y /= fl; fwd.z /= fl; }
	else            { fwd = aim; }

	pRifle->SetWorldOrientation(fwd, pos);
}

void CGameFramework::FireBullet()
{
	if (!m_pScene || !m_pCamera || !m_pBulletMesh) return;
	const SceneState state = m_pScene->GetCurrentState();
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return;
	if (m_fFireCooldown > 0.0f) return;

	// 카메라 방향
	const float yaw   = m_pCamera->GetYaw();
	const float pitch = m_pCamera->GetPitch();
	const float cp = cosf(pitch);
	const XMFLOAT3 camAim{ sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp };

	// 총구 위치/방향 결정 (소총이 있으면 소총 기준, 없으면 카메라 기준)
	XMFLOAT3 origin;
	XMFLOAT3 aim;
	CGameObject* pRifle = m_pPlayer ? m_pPlayer->GetRifle() : nullptr;
	if (pRifle) {
		const XMFLOAT3 rpos = pRifle->GetPosition();
		const XMFLOAT3 rfwd = pRifle->GetLook();
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

	// 반동 애니메이션 시작
	if (m_pPlayer) m_pPlayer->SetRecoilTimer(0.0f);
}

void CGameFramework::SpawnEnemyBullet(const XMFLOAT3& xmf3Origin, const XMFLOAT3& xmf3Dir)
{
	if (!m_pScene || !m_pEnemyBulletMesh) return;
	const SceneState state = m_pScene->GetCurrentState();
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return;

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

	// 플레이어로부터 안전 거리 안에 적을 스폰
	const float kEnemyHalfY = 1.3f;
	const XMFLOAT3 playerPos = m_pPlayer ? m_pPlayer->GetPosition() : XMFLOAT3{ 0, 0, 0 };
	std::vector<XMFLOAT3> spawns = PickEnemySpawnPositions(state, playerPos, 12, kEnemyHalfY);

	std::mt19937 seedGen{ std::random_device{}() };
	for (const XMFLOAT3& pos : spawns) {
		const unsigned int nSeed = seedGen();
		auto pEnemy = std::make_shared<CEnemyObject>(state, nSeed);
		pEnemy->SetMesh(m_pEnemyMesh);
		pEnemy->SetPosition(pos);
		// 적이 발사할 때마다 GameFramework 의 SpawnEnemyBullet 호출
		pEnemy->SetFireCallback([this](const XMFLOAT3& xmf3Origin, const XMFLOAT3& xmf3Dir) {
			SpawnEnemyBullet(xmf3Origin, xmf3Dir);
		});
		// 시야 판정과 추적용 플레이어 위치 게터
		pEnemy->SetPlayerPosGetter([this]() {
			return m_pPlayer ? m_pPlayer->GetPosition() : XMFLOAT3{ 0, 0, 0 };
		});
		if (m_pEnemyMarkerMesh) {
			pEnemy->SetMarkerMesh(m_pEnemyMarkerMesh);
		}
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

	// 적 마커 토글과 승리 조건 검사
	{
		const SceneState curState = m_pScene->GetCurrentState();
		if (curState == SceneState::MAP1 || curState == SceneState::MAP2) {
			const int nAlive = m_pScene->CountAliveEnemies();

			// 잔여 적 1~5 마리일 때 마커 표시
			m_pScene->SetEnemyMarkersVisible(nAlive > 0 && nAlive <= 5);

			// 적이 모두 죽으면 승리 카운트다운 시작
			if (nAlive == 0 && m_fVictoryTimer < 0.0f) {
				m_fVictoryTimer = 2.0f;
			}
			if (m_fVictoryTimer >= 0.0f) {
				m_fVictoryTimer -= m_GameTimer.GetTimeElapsed();
				if (m_fVictoryTimer <= 0.0f) {
					m_fVictoryTimer = -1.0f;
					m_bResetPending = true;
				}
			}
		}
	}

	// 시작 화면 -> 맵 선택
	if (m_pScene->IsGameStartRequested() && m_pScene->GetCurrentState() == SceneState::LANDING) {
		m_pScene->TransitionToScene(SceneState::MAP_SELECT);
		m_pScene->ClearGameStartRequest();
		SetupMapSelectCamera();
	}

	// 맵 선택 화면 -> 게임 씬
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

	// 사망 시퀀스: 카메라를 앞으로 기울이며 hit-flash 유지
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

	// LANDING 복귀 처리
	if (m_bResetPending) {
		m_bResetPending = false;
		m_pScene->ResetGameplayState();
		if (m_pPlayer) m_pPlayer->SetHP(10);
		m_fVictoryTimer = -1.0f;
		m_fPlayerDeathTimer = -1.0f;
		m_pScene->TransitionToScene(SceneState::LANDING);
		if (m_bMouseCaptured) {
			::ShowCursor(TRUE);
			m_bMouseCaptured = false;
		}
		m_fFireCooldown = 0.0f;
		if (m_pPlayer) {
			m_pPlayer->SetVerticalVelocity(0.0f);
			m_pPlayer->SetGrounded(true);
		}
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
	// 맵에 진입할 때 카메라/플레이어/적 등을 초기화한다
	MapInfo info;
	switch (state) {
	case SceneState::MAP1: info = GetMap1Info(); break;
	case SceneState::MAP2: info = GetMap2Info(); break;
	default: return;
	}
	m_pCamera->GenerateViewMatrix(info.cameraPosition, info.cameraLookAt, XMFLOAT3(0.0f, 1.0f, 0.0f));
	if (m_pPlayer) m_pPlayer->SetPosition(info.cameraPosition);
	m_pCamera->SetMode(ECameraMode::FPS);
	if (m_pScene) m_pScene->SetPlayerVisible(false);
	m_bMouseCaptured = false;
	if (m_pPlayer) {
		m_pPlayer->SetVerticalVelocity(0.0f);
		m_pPlayer->SetGrounded(true);
	}
	m_fFireCooldown = 0.0f;

	if (m_pPlayer) m_pPlayer->SetHP(10);
	m_bResetPending = false;
	m_fVictoryTimer = -1.0f;
	if (m_pScene) m_pScene->ResetGameplayState();
	SpawnEnemiesForMap(state);

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
	// 맵 선택 화면 카메라: 미니어처들이 보이도록 멀리서 잡는다
	m_pCamera->GenerateViewMatrix(
		XMFLOAT3(0.0f, 5.0f, -55.0f),
		XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f));
	if (m_bMouseCaptured) {
		::ShowCursor(TRUE);
		m_bMouseCaptured = false;
	}
	if (m_pPlayer) {
		m_pPlayer->SetVerticalVelocity(0.0f);
		m_pPlayer->SetGrounded(true);
	}
}

void CGameFramework::WaitForGPUComplete()
{
	UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];

	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);

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
	m_GameTimer.Tick(0.0f);

	// 피격 비네트 감쇠
	if (m_fHitFlash > 0.0f) {
		m_fHitFlash -= kHitFlashDecayRate * m_GameTimer.GetTimeElapsed();
		if (m_fHitFlash < 0.0f) m_fHitFlash = 0.0f;
	}

	ProcessInput();

	AnimateObjects();

	// 명령 할당자/리스트 리셋
	HRESULT hResult = m_pd3dCommandAllocator->Reset();
	hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL);

	// 백버퍼를 PRESENT -> RENDER_TARGET 으로 전환
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

	// RTV CPU 핸들
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (m_nSwapChainBufferIndex * m_nRtvDescriptorIncrementSize);

	// DSV CPU 핸들
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

	m_pd3dCommandList->ClearDepthStencilView(
		d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		NULL
	);

	// 씬 렌더링
	if (m_pScene) m_pScene->Render(m_pd3dCommandList.Get(), m_pCamera.get());

	// 플레이어 소총 렌더링 (게임플레이 씬에서만)
	if (m_pPlayer && m_pPlayer->GetRifle() && m_pScene && m_pCamera) {
		const SceneState st = m_pScene->GetCurrentState();
		if (st == SceneState::MAP1 || st == SceneState::MAP2) {
			m_pPlayer->GetRifle()->Render(m_pd3dCommandList.Get(), m_pCamera.get());
		}
	}

	// HUD: 십자선, 라이프 바, 적 카운트, WIN, 피격 비네트
	if (m_pCrosshair && m_pScene && m_pCamera) {
		const SceneState st = m_pScene->GetCurrentState();
		if (st == SceneState::MAP1 || st == SceneState::MAP2) {
			m_pCrosshair->Render(m_pd3dCommandList.Get(), m_pCamera.get());

			// 라이프 바 칸: HP 만큼 앞에서부터
			const int nLife = m_pPlayer ? m_pPlayer->GetHP() : 0;
			const int nDraw = (nLife < 0) ? 0 : (nLife > 10 ? 10 : nLife);
			for (int i = 0; i < nDraw && i < static_cast<int>(m_pLifeBarSegments.size()); ++i) {
				if (m_pLifeBarSegments[i]) {
					m_pLifeBarSegments[i]->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}

			// 적 카운트 점: 살아있는 적 수만큼 앞에서부터
			const int nAlive = m_pScene->CountAliveEnemies();
			const int nDrawPips = (nAlive < 0) ? 0
				: (nAlive > static_cast<int>(m_pCountPips.size()) ? static_cast<int>(m_pCountPips.size()) : nAlive);
			for (int i = 0; i < nDrawPips; ++i) {
				if (m_pCountPips[i]) {
					m_pCountPips[i]->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}

			// 승리 시 WIN 글자
			if (m_fVictoryTimer >= 0.0f) {
				for (auto& pLetter : m_pWinLetters) {
					if (pLetter) pLetter->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}

			// 피격 비네트
			if (m_fHitFlash > 0.0f && m_pHitOverlayShader && m_pHitOverlayQuad) {
				m_pHitOverlayShader->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				m_pd3dCommandList->SetGraphicsRoot32BitConstants(3, 1, &m_fHitFlash, 0);
				m_pHitOverlayQuad->Render(m_pd3dCommandList.Get(), m_pCamera.get());
			}
		}
	}

	// PRESENT 상태로 복귀
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	// 명령 리스트 제출
	hResult = m_pd3dCommandList->Close();

	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(
		_countof(ppd3dCommandLists),
		ppd3dCommandLists
	);

	WaitForGPUComplete();

	m_pdxgiSwapChain->Present(0, 0);
	MoveToNextFrame();
	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}

CGameFramework::~CGameFramework()
{

}
