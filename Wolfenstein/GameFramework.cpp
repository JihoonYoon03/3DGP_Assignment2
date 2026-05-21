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

// ???? ???��???? D3D12 ??????/??????? ???? ??? ??????? ??????.
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
	// GPU?? ??? ????? ????? ?????? ????? ?? ??? ????.
	WaitForGPUComplete();

	// ???? ???(?????? ????? ????) ????.
	ReleaseObjects();

	::CloseHandle(m_hFenceEvent);

	// swapchain ?????? ???? ???? ??? ?? ????.
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

	// ?????? ????? ????????? ??? ???? ??? ?��???.
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

	// ??????? ???????? ?????? D3D12 ??? ???? 12.0 ?? ??????? ? ?????? ??��?.
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != m_pdxgiFactory->EnumAdapters1(i, &pd3dAdapter); ++i)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
		pd3dAdapter->GetDesc1(&dxgiAdapterDesc);

		if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			continue;

		// ID3D12Device ???? ?��?.
		if (SUCCEEDED(D3D12CreateDevice(
			pd3dAdapter.Get(),
			D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(&m_pd3dDevice)
		)))
			break;
	}

	// 12.0 ?????? ?? ????? WARP(?????????) ?????? ????.
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
	d3dMsaaQualityLevels.SampleCount = 4;	// MSAA 4x ??????��?
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;
	m_pd3dDevice->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&d3dMsaaQualityLevels,
		sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS)
	);

	// ???????? ??????? ???? ???? ??? ???? ???? ???????.
	m_nMsaa4xQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;

	// ?�� ????, ??? 0.
	hResult = m_pd3dDevice->CreateFence(
		0,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pd3dFence)
	);
	m_nFenceValue = 0;

	// ?�� ??? ???? ???? ??? ???? (???? ???? FALSE, ??? FALSE).
	m_hFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

void CGameFramework::CreateCommandQueueAndList()
{
	// Direct ��??? ? ????.
	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	HRESULT hResult = m_pd3dDevice->CreateCommandQueue(
		&d3dCommandQueueDesc,
		IID_PPV_ARGS(&m_pd3dCommandQueue)
	);

	// ��??? ????? ????.
	hResult = m_pd3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&m_pd3dCommandAllocator)
	);

	// ��??? ????? ????.
	hResult = m_pd3dDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_pd3dCommandAllocator.Get(),
		NULL,
		IID_PPV_ARGS(&m_pd3dCommandList)
	);

	// ???? ??????? ???? ???? Open ???????? Close ?? ???��?.
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

	// ??????? ???? ??????? RTV ??????? ??.
	HRESULT hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dRtvDescriptorHeap)
	);

	m_nRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// DSV ??????? ??(1??).
	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	hResult = m_pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		IID_PPV_ARGS(&m_pd3dDsvDescriptorHeap)
	);

	m_nDsvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

// ??????? ?? ????? ???? RTV ?? ?????.
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

	// ????-????? ???? ????? ??.
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

	// ????-????? ?? ????.
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

	// ???? ??? ???? - ?????/???????/???????/??? ?????.
	m_pCamera = std::make_unique<CCamera>();
	m_pCamera->SetViewport(0, 0, m_nWndClientWidth, m_nWndClientHeight, 0.0f, 1.0f);
	m_pCamera->SetScissorRect(0, 0, m_nWndClientWidth, m_nWndClientHeight);
	m_pCamera->GenerateProjectionMatrix(1.0f, FRUSTUM_FAR_PLANE,
		float(m_nWndClientWidth) / float(m_nWndClientHeight), 90.0f);
	m_pCamera->GenerateViewMatrix(XMFLOAT3(0.0f, 0.0f, -50.0f), XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f));

	// ?? ???? ?? ?? ?????? ??? ???? ??? ????.
	m_pScene = std::make_unique<CScene>();
	m_pScene->BuildObjects(m_pd3dDevice.Get(), m_pd3dCommandList.Get());

	// TPS ?????????? ????? ?��???? ??(???? ???). ??/????/????? ??? ?????? ???? ??????.
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
	// �÷��̾� �Ѿ�: ��� ť�� (���� �� ����)
	m_pBulletMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.4f, 0.4f, 0.4f, true, XMFLOAT4(1.0f, 0.9f, 0.2f, 1.0f));

	// �� �Ѿ�: �ð� ������ ���� ���� ť��.
	m_pEnemyBulletMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.4f, 0.4f, 0.4f, true, XMFLOAT4(1.0f, 0.25f, 0.20f, 1.0f));

	// �� ���� �޽�: ��ο� ���� �迭 ť��. �÷��̾�(������) �� �ð� ����.
	m_pEnemyMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		1.2f, 2.6f, 1.2f, true, XMFLOAT4(0.45f, 0.15f, 0.55f, 1.0f));

	// 소총 메시: 가늘고 긴 직육면체 (어두운 회색). 깊이가 길이 축이며 +Z 방향이 총구.
	// 깊이는 1.2 로 잡아 FPS 모드에서 카메라 근평면(=1.0) 클리핑을 피할 수 있도록 한다.
	m_pRifleMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.18f, 0.18f, 1.2f, true, XMFLOAT4(0.25f, 0.25f, 0.28f, 1.0f));
	m_pRifle = std::make_shared<CGameObject>();
	m_pRifle->SetMesh(m_pRifleMesh);

	// 적 마커 기둥 메시: 가늘고 매우 긴 노란 기둥. 잔여 적 ≤3 일 때 적 머리 위에 표시.
	m_pEnemyMarkerMesh = std::make_shared<CCubeMeshDiffused>(
		m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
		0.15f, 4.0f, 0.15f, true, XMFLOAT4(1.0f, 0.85f, 0.1f, 1.0f));

	// HUD ���̴� (���ڼ� + ������ �� ����). ���� OFF, �ø� OFF.
	// CreateShader �� ���� �Լ��̹Ƿ� base �����ͷ� ȣ���ص� CHudShader::CreateShader �� ����ġ�ȴ�.
	m_pHudShader = std::make_shared<CHudShader>();
	if (m_pScene) {
		m_pHudShader->CreateShader(m_pd3dDevice.Get(), m_pScene->GetGraphicsRootSignature());
	}

	// ȭ�� ���߾� ���� ���ڼ�(+) ������. ������ NDC(Ŭ�� ����) ��ǥ�� �̸�
	// ���� �����Ƿ� ī�޶� ȸ��/�̵��� �����ϰ� �׻� ȭ�� ������� �׷�����.
	{
		auto pCrosshairMesh = std::make_shared<CCrosshairMesh>(
			m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
			UINT(m_nWndClientWidth), UINT(m_nWndClientHeight),
			10u, 2u, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
		m_pCrosshair = std::make_shared<CGameObject>();
		m_pCrosshair->SetMesh(pCrosshairMesh);
		m_pCrosshair->SetShader(m_pHudShader);
	}

	// ������ �� ĭ 10�� ����. ��� ���� ��(����)���� �����, �� ������ m_nPlayerLife
	// ������ŭ �տ������͸� �����Ͽ� ���� ĭ�� �׸��� �ʴ´�.
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

	// 적 잔여 수 점 카운트 (좌상단). 최대 적 수 10 만큼 미리 생성하고 살아있는 적
	// 수만큼만 앞에서부터 그린다. CHudQuadMesh 로 NDC 좌표를 직접 지정.
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

		m_pCountPips.reserve(10);
		for (int i = 0; i < 10; ++i) {
			const float xL = xLBase + float(i) * (pipNdcW + gapNdc);
			const float xR = xL + pipNdcW;
			auto pPipMesh = std::make_shared<CHudQuadMesh>(
				m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
				xL, xR, yT, yB,
				XMFLOAT4(0.95f, 0.85f, 0.20f, 1.0f));
			auto pPipObj = std::make_shared<CGameObject>();
			pPipObj->SetMesh(pPipMesh);
			pPipObj->SetShader(m_pHudShader);
			m_pCountPips.push_back(std::move(pPipObj));
		}
	}

	// 승리 메시지 "WIN" — 7-세그먼트 스타일로 W/I/N 3 글자를 NDC 박스 조각들로 구성.
	// 각 글자는 픽셀 단위 폭/높이로 정의한 뒤 NDC 로 환산, 화면 중앙(약간 위쪽)에 배치한다.
	{
		const float wPx = float(m_nWndClientWidth  > 0 ? m_nWndClientWidth  : 1);
		const float hPx = float(m_nWndClientHeight > 0 ? m_nWndClientHeight : 1);
		const float kLetterW = 80.0f;       // 글자 폭(px)
		const float kLetterH = 120.0f;      // 글자 높이(px)
		const float kStroke  = 14.0f;       // 획 두께(px)
		const float kLetterGap = 30.0f;     // 글자 사이 간격(px)
		const XMFLOAT4 col(1.0f, 0.85f, 0.20f, 1.0f);

		// 화면 중앙(약간 위쪽) 기준 좌측 시작 x(px).
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

		// W (4 stroke): 좌세로, 우세로, 좌하 대각 대신 단순 가로 하단 + 가운데 짧은 위 막대.
		{
			const float L = startXPx;
			const float R = L + kLetterW;
			const float T = topYPx;
			const float B = topYPx + kLetterH;
			// 좌 세로
			addQuad(L, L + kStroke, T, B);
			// 우 세로
			addQuad(R - kStroke, R, T, B);
			// 가운데 짧은 세로 (위쪽으로 1/3)
			const float midX = (L + R) * 0.5f;
			addQuad(midX - kStroke * 0.5f, midX + kStroke * 0.5f, B - kLetterH * 0.55f, B);
			// 하단 가로
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
		// N (3 stroke): 좌세로 + 우세로 + 대각선 대신 상단 가로 두꺼운 막대로 단순화
		{
			const float L = startXPx + (kLetterW + kLetterGap) * 2.0f;
			const float R = L + kLetterW;
			const float T = topYPx;
			const float B = topYPx + kLetterH;
			// 좌 세로
			addQuad(L, L + kStroke, T, B);
			// 우 세로
			addQuad(R - kStroke, R, T, B);
			// 상단 대각선 단순화 — 상단 가로 + 우측 끝 살짝 내려오는 짧은 세로 강조
			addQuad(L, R, T, T + kStroke);
		}
	}

	// ������ ���� �ݹ� ���. Scene ���� EnemyBullet �� Player �浹 �� ȣ��ȴ�.
	if (m_pScene) {
		m_pScene->SetOnPlayerHit([this]() {
			if (m_nPlayerLife > 0) --m_nPlayerLife;
			if (m_nPlayerLife <= 0) m_bResetPending = true;
		});
	}

	// ��??? ??????? ??? ??? ??????? GPU ?? ??? ???��? ??????? ???.
	m_pd3dCommandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	// GPU ?? ???��? ?? ????? ?????? ??????.
	WaitForGPUComplete();

	// ???��?? ??? ??????? ????(???? ????? ????).
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
			// F9: ??????/???? ???.
			ChangeSwapChainState();
			break;
		case 'V':
			// V: FPS / TPS ???? ??? (?? C). ????? ?????? ???? ????? ???��?.
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
	// 1???/3??? ???(???�J ??, WASD, ????) ????? MAP1/MAP2 ?????? ??????.
	const bool bGameplay = (state == SceneState::MAP1 || state == SceneState::MAP2);
	if (!bGameplay) {
		// LANDING/MAP_SELECT: ???�J ��?? ???, ��? ????.
		if (m_bMouseCaptured) {
			::ShowCursor(TRUE);
			m_bMouseCaptured = false;
		}
		return;
	}

	// Tick the shoot cooldown so the next click can fire when this hits 0.
	if (m_fFireCooldown > 0.0f) m_fFireCooldown -= m_GameTimer.GetTimeElapsed();

	const bool bForeground = (::GetForegroundWindow() == m_hWnd);

	// =============== ???�J ?? ===============
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
				// 마우스 Y 부호(화면 아래=양수)와 pitch 부호(위=양수)를 입력단에서 통일한다.
				// 이전에 Rotate(-fPitch, ...) 로 처리하던 부호 반전을 여기서 흡수한다.
				const float fPitch = XMConvertToRadians(static_cast<float>(-dy) * kSensitivityDeg);
				m_pCamera->Rotate(fPitch, fYaw);
				::SetCursorPos(m_ptWndCenterScreen.x, m_ptWndCenterScreen.y);
			}
			m_ptWndCenterScreen = ptCenter;
		}
	}

	// =============== WASD + ?�� (XZ) ===============
	// ???? ??? ?��???? ????? m_xmf3PlayerPos. ???? TPS ?? ?? ??? ???? ????? ????????,
	// ???/?��/??? ?????? ??? m_xmf3PlayerPos ?? ???? ???????.
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

	// =============== ???? + ??? (Y) ===============
	// ???? ????: 3 * STEP_H (~2.1) ???? ???. v0 = sqrt(2 * g * h).
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
		// ????: ??? ????, y ????.
		m_fVerticalVelocity -= kGravity * dt;
		next.y = pos.y + m_fVerticalVelocity * dt;
		if (next.y <= groundY) {
			next.y = groundY;
			m_fVerticalVelocity = 0.0f;
			m_bGrounded = true;
		}
	}
	else {
		// ????: ?? ????? ???? ?? ??? ???? (??? ????????? ????????).
		next.y = groundY;
	}

	// ???? ??? ?��???? ??? ????.
	m_xmf3PlayerPos = next;

	// ???? ????? ??? ???? ?��?.
	if (m_pCamera->GetMode() == ECameraMode::FPS) {
		// FPS: ???? ??? = ?��???? ?? ???.
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
		// TPS: ?��???? ??? kBack ???, ???? kUp ??? ?????? ????? ???? ?��?.
		// Spring-arm: clamp the camera-to-player distance if a wall is in
		// the way so the camera does not pop through level geometry.
		// TPS sphere orbit: yaw=����, pitch=���� �˵� ����. ī�޶�� �׻� �÷��̾ �ٶ󺻴�.
		// TPS: 어깨 너머(over-the-shoulder) 카메라.
		// 카메라의 yaw/pitch 방향은 Rotate() 에서 이미 FPS 와 동일하게 갱신되므로
		// look 벡터가 곳 조준점 방향이다. 위치만 플레이어 뒤로 오프셋한다.
		// TPS 구면 궤도: yaw=수평, pitch=수직 궤도 각도. 카메라는 항상 플레이어를
		// 중심으로 공전하며 플레이어를 바라본다(화면 중앙 고정). 총알 방향은 별도로
		// yaw/pitch 에서 조준 벡터를 계산해 화면 조준선과 일치시킨다(FireBullet 참고).
		const float kBack = 7.0f;   // 플레이어 ↔ 카메라 궤도 반지름
		const float kUp   = 1.5f;   // 추가 수직 오프셋 (머리 위 약간)

		const float yaw   = m_pCamera->GetYaw();
		const float pitch = m_pCamera->GetPitch();

		// 수평 후방 단위 벡터 (벽 클램프 대상). pitch 와 무관하게 yaw 만 사용.
		const XMFLOAT3 backDir{ -sinf(yaw), 0.0f, -cosf(yaw) };

		// 구면 궤도 분해 — 수평 성분(반지름 * cos pitch), 수직 성분(반지름 * sin pitch).
		const float horizBack = kBack * cosf(pitch);
		const float vertBack  = kBack * sinf(pitch);
		const float eyeY      = m_xmf3PlayerPos.y + vertBack + kUp;

		// 카메라-플레이어 사이 벽이 있으면 수평 거리를 클램프해 카메라 관통을 막는다.
		const float dist = ClampDistanceAgainstWalls(
			state, m_xmf3PlayerPos, backDir, horizBack, eyeY);

		XMFLOAT3 tpsEye{
			m_xmf3PlayerPos.x + backDir.x * dist,
			eyeY,
			m_xmf3PlayerPos.z + backDir.z * dist };

		// 카메라는 항상 플레이어를 바라본다. m_fPitch / m_fYaw 는 궤도 각도로 그대로
		// 유지되므로 다음 Rotate() 가 자연스럽게 누적된다 (SetPositionAndTarget 보장).
		m_pCamera->SetPositionAndTarget(tpsEye, m_xmf3PlayerPos);

		if (m_pPlayerModel) {
			XMFLOAT3 modelCenter{
				m_xmf3PlayerPos.x,
				m_xmf3PlayerPos.y - MAP_EYE_HEIGHT + 1.3f,
				m_xmf3PlayerPos.z };
			m_pPlayerModel->SetWorldYawAndPosition(m_pCamera->GetYaw(), modelCenter);
		}
	}

	// 카메라 위치/look 이 확정되었으므로 소총 transform 도 동기화한다.
	UpdateRifleTransform();
}

void CGameFramework::UpdateRifleTransform()
{
	if (!m_pRifle || !m_pCamera) return;
	if (!m_pScene) return;
	const SceneState st = m_pScene->GetCurrentState();
	if (st != SceneState::MAP1 && st != SceneState::MAP2) return;

	const float yaw   = m_pCamera->GetYaw();
	const float pitch = m_pCamera->GetPitch();
	// 조준선 방향 — 메시의 +Z 축이 향할 방향.
	const float cp = cosf(pitch);
	const XMFLOAT3 aim{ sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp };

	XMFLOAT3 pos;
	XMFLOAT3 fwd;

	if (m_pCamera->GetMode() == ECameraMode::FPS) {
		// FPS: 카메라 기준 우측 하단(어깨총처럼 화면 한쪽에 보이도록).
		// 근평면(=1.0) 클리핑 방지를 위해 소총 중심을 카메라 1.7 단위 전방에 둔다
		// (소총 깊이 1.2 / 2 = 0.6, 1.7 - 0.6 = 1.1 > 1.0 근평면).
		const XMFLOAT3 camPos   = m_pCamera->GetPosition();
		const XMFLOAT3 camRight = m_pCamera->GetRight();
		const float kForward = 1.7f;
		const float kSide    = 0.45f;
		const float kDown    = 0.35f;
		pos.x = camPos.x + camRight.x * kSide + aim.x * kForward;
		pos.y = camPos.y - kDown              + aim.y * kForward;
		pos.z = camPos.z + camRight.z * kSide + aim.z * kForward;
		fwd = aim;
	}
	else {
		// TPS: 플레이어 모델 오른쪽 어깨 옆. 메시 중심 높이는 modelCenter 와 동일.
		const XMFLOAT3 playerYawRight{ cosf(yaw), 0.0f, -sinf(yaw) };
		const float modelCenterY = m_xmf3PlayerPos.y - MAP_EYE_HEIGHT + 1.3f;
		pos.x = m_xmf3PlayerPos.x + playerYawRight.x * 0.85f + sinf(yaw) * 0.4f;
		pos.y = modelCenterY;
		pos.z = m_xmf3PlayerPos.z + playerYawRight.z * 0.85f + cosf(yaw) * 0.4f;
		fwd = aim;
	}

	m_pRifle->SetWorldOrientation(fwd, pos);
}

void CGameFramework::FireBullet()
{
	if (!m_pScene || !m_pCamera || !m_pBulletMesh) return;
	const SceneState state = m_pScene->GetCurrentState();
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return;
	if (m_fFireCooldown > 0.0f) return;

	// 조준 방향(=조준선 방향): 카메라 모드와 무관하게 yaw/pitch 로부터 계산한다.
	// FPS 에선 m_pCamera->GetLook() 과 동일. TPS 에선 카메라가 플레이어를 바라보므로
	// GetLook() 을 그대로 쓸 수 없다 — 이 식이 정답.
	const float yaw   = m_pCamera->GetYaw();
	const float pitch = m_pCamera->GetPitch();
	const float cp = cosf(pitch);
	const XMFLOAT3 aim{ sinf(yaw) * cp, sinf(pitch), cosf(yaw) * cp };

	// 발사 원점: 소총 총구. 소총 transform 은 UpdateRifleTransform 이 매 프레임 갱신한다.
	// 소총이 아직 준비되지 않았다면 플레이어 머리 앞쪽으로 폴백.
	XMFLOAT3 origin;
	if (m_pRifle) {
		const XMFLOAT3 rpos = m_pRifle->GetPosition();
		const XMFLOAT3 rfwd = m_pRifle->GetLook(); // SetWorldOrientation 에서 _31..33 = forward
		const float kMuzzle = 0.7f; // 소총 깊이 1.2 의 절반(0.6) + 약간 (총알이 메시 외부에서 시작하도록)
		origin = XMFLOAT3{
			rpos.x + rfwd.x * kMuzzle,
			rpos.y + rfwd.y * kMuzzle,
			rpos.z + rfwd.z * kMuzzle };
	}
	else {
		origin = XMFLOAT3{
			m_xmf3PlayerPos.x + aim.x * 0.8f,
			m_xmf3PlayerPos.y + 0.2f,
			m_xmf3PlayerPos.z + aim.z * 0.8f };
	}

	const float kBulletSpeed = 50.0f;
	auto pBullet = std::make_shared<CBulletObject>(origin, aim, kBulletSpeed, EObjectTag::Bullet);
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

	// �� �Ѿ��� �÷��̾� �Ѿ˺��� ���� ���� ȸ�� ���ɼ��� �����.
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

	// ���� AABB half y = 1.3 (��ü ���� 2.6). �÷��̾� �ݰ� 5Ÿ�� �ٱ����� ������ ����.
	const float kEnemyHalfY = 1.3f;
	std::vector<XMFLOAT3> spawns = PickEnemySpawnPositions(state, m_xmf3PlayerPos, 10, kEnemyHalfY);

	std::mt19937 seedGen{ std::random_device{}() };
	for (const XMFLOAT3& pos : spawns) {
		// �ν��Ͻ��� �õ�� RNG ������ Ȯ��.
		const unsigned int nSeed = seedGen();
		auto pEnemy = std::make_shared<CEnemyObject>(state, nSeed);
		pEnemy->SetMesh(m_pEnemyMesh);
		pEnemy->SetPosition(pos);
		// ���� �Ѿ��� �߻��� �� ȣ��Ǵ� �ݹ� ? GameFramework �� SpawnEnemyBullet �� ����.
		pEnemy->SetFireCallback([this](const XMFLOAT3& xmf3Origin, const XMFLOAT3& xmf3Dir) {
			SpawnEnemyBullet(xmf3Origin, xmf3Dir);
		});
		// �þ� ���� / ���� ���� ����� ���� �÷��̾��� ���� ��ġ�� �����´�.
		pEnemy->SetPlayerPosGetter([this]() {
			return m_xmf3PlayerPos;
		});
		// 머리 위 마커 메시 주입. 가시성은 AnimateObjects 가 잔여 적 수에 따라 토글한다.
		if (m_pEnemyMarkerMesh) {
			pEnemy->SetMarkerMesh(m_pEnemyMarkerMesh);
		}
		m_pScene->AddObjectToCurrentMap(pEnemy);
	}
}

void CGameFramework::AnimateObjects()
{
	if (!m_pScene) return;
	m_pScene->AnimateObjects(m_GameTimer.GetTimeElapsed());

	// 게임플레이 맵에서만: 적 수 카운트 → 마커 토글 + 승리 타이머 처리.
	{
		const SceneState curState = m_pScene->GetCurrentState();
		if (curState == SceneState::MAP1 || curState == SceneState::MAP2) {
			const int nAlive = m_pScene->CountAliveEnemies();

			// 마커: 잔여 1~3 마리일 때만 표시. 0 이거나 4+ 면 숨김.
			m_pScene->SetEnemyMarkersVisible(nAlive > 0 && nAlive <= 3);

			// 승리 타이머: 적 0 마리이고 아직 카운트다운 중이 아니면 시작.
			if (nAlive == 0 && m_fVictoryTimer < 0.0f) {
				m_fVictoryTimer = 2.0f;
			}
			// 카운트다운이 활성이면 매 프레임 감소. 0 도달 시 LANDING 복귀 트리거.
			if (m_fVictoryTimer >= 0.0f) {
				m_fVictoryTimer -= m_GameTimer.GetTimeElapsed();
				if (m_fVictoryTimer <= 0.0f) {
					m_fVictoryTimer = -1.0f;
					m_bResetPending = true;
				}
			}
		}
	}

	// ???? ????? GAME START ????? ?????? ?? ???? ???(MAP_SELECT) ???? ???.
	if (m_pScene->IsGameStartRequested() && m_pScene->GetCurrentState() == SceneState::LANDING) {
		m_pScene->TransitionToScene(SceneState::MAP_SELECT);
		m_pScene->ClearGameStartRequest();
		SetupMapSelectCamera();
	}

	// ?? ???? ????? ????? ????? ???? ????? ?????? ???.
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

	// ������ 0 ���� ��� �� �����÷��� ��ü ���� �� LANDING ȭ������ ����.
	// ����ڰ� �ٽ� GAME START �� ������ MAP_SELECT �� MAP1/2 �帧���� �� ������ ���۵ȴ�.
	if (m_bResetPending) {
		m_bResetPending = false;
		m_pScene->ResetGameplayState();
		m_nPlayerLife = 10;
		// 방어적으로 승리 타이머도 초기화 (정상 흐름은 이미 -1, 사망 reset 경로 대비).
		m_fVictoryTimer = -1.0f;
		m_pScene->TransitionToScene(SceneState::LANDING);
		// ���콺 ĸó / �߻� ��ٿ� / ���� ���� �ʱ�ȭ ? ���� ���ӿ��� �����ϰ� ����.
		if (m_bMouseCaptured) {
			::ShowCursor(TRUE);
			m_bMouseCaptured = false;
		}
		m_fFireCooldown = 0.0f;
		m_fVerticalVelocity = 0.0f;
		m_bGrounded = true;
		// ī�޶� ���� ȭ�� �������� �ǵ�����.
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
	// ?? ???? 1??? ???? ???/?????? ?? ???? MapInfo ???? ?????��?.
	MapInfo info;
	switch (state) {
	case SceneState::MAP1: info = GetMap1Info(); break;
	case SceneState::MAP2: info = GetMap2Info(); break;
	default: return; // LANDING ?? ?????? ???? ???????.
	}
	m_pCamera->GenerateViewMatrix(info.cameraPosition, info.cameraLookAt, XMFLOAT3(0.0f, 1.0f, 0.0f));
	// ???? ??? ?��???? ????? ???? ???? ????? ????.
	m_xmf3PlayerPos = info.cameraPosition;
	// ?????? ?? ?? ???? ?? ??? FPS ?? ????.
	m_pCamera->SetMode(ECameraMode::FPS);
	if (m_pScene) m_pScene->SetPlayerVisible(false);
	// ???? ProcessInput ???? ??? ???�J ��??? ?????? ???.
	m_bMouseCaptured = false;
	// ???? ???? ????.
	m_fVerticalVelocity = 0.0f;
	m_bGrounded = true;
	// Bullets fired on the previous map should not carry over; the cooldown
	// is per-life, so reset it too.
	m_fFireCooldown = 0.0f;

	// ������ / �� / ���� ��ü ����. ���� ������ ���� ��ü�� �����ϰ� �� ���� �����Ѵ�.
	// LANDING �� MAP_SELECT �� MAP1/2 �� ������ ��� ��ο��� ȣ��ǹǷ�,
	// �װ� �ٽ� �����ϴ� �帧�� ù ���� ���� �帧�� �����ϰ� �����Ѵ�.
	m_nPlayerLife = 10;
	m_bResetPending = false;
	// 승리 타이머 초기화 — 새 게임 시작 시 카운트다운/메시지 잔여 상태 정리.
	m_fVictoryTimer = -1.0f;
	if (m_pScene) m_pScene->ResetGameplayState();
	SpawnEnemiesForMap(state);

	// �÷��̾� ���� ��ġ�� �� ���� ��ġ�� ��� ����ȭ�Ͽ�, ù ������ �浹 ������
	// ���� ��ġ�� �������� �ʰ� �Ѵ� (Scene::AnimateObjects �� EnemyBullet �� Player).
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
	// ?? ???? ??? ????: ??? ?? ?????? ????????? ????.
	m_pCamera->GenerateViewMatrix(
		XMFLOAT3(0.0f, 5.0f, -55.0f),
		XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 1.0f, 0.0f));
	// ???�J ��??? ??????? ��???? ????? ???.
	if (m_bMouseCaptured) {
		::ShowCursor(TRUE);
		m_bMouseCaptured = false;
	}
	// ???? ???? ????.
	m_fVerticalVelocity = 0.0f;
	m_bGrounded = true;
}

void CGameFramework::WaitForGPUComplete()
{
	// ???? ?????? ???? Fence Value ?? 1 ????.
	UINT64 nFenceValue = ++m_nFenceValues[m_nSwapChainBufferIndex];

	// cmdQueue ?? ?????? ???, GPU ?? ?????? ??? ?????? ?�� ???? nFenceValue ?? ?????.
	HRESULT hResult = m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFenceValue);

	// ???? ?��?? nFenceValue ?? ???????? ?????? ?????? ???.
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
	// ?????? ?��? ????, ???/???????/?????? ????.
	m_GameTimer.Tick(0.0f);

	ProcessInput();

	AnimateObjects();

	// ��??? ?????/????? ????.
	HRESULT hResult = m_pd3dCommandAllocator->Reset();
	hResult = m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL);

	// ???? ?????? PRESENT ???��??? RENDER_TARGET ???��? ???.
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

	// ??????? RTV CPU ????? ?????��?.
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =
		m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (m_nSwapChainBufferIndex * m_nRtvDescriptorIncrementSize);

	// ????-????? ????????? CPU ????? ?????��?.
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

	// ????/????? ?????.
	m_pd3dCommandList->ClearDepthStencilView(
		d3dDsvCPUDescriptorHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0,
		0,
		NULL
	);

	// ?? ??????.
	if (m_pScene) m_pScene->Render(m_pd3dCommandList.Get(), m_pCamera.get());

	// 소총: 게임플레이 맵에서 FPS/TPS 양쪽 모두 표시. Scene::Render 직후이므로
	// CObjectsShader 의 PSO 가 그대로 바인딩되어 있어 별도 PSO 전환 없이 동일 파이프
	// 라인으로 그릴 수 있다. 소총 GameObject 는 shader 미할당이지만 m_xmf4x4World 만
	// 바인딩되면 충분하다.
	if (m_pRifle && m_pScene && m_pCamera) {
		const SceneState st = m_pScene->GetCurrentState();
		if (st == SceneState::MAP1 || st == SceneState::MAP2) {
			m_pRifle->Render(m_pd3dCommandList.Get(), m_pCamera.get());
		}
	}

	// ���ڼ��� ���� �÷��� ��(MAP1/MAP2)������ �׸���.
	// CGameObject::Render �� CHudShader �� PSO �� ��ȯ�� �� �޽ø� �׸��Ƿ�
	// ���� �׽�Ʈ�� ���� �־� �׻� �ٸ� ��� �ȼ� ���� ǥ�õȴ�.
	if (m_pCrosshair && m_pScene && m_pCamera) {
		const SceneState st = m_pScene->GetCurrentState();
		if (st == SceneState::MAP1 || st == SceneState::MAP2) {
			m_pCrosshair->Render(m_pd3dCommandList.Get(), m_pCamera.get());

			// ������ ��: ȭ�� �߾� �ϴ�. m_nPlayerLife ������ŭ �տ������� �����Ѵ�.
			// ���� ĭ�� �׸��� �ʴ� �ܼ� ���. CHudShader �� �����ϹǷ� PSO ��ȯ ��� ����.
			const int nDraw = (m_nPlayerLife < 0) ? 0 : (m_nPlayerLife > 10 ? 10 : m_nPlayerLife);
			for (int i = 0; i < nDraw && i < static_cast<int>(m_pLifeBarSegments.size()); ++i) {
				if (m_pLifeBarSegments[i]) {
					m_pLifeBarSegments[i]->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}

			// 적 잔여 수 점 카운트(좌상단). 살아있는 적 수만큼만 앞에서부터 그린다.
			const int nAlive = m_pScene->CountAliveEnemies();
			const int nDrawPips = (nAlive < 0) ? 0
				: (nAlive > static_cast<int>(m_pCountPips.size()) ? static_cast<int>(m_pCountPips.size()) : nAlive);
			for (int i = 0; i < nDrawPips; ++i) {
				if (m_pCountPips[i]) {
					m_pCountPips[i]->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}

			// 승리 메시지 — 타이머가 활성일 때만 화면 중앙에 "WIN" 표시.
			if (m_fVictoryTimer >= 0.0f) {
				for (auto& pLetter : m_pWinLetters) {
					if (pLetter) pLetter->Render(m_pd3dCommandList.Get(), m_pCamera.get());
				}
			}
		}
	}

	// PRESENT ???��? ??? ???.
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	// ???? ????? ????.
	hResult = m_pd3dCommandList->Close();

	// ???? ? ????.
	ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
	m_pd3dCommandQueue->ExecuteCommandLists(
		_countof(ppd3dCommandLists),
		ppd3dCommandLists
	);

	// GPU ??? ???.
	WaitForGPUComplete();

	// ?????? ??? ?? FPS ��?? ????.
	m_pdxgiSwapChain->Present(0, 0);
	MoveToNextFrame();
	m_GameTimer.GetFrameRate(m_pszFrameRate + 12, 37);
	::SetWindowText(m_hWnd, m_pszFrameRate);
}

CGameFramework::~CGameFramework()
{

}
