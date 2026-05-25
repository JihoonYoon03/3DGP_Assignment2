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

	// 프레임워크 초기화 함수이다 (창 생성 직후 한 번 호출)
	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);

	void OnDestroy();

	// 스왑 체인, 디바이스, 디스크립터 힙, 명령 큐/할당자/리스트를 생성하는 함수
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateDirect3DDevice();
	void CreateCommandQueueAndList();

	// 렌더 타겟 뷰와 깊이-스텐실 뷰를 생성하는 함수
	void CreateRenderTargetViews();
	void CreateDepthStencilView();

	// 렌더링할 메시와 게임 객체를 생성하고 소멸하는 함수
	void BuildObjects();
	void ReleaseObjects();

	// 프레임워크의 핵심(사용자 입력, 애니메이션, 렌더링)을 수행하는 함수
	void ProcessInput();
	void AnimateObjects();
	void FrameAdvance();

	// CPU와 GPU를 동기화하는 함수
	void WaitForGPUComplete();

	// 윈도우 메시지(키보드, 마우스 입력)를 처리하는 함수이다.
	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void ChangeSwapChainState();

	// 게임의 1인칭/3인칭 카메라를 해당 씬 상태에 맞게 초기화한다.
	void SetupGameCamera(SceneState state);
	// 맵 선택 화면을 위한 미니어처 시점 카메라 셋업.
	void SetupMapSelectCamera();

	// Spawn a bullet at the player's gun position aimed along the camera
	// look direction. Respects the per-frame cooldown so rapid clicks do
	// not flood the scene.
	void FireBullet();

	// 적이 호출하는 총알 발사. 좌표/방향은 적 AI 가 직접 결정한다.
	// EObjectTag::EnemyBullet 으로 태깅된 총알이 Scene 에 추가된다.
	void SpawnEnemyBullet(const XMFLOAT3& xmf3Origin, const XMFLOAT3& xmf3Dir);

	// 현재 맵에 적을 PickEnemySpawnPositions 결과만큼 스폰한다.
	void SpawnEnemiesForMap(SceneState state);

	// 매 프레임 소총의 월드 행렬을 카메라 모드(FPS/TPS)에 맞게 갱신한다.
	// FPS: 카메라 우측 하단(어깨총처럼 화면 한쪽에 보이도록).
	// TPS: 플레이어 모델 우측 어깨 옆, 조준선(aim) 방향 정렬.
	void UpdateRifleTransform();

	// 조준점(화면 중앙)에서 광선을 쏴 첫 충돌점(벽 또는 살아있는 적 AABB) 의
	// 월드 좌표를 반환한다. 충돌이 없으면 카메라 forward * kMaxAimDist 폴백.
	// UpdateRifleTransform 과 FireBullet 이 호출해, 총구가 조준점을 정확히 가리키도록 한다.
	XMFLOAT3 GetAimTargetPoint() const;

	void MoveToNextFrame();

	std::unique_ptr<class CCamera> m_pCamera;

private:
	// 아래 영역은 그래픽 화면 크기와 관련 있는 부분이다.
	// -------------------------------
	HINSTANCE	m_hInstance;
	HWND		m_hWnd;

	int			m_nWndClientWidth;
	int			m_nWndClientHeight;
	// -------------------------------

	// ComPtr - COM 객체들을 RAII에 맞게 알아서 해제해줌
	// Create~ 함수 등에 해당 객체의 포인터를 요구할 경우 .Get()
	// 반환값을 받아야 하는 경우 .GetAddressOf()를 사용할 것
	// 객체 초기화나 Release를 신경 쓸 필요 없음.

	// DXGI 팩토리 인터페이스를 위한 포인터
	// DXGI Factory - 시스템 그래픽 환경을 알기 위해 필요
	// GPU 혹은 다른 그래픽 어댑터 탐색(IDXGIAdapter 탐색), 스왑체인 생성, 풀스크린 전환 관리
	// Adapter와 D3D12Device 연결
	ComPtr<IDXGIFactory4>	m_pdxgiFactory;

	// 스왑 체인 인터페이스를 위한 포인터. 주로 디스플레이를 관리하기 위해 필요
	// SwapChain - 2개 이상의 렌더 스트림을 교체하여 화면 티어링 방지 등에 사용
	ComPtr<IDXGISwapChain3>	m_pdxgiSwapChain;

	// Direct3D 디바이스 인터페이스를 위한 포인터이다. 주로 리소스를 생성하기 위하여 필요하다.
	ComPtr<ID3D12Device>		m_pd3dDevice;

	// MSAA 다중 샘플링을 활성화하고 다중 샘플링 수준을 설정한다.
	bool m_bMsaa4xEnable = false;
	UINT m_nMsaa4xQualityLevels = 0;

	// 스왑 체인의 후면 버퍼의 개수이다.
	static const UINT	m_nSwapChainBuffers = 2;
	// 현재 스왑 체인의 후면 버퍼 인덱스이다.
	UINT				m_nSwapChainBufferIndex;

	// Render Target View, DescHeap 인터페이스 포인터, RTV 디스크립터 증가 크기이다.
	ComPtr<ID3D12Resource>			m_ppd3dSwapChainBackBuffers[m_nSwapChainBuffers];
	ComPtr<ID3D12DescriptorHeap>	m_pd3dRtvDescriptorHeap;
	UINT							m_nRtvDescriptorIncrementSize;

	// Depth Stencil View, 디스크립터 힙 인터페이스 포인터, DSV 디스크립터 크기이다.
	ComPtr<ID3D12Resource>			m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap>	m_pd3dDsvDescriptorHeap;
	UINT							m_nDsvDescriptorIncrementSize;

	// 명령 큐, 명령 할당자, 명령 리스트 인터페이스 포인터이다.
	ComPtr<ID3D12CommandQueue>			m_pd3dCommandQueue;
	ComPtr<ID3D12CommandAllocator>		m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList>	m_pd3dCommandList;

	// 그래픽스 파이프라인의 상태 객체에 대한 인터페이스 포인터이다.
	ComPtr<ID3D12PipelineState>			m_pd3dPipelineState;

	// 펜스 인터페이스 포인터, 펜스의 값, 이벤트 핸들이다.
	ComPtr<ID3D12Fence>	m_pd3dFence;
	UINT64				m_nFenceValue;
	HANDLE				m_hFenceEvent;

	// 게임의 진행 시간을 관리하는 타이머이다.
	CGameTimer m_GameTimer;

	// 프레임 레이트를 측정하기 위해 사용한다.
	_TCHAR m_pszFrameRate[50];

	// 후면버퍼마다 별도 펜스 값을 관리하기 위한 멤버 변수.
	UINT64 m_nFenceValues[m_nSwapChainBuffers];

	// 현재 활성 게임 씬
	std::unique_ptr<CScene> m_pScene;

	// 1인칭 마우스 룩 관련 상태.
	// m_bMouseCaptured: 마우스를 화면 중앙으로 캡처 중인지 여부.
	// m_ptWndCenterScreen: 윈도우 중앙의 화면 좌표 (delta 계산).
	bool m_bMouseCaptured = false;
	POINT m_ptWndCenterScreen{ 0, 0 };

	// 점프/중력 입력 상태.
	// m_bPrevSpacePressed: 스페이스 키 edge 검출용 이전 프레임 상태.
	// 점프 속도/접지 여부는 m_pPlayer (CPlayer/CCharacter) 가 보유.
	bool m_bPrevSpacePressed = false;

	// === 플레이어 객체 ===
	// CPlayer 인스턴스. 위치/HP/소총/점프/반동 등 플레이어 상태를 모두 보유.
	// CCharacter 를 상속하므로 적(CEnemyObject) 과 동일한 인터페이스(HP, 점프
	// 물리, TryMoveXZ) 를 공유한다. 입력/카메라/HUD 등 외부 시스템 연결만
	// CGameFramework 가 담당.
	// m_pPlayerModel: TPS 모드에서 그려지는 플레이어의 시각 모델(보라 큐브).
	std::shared_ptr<CPlayer> m_pPlayer;
	std::shared_ptr<CGameObject> m_pPlayerModel;

	// === Shooting / crosshair ===
	// Shared mesh for every spawned bullet so the GPU side stays cheap.
	std::shared_ptr<CMesh> m_pBulletMesh;
	// Cooldown so a held click does not spew bullets every frame.
	float m_fFireCooldown = 0.0f;
	// 화면 정중앙에 회전 없이 고정되는 십자선(+) 조준점.
	std::shared_ptr<CGameObject> m_pCrosshair;

	// === 적 / 라이프 UI ===
	// m_pEnemyMesh: 모든 적이 공유하는 큐브 메시. 색은 어두운 보라 계열.
	// m_pEnemyBulletMesh: 적 총알 전용 메시.
	// m_pHudShader: 십자선과 라이프 바 칸이 공유하는 HUD 셰이더.
	// m_pLifeBarSegments: 라이프 칸 10개. m_pPlayer->GetHP() 개수만큼 앞에서부터 렌더.
	// m_bResetPending: 라이프 0 으로 죽었을 때 다음 프레임에 LANDING 으로 보낼 트리거.
	std::shared_ptr<CMesh>   m_pEnemyMesh;
	std::shared_ptr<CMesh>   m_pEnemyBulletMesh;
	std::shared_ptr<CShader> m_pHudShader;
	std::vector<std::shared_ptr<CGameObject>> m_pLifeBarSegments;
	bool m_bResetPending = false;

	// [Claude] 플레이어 사망 시퀀스.
	// m_fPlayerDeathTimer 는 -1.0f 일 때 비활성. HP 가 0 으로 떨어지면
	// kPlayerDeathDuration 으로 설정되어 매 프레임 감산되고, 그동안 입력은
	// 차단되고 카메라가 앞으로 천천히 기울며 화면이 붉게 유지된다. 0 도달 시
	// m_bResetPending 을 true 로 세팅하여 기존 reset 로직으로 LANDING 복귀.
	float m_fPlayerDeathTimer = -1.0f;
	static constexpr float kPlayerDeathDuration  = 1.5f;
	static constexpr float kPlayerDeathPitchRate = -0.7f; // rad/s (~-40°/s → 1.5초간 약 -60°)

	// === 소총 메시 ===
	// 플레이어 소총 객체 자체는 m_pPlayer->GetRifle() 로 접근.
	// 적 소총은 적 1체당 1개의 CGameObject 가 이 메시를 공유 (CEnemyObject::m_pRifle).
	std::shared_ptr<CMesh>      m_pRifleMesh;
	std::shared_ptr<CMesh>      m_pEnemyRifleMesh;

	// 반동 애니메이션 지속 시간 (CPlayer::TickRecoil 에 전달).
	// 키프레임:  t=0.00  offset= 0.00
	//            t=0.06  offset=-0.30  (최대 뒤로 후진)
	//            t=0.14  offset=-0.10  (절반 복귀)
	//            t=0.25  offset= 0.00  (원위치)
	static constexpr float kRecoilDuration = 0.25f;

	// === 적 잔여 수 점 카운트(좌상단) ===
	// SpawnEnemiesForMap 이 만드는 최대 적 수(12) 만큼 점을 미리 생성하고
	// 살아있는 적 수만큼만 앞에서부터 그린다. 라이프 바와 동일한 패턴.
	std::vector<std::shared_ptr<CGameObject>> m_pCountPips;

	// === 적 마커 기둥 ===
	// 잔여 적 수 ≤ 3 일 때 활성화되어 적 머리 위에 따라다니는 노란 기둥.
	// 메시는 GameFramework 가 소유하고 SpawnEnemiesForMap 에서 각 적에 주입한다.
	std::shared_ptr<CMesh> m_pEnemyMarkerMesh;

	// === 승리 처리 ===
	// 모든 적 처치 시 화면 중앙에 "WIN" 글자(여러 CHudQuadMesh 세그먼트) 표시.
	// m_fVictoryTimer: ≥0 카운트다운 중. 0 도달 시 m_bResetPending 으로 LANDING 복귀.
	// -1.0f 는 비활성 상태.
	std::vector<std::shared_ptr<CGameObject>> m_pWinLetters;
	float m_fVictoryTimer = -1.0f;

	// === 피격 비네트 오버레이 ===
	// 적 총알 명중 시 화면 외곽이 잠시 빨갛게 깜빡인다.
	// m_fHitFlash: 피격 시 1.0 으로 설정되고 매 프레임 kHitFlashDecayRate 비율로 감쇠.
	// m_pHitOverlayShader: NDC 풀스크린 + alpha blend PSO. 한 번만 생성.
	// m_pHitOverlayQuad: NDC (-1,1)~(1,-1) 풀스크린 쿼드 GameObject. m_fHitFlash > 0 일 때만 렌더.
	float m_fHitFlash = 0.0f;
	static constexpr float kHitFlashDecayRate = 1.7f; // 1.0 → 0 까지 약 0.6초
	std::shared_ptr<CShader>     m_pHitOverlayShader;
	std::shared_ptr<CGameObject> m_pHitOverlayQuad;
};
