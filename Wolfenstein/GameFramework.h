#pragma once

#include "Scene.h"

#include "Timer.h"
#include "Maps.h"

class CGameObject;

class CGameFramework {
public:
	CGameFramework();
	~CGameFramework();

	// 프레임워크 초기화 함수이다 (주 윈도우 생성 시 호출)
	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);

	void OnDestroy();

	// 스왑 체인, 디바이스, 서술자 힙, 명령 큐/할당자/리스트를 생성하는 함수
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateDirect3DDevice();
	void CreateCommandQueueAndList();

	// 렌더 타겟 뷰와 깊이-스텐실 뷰를 생성하는 함수
	void CreateRenderTargetViews();
	void CreateDepthStencilView();

	// 렌더링할 메쉬와 게임 객체를 생성하고 소멸하는 함수
	void BuildObjects();
	void ReleaseObjects();

	// 프레임워크의 핵심(사용자 입력, 애니메이션, 렌더링)을 구성하는 함수
	void ProcessInput();
	void AnimateObjects();
	void FrameAdvance();

	// CPU와 GPU를 동기화하는 함수
	void WaitForGPUComplete();

	// 윈도우의 메시지(키보드, 마우스 입력)를 처리하는 함수이다.
	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void ChangeSwapChainState();

	// 인게임 1인칭/3인칭 카메라를 해당 맵 상태에 맞게 초기화한다.
	void SetupGameCamera(SceneState state);
	// 맵 선택 화면을 위한 미니어처 뷰 카메라 셋업.
	void SetupMapSelectCamera();

	void MoveToNextFrame();

	std::unique_ptr<class CCamera> m_pCamera;

private:
	// 아래 구역은 그려질 화면 크기와 관련 있는 부분이다.
	// -------------------------------
	HINSTANCE	m_hInstance;
	HWND		m_hWnd;

	int			m_nWndClientWidth;
	int			m_nWndClientHeight;
	// -------------------------------

	// ComPtr - COM 객체들을 RAII에 맞게 알아서 관리해줌
	// Create~ 함수 등에서 해당 객체의 포인터를 요구할 경우 .Get()
	// 반환값을 받아야 하는 경우 .GetAddressOf()로 받으면 됨
	// 객체 초기화나 Release는 신경 쓸 필요 없음.

	// DXGI 팩토리 인터페이스에 대한 포인터
	// DXGI Factory - 시스템 그래픽 환경을 조사 및 제어
	// GPU혹은 내장 그래픽 탐지(IDXGIAdapter 탐색), 스왑체인 생성, 풀스크린 전환 제어
	// Adapter로 D3D12Device 생성
	ComPtr<IDXGIFactory4>	m_pdxgiFactory;

	// 스왑 체인 인터페이스에 대한 포인터. 주로 디스플레이를 제어하기 위해 필요
	// SwapChain - 2개 이상의 버퍼 세트를 교체하여 화면 티어링 현상 없게 함
	ComPtr<IDXGISwapChain3>	m_pdxgiSwapChain;

	// Direct3D 디바이스 인터페이스에 대한 포인터이다. 주로 리소스를 생성하기 위하여 필요하다.
	ComPtr<ID3D12Device>		m_pd3dDevice;

	// MSAA 다중 샘플링을 활성화하고 다중 샘플링 레벨을 설정한다.
	bool m_bMsaa4xEnable = false;
	UINT m_nMsaa4xQualityLevels = 0;

	// 스왑 체인의 후면 버퍼의 개수이다.
	static const UINT	m_nSwapChainBuffers = 2;
	// 현재 스왑 체인의 후면 버퍼 인덱스이다.
	UINT				m_nSwapChainBufferIndex;

	// Render Target View, DescHeap 인터페이스 포인터, RTV 서술자 원소의 크기이다.
	ComPtr<ID3D12Resource>			m_ppd3dSwapChainBackBuffers[m_nSwapChainBuffers];
	ComPtr<ID3D12DescriptorHeap>	m_pd3dRtvDescriptorHeap;
	UINT							m_nRtvDescriptorIncrementSize;

	// Depth Stencil View, 서술자 힙 인터페이스 포인터, DSV 원소의 크기이다.
	ComPtr<ID3D12Resource>			m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap>	m_pd3dDsvDescriptorHeap;
	UINT							m_nDsvDescriptorIncrementSize;

	// 명령 큐, 명령 할당자, 명령 리스트 인터페이스 포인터이다.
	ComPtr<ID3D12CommandQueue>			m_pd3dCommandQueue;
	ComPtr<ID3D12CommandAllocator>		m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList>	m_pd3dCommandList;

	// 그래픽스 파이프라인 상태 객체에 대한 인터페이스 포인터이다.
	ComPtr<ID3D12PipelineState>			m_pd3dPipelineState;

	// 펜스 인터페이스 포인터, 펜스의 값, 이벤트 핸들이다.
	ComPtr<ID3D12Fence>	m_pd3dFence;
	UINT64				m_nFenceValue;
	HANDLE				m_hFenceEvent;

	// 다음은 게임 프레임워크에서 사용할 타이머이다.
	CGameTimer m_GameTimer;

	// 다음은 프레임 레이트를 주 윈도우의 캡션에 출력하기 위한 문자열이다.
	_TCHAR m_pszFrameRate[50];

	// 후면버퍼마다 현재 펜스 값을 관리하기 위한 멤버 변수.
	UINT64 m_nFenceValues[m_nSwapChainBuffers];

	// 씬을 위한 멤버 변수
	std::unique_ptr<CScene> m_pScene;

	// 1인칭 마우스 룩 관련 상태.
	// m_bMouseCaptured: 마우스를 윈도우 중앙으로 캡처 중인지 여부.
	// m_ptWndCenterScreen: 윈도우 중앙의 화면 좌표 (delta 계산용).
	bool m_bMouseCaptured = false;
	POINT m_ptWndCenterScreen{ 0, 0 };

	// 점프/중력 상태.
	// m_fVerticalVelocity: 수직 속도(유닛/sec). 0보다 크면 상승 중.
	// m_bGrounded: 현재 바닥에 닿아 있는지 여부.
	// m_bPrevSpacePressed: 스페이스 키의 edge 검출용 이전 프레임 상태.
	float m_fVerticalVelocity = 0.0f;
	bool m_bGrounded = true;
	bool m_bPrevSpacePressed = false;

	// === 플레이어 위치 및 시점 토글 (요구 C) ===
	// m_xmf3PlayerPos: 플레이어의 권위 있는 위치(눈높이 기준). 카메라가 TPS 일 때
	// 뒤로 빠진 위치로 이동하더라도, 이동/충돌/중력은 이 변수만 사용한다.
	// m_pPlayerModel: TPS 모드에서만 그려지는 플레이어의 시각 모델(빨간 큐브).
	// m_bPrevVPressed: 'V' 키의 edge 검출은 OnProcessingKeyboardMessage WM_KEYUP 으로 처리하므로 따로 필요 없지만,
	// 추후 확장을 위해 유지.
	XMFLOAT3 m_xmf3PlayerPos{ 0.0f, MAP_EYE_HEIGHT, 0.0f };
	std::shared_ptr<CGameObject> m_pPlayerModel;
};
