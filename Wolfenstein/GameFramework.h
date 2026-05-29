#pragma once

#include "Scene.h"

#include "Timer.h"
#include "Maps.h"
#include "Player.h"

class CGameObject;

class CGameFramework {
public:
	CGameFramework();
	~CGameFramework();

	// 프레임워크 초기화 함수이다.
	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);
	void OnDestroy();

	// D3D12 디바이스/스왑체인/명령 큐 등을 생성하는 함수
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateDirect3DDevice();
	void CreateCommandQueueAndList();

	// 렌더 타겟 뷰와 깊이 스텐실 뷰를 생성하는 함수
	void CreateRenderTargetViews();
	void CreateDepthStencilView();

	// 게임 객체를 생성하고 소멸하는 함수
	void BuildObjects();
	void ReleaseObjects();

	// 프레임워크의 핵심(입력, 애니메이션, 렌더링)을 수행하는 함수
	void ProcessInput();
	void AnimateObjects();
	void FrameAdvance();

	// CPU와 GPU를 동기화하는 함수
	void WaitForGPUComplete();

	// 윈도우 메시지 처리 함수이다.
	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void ChangeSwapChainState();

	// 씬 상태에 맞춰 카메라를 셋업한다.
	void SetupGameCamera(SceneState state);
	void SetupMapSelectCamera();

	// 플레이어 총알 발사
	void FireBullet();

	// 적 총알 발사
	void SpawnEnemyBullet(const XMFLOAT3& xmf3Origin, const XMFLOAT3& xmf3Dir);

	// 현재 맵에 적을 스폰한다.
	void SpawnEnemiesForMap(SceneState state);

	// 소총의 월드 행렬을 카메라 모드에 맞게 갱신한다.
	void UpdateRifleTransform();

	// 조준점으로부터 광선을 쏴 첫 충돌점의 월드 좌표를 반환한다.
	XMFLOAT3 GetAimTargetPoint() const;

	void MoveToNextFrame();

	std::unique_ptr<class CCamera> m_pCamera;

private:
	HINSTANCE	m_hInstance;
	HWND		m_hWnd;

	int			m_nWndClientWidth;
	int			m_nWndClientHeight;

	// DXGI 팩토리 인터페이스 포인터
	ComPtr<IDXGIFactory4>	m_pdxgiFactory;
	// 스왑 체인 인터페이스 포인터
	ComPtr<IDXGISwapChain3>	m_pdxgiSwapChain;
	// Direct3D 디바이스 인터페이스 포인터
	ComPtr<ID3D12Device>		m_pd3dDevice;

	// MSAA 다중 샘플링 설정값
	bool m_bMsaa4xEnable = false;
	UINT m_nMsaa4xQualityLevels = 0;

	// 스왑 체인 후면 버퍼 개수와 인덱스
	static const UINT	m_nSwapChainBuffers = 2;
	UINT				m_nSwapChainBufferIndex;

	// RTV 관련
	ComPtr<ID3D12Resource>			m_ppd3dSwapChainBackBuffers[m_nSwapChainBuffers];
	ComPtr<ID3D12DescriptorHeap>	m_pd3dRtvDescriptorHeap;
	UINT							m_nRtvDescriptorIncrementSize;

	// DSV 관련
	ComPtr<ID3D12Resource>			m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap>	m_pd3dDsvDescriptorHeap;
	UINT							m_nDsvDescriptorIncrementSize;

	// 명령 큐/할당자/리스트
	ComPtr<ID3D12CommandQueue>			m_pd3dCommandQueue;
	ComPtr<ID3D12CommandAllocator>		m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList>	m_pd3dCommandList;

	// 그래픽스 파이프라인 상태 객체
	ComPtr<ID3D12PipelineState>			m_pd3dPipelineState;

	// 펜스
	ComPtr<ID3D12Fence>	m_pd3dFence;
	UINT64				m_nFenceValue;
	HANDLE				m_hFenceEvent;

	// 게임 타이머
	CGameTimer m_GameTimer;

	// 프레임 레이트 문자열
	_TCHAR m_pszFrameRate[50];

	// 후면버퍼별 펜스 값
	UINT64 m_nFenceValues[m_nSwapChainBuffers];

	// 현재 활성 씬
	std::unique_ptr<CScene> m_pScene;

	// 1인칭 마우스 캡처 상태
	bool m_bMouseCaptured = false;
	POINT m_ptWndCenterScreen{ 0, 0 };

	// 스페이스 키 edge 검출용
	bool m_bPrevSpacePressed = false;

	// 플레이어 객체와 TPS 모드용 모델
	std::shared_ptr<CPlayer> m_pPlayer;
	std::shared_ptr<CGameObject> m_pPlayerModel;

	// 총알 메시와 발사 쿨다운
	std::shared_ptr<CMesh> m_pBulletMesh;
	float m_fFireCooldown = 0.0f;
	// 화면 정중앙 십자선
	std::shared_ptr<CGameObject> m_pCrosshair;

	// 적/적 총알/HUD 셰이더
	std::shared_ptr<CMesh>   m_pEnemyMesh;
	std::shared_ptr<CMesh>   m_pEnemyBulletMesh;
	std::shared_ptr<CShader> m_pHudShader;
	// 라이프 바 10칸
	std::vector<std::shared_ptr<CGameObject>> m_pLifeBarSegments;
	// 사망 후 LANDING 복귀 트리거
	bool m_bResetPending = false;

	// 플레이어 사망 페이드 타이머
	float m_fPlayerDeathTimer = -1.0f;
	static constexpr float kPlayerDeathDuration  = 1.5f;
	static constexpr float kPlayerDeathPitchRate = -0.7f;

	// 소총 메시 (플레이어용/적용)
	std::shared_ptr<CMesh>      m_pRifleMesh;
	std::shared_ptr<CMesh>      m_pEnemyRifleMesh;

	// 반동 애니메이션 지속 시간
	static constexpr float kRecoilDuration = 0.25f;

	// 적 잔여 수 카운트 점 (좌상단)
	std::vector<std::shared_ptr<CGameObject>> m_pCountPips;

	// 적 머리 위 마커 기둥 메시
	std::shared_ptr<CMesh> m_pEnemyMarkerMesh;

	// 승리 시 표시되는 WIN 글자와 타이머
	std::vector<std::shared_ptr<CGameObject>> m_pWinLetters;
	float m_fVictoryTimer = -1.0f;

	// 피격 시 화면 외곽 빨간 비네트
	float m_fHitFlash = 0.0f;
	static constexpr float kHitFlashDecayRate = 1.7f;
	std::shared_ptr<CShader>     m_pHitOverlayShader;
	std::shared_ptr<CGameObject> m_pHitOverlayQuad;
};
