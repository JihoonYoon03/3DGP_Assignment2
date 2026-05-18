#pragma once

#include "Scene.h"

#include "Timer.h"

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

	// Setup the in-game camera (called when transitioning Landing -> Map).
	void SetupGameCamera();

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
	// ID3D12Device - IDXGIAdapter(그래픽카드 하드웨어)를 소프트웨어적으로 추상화한 객체
	// 매우 중요한 객체. 모든 렌더링 리소스와 객체는 device를 통해 생성
	// 리소스 및 객체 생성(ID3D12Device::Create) 수행
	// 버퍼/텍스처/뷰(RTV, DSV, SRV)/파이프라인 상태, 실행 도구(CmdQueue/List/Allocator), Fence 등
	// 하드웨어 기능 점검(레이트레이싱 가능 여부 등), CheckFeatureSupport로 진행
	// 가상 메모리 관리. GPU 메모리에 데이터 할당 및 관리하는 인터페이스 제공(CreateCommittedResource, CreateReservedResource 등)
	// 멀티스레드 환경에서 안전해서, 여러 스레드에서 동시에 Create 해도 문제 없음
	// 모든 리소스의 부모이자 GPU와 직접 소통하는 창구 역할
	ComPtr<ID3D12Device>		m_pd3dDevice;

	// MSAA 다중 샘플링을 활성화하고 다중 샘플링 레벨을 설정한다.
	bool m_bMsaa4xEnable = false;
	UINT m_nMsaa4xQualityLevels = 0;

	// 스왑 체인의 후면 버퍼의 개수이다.
	static const UINT	m_nSwapChainBuffers = 2;
	// 현재 스왑 체인의 후면 버퍼 인덱스이다.
	UINT				m_nSwapChainBufferIndex;

	// DescriptorHeap(= Resource View) - GPU는 메모리 리소스를 직접 읽지 못 함.
	// 따라서 리소스가 어디에 있는지, 어떤 형식인지 설명해주는 "설명서" = Descriptor.
	// 이것을 모아놓은 공간이 DescriptorHeap. 연속된 메모리 배열이므로 아주 빠르게 리소스 접근 가능.

	// Render Target View, DescHeap 인터페이스 포인터, RTV 서술자 원소의 크기이다.
	// RTV - 결과물을 그려넣을 버퍼 설명서
	ComPtr<ID3D12Resource>			m_ppd3dSwapChainBackBuffers[m_nSwapChainBuffers];
	ComPtr<ID3D12DescriptorHeap>	m_pd3dRtvDescriptorHeap;
	UINT							m_nRtvDescriptorIncrementSize;

	// Depth Stencil View, 서술자 힙 인터페이스 포인터, DSV 원소의 크기이다.
	// DSV - 깊이/스텐실 테스트용 버퍼 설명서
	ComPtr<ID3D12Resource>			m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap>	m_pd3dDsvDescriptorHeap;
	UINT							m_nDsvDescriptorIncrementSize;
	

	// 명령 큐, 명령 할당자, 명령 리스트 인터페이스 포인터이다.
	// Allocator에 List를 통해 작성된 Command를 Queue에 넣어서 실행시킨다.
	ComPtr<ID3D12CommandQueue>			m_pd3dCommandQueue;
	// 실제 명령 데이터가 쌓이는 공간 - CommandAllocator.
	// cmdList로 명령을 기록하는 동안, cmdAllocator를 건드리면 안 된다. 기록 중 수정 X
	// 기록이 끝나고 GPU가 명령을 모두 실행하면, Fence를 통해 작업 완료를 확인하고
	// cmdAllocator를 Reset()한다.
	// Reset 시, 메모리를 해제하는 것이 아니라 쓰기 포인터만 앞으로 이동시켜
	// 다음 명령을 덮어쓴다.
	ComPtr<ID3D12CommandAllocator>		m_pd3dCommandAllocator;
	// 명령을 작성하는 펜 - CommandList
	// 멀티스레드 환경에선 Allocator를 스레드마다 가져야 충돌 없음. List는 공유해도 무방함
	ComPtr<ID3D12GraphicsCommandList>	m_pd3dCommandList;

	// 그래픽스 파이프라인 상태 객체에 대한 인터페이스 포인터이다.
	ComPtr<ID3D12PipelineState>			m_pd3dPipelineState;


	// 펜스 인터페이스 포인터, 펜스의 값, 이벤트 핸들이다.
	// Fence - CPU와 GPU의 동기화 목적. CPU는 GPU 연산을 기다리지 않으므로(비동기),
	// GPU가 사용중인 데이터를 수정해버리면 오류 발생.
	// GPU가 작업 완료시 Fence값을 바꾸고, CPU는 그 값을 감시하다가 바뀌게 되면 다음 연산.
	// 또, 특정 리소스(버퍼)가 사용중인지 확인하여 덮어쓰기 방지, 프레임 제한의 기능도 있다.
	ComPtr<ID3D12Fence>	m_pd3dFence;
	UINT64				m_nFenceValue;
	// Event - 신호등. 특정 조건 만족 시 파란불
	HANDLE				m_hFenceEvent;

	// 다음은 게임 프레임워크에서 사용할 타이머이다.
	CGameTimer m_GameTimer;

	// 다음은 프레임 레이트를 주 윈도우의 캡션에 출력하기 위한 문자열이다.
	_TCHAR m_pszFrameRate[50];

	// 후면버퍼마다 현재 펜스 값을 관리하기 위한 멤버 변수.
	UINT64 m_nFenceValues[m_nSwapChainBuffers];

	// 씬을 위한 멤버 변수
	std::unique_ptr<CScene> m_pScene;
};

