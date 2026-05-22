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

	// �����ӿ�ũ �ʱ�ȭ �Լ��̴� (�� ������ ���� �� ȣ��)
	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);

	void OnDestroy();

	// ���� ü��, ����̽�, ������ ��, ���� ť/�Ҵ���/����Ʈ�� �����ϴ� �Լ�
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateDirect3DDevice();
	void CreateCommandQueueAndList();

	// ���� Ÿ�� ��� ����-���ٽ� �並 �����ϴ� �Լ�
	void CreateRenderTargetViews();
	void CreateDepthStencilView();

	// �������� �޽��� ���� ��ü�� �����ϰ� �Ҹ��ϴ� �Լ�
	void BuildObjects();
	void ReleaseObjects();

	// �����ӿ�ũ�� �ٽ�(����� �Է�, �ִϸ��̼�, ������)�� �����ϴ� �Լ�
	void ProcessInput();
	void AnimateObjects();
	void FrameAdvance();

	// CPU�� GPU�� ����ȭ�ϴ� �Լ�
	void WaitForGPUComplete();

	// �������� �޽���(Ű����, ���콺 �Է�)�� ó���ϴ� �Լ��̴�.
	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void ChangeSwapChainState();

	// �ΰ��� 1��Ī/3��Ī ī�޶� �ش� �� ���¿� �°� �ʱ�ȭ�Ѵ�.
	void SetupGameCamera(SceneState state);
	// �� ���� ȭ���� ���� �̴Ͼ�ó �� ī�޶� �¾�.
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
	// �Ʒ� ������ �׷��� ȭ�� ũ��� ���� �ִ� �κ��̴�.
	// -------------------------------
	HINSTANCE	m_hInstance;
	HWND		m_hWnd;

	int			m_nWndClientWidth;
	int			m_nWndClientHeight;
	// -------------------------------

	// ComPtr - COM ��ü���� RAII�� �°� �˾Ƽ� ��������
	// Create~ �Լ� ��� �ش� ��ü�� �����͸� �䱸�� ��� .Get()
	// ��ȯ���� �޾ƾ� �ϴ� ��� .GetAddressOf()�� ������ ��
	// ��ü �ʱ�ȭ�� Release�� �Ű� �� �ʿ� ����.

	// DXGI ���丮 �������̽��� ���� ������
	// DXGI Factory - �ý��� �׷��� ȯ���� ���� �� ����
	// GPUȤ�� ���� �׷��� Ž��(IDXGIAdapter Ž��), ����ü�� ����, Ǯ��ũ�� ��ȯ ����
	// Adapter�� D3D12Device ����
	ComPtr<IDXGIFactory4>	m_pdxgiFactory;

	// ���� ü�� �������̽��� ���� ������. �ַ� ���÷��̸� �����ϱ� ���� �ʿ�
	// SwapChain - 2�� �̻��� ���� ��Ʈ�� ��ü�Ͽ� ȭ�� Ƽ� ���� ���� ��
	ComPtr<IDXGISwapChain3>	m_pdxgiSwapChain;

	// Direct3D ����̽� �������̽��� ���� �������̴�. �ַ� ���ҽ��� �����ϱ� ���Ͽ� �ʿ��ϴ�.
	ComPtr<ID3D12Device>		m_pd3dDevice;

	// MSAA ���� ���ø��� Ȱ��ȭ�ϰ� ���� ���ø� ������ �����Ѵ�.
	bool m_bMsaa4xEnable = false;
	UINT m_nMsaa4xQualityLevels = 0;

	// ���� ü���� �ĸ� ������ �����̴�.
	static const UINT	m_nSwapChainBuffers = 2;
	// ���� ���� ü���� �ĸ� ���� �ε����̴�.
	UINT				m_nSwapChainBufferIndex;

	// Render Target View, DescHeap �������̽� ������, RTV ������ ������ ũ���̴�.
	ComPtr<ID3D12Resource>			m_ppd3dSwapChainBackBuffers[m_nSwapChainBuffers];
	ComPtr<ID3D12DescriptorHeap>	m_pd3dRtvDescriptorHeap;
	UINT							m_nRtvDescriptorIncrementSize;

	// Depth Stencil View, ������ �� �������̽� ������, DSV ������ ũ���̴�.
	ComPtr<ID3D12Resource>			m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap>	m_pd3dDsvDescriptorHeap;
	UINT							m_nDsvDescriptorIncrementSize;

	// ���� ť, ���� �Ҵ���, ���� ����Ʈ �������̽� �������̴�.
	ComPtr<ID3D12CommandQueue>			m_pd3dCommandQueue;
	ComPtr<ID3D12CommandAllocator>		m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList>	m_pd3dCommandList;

	// �׷��Ƚ� ���������� ���� ��ü�� ���� �������̽� �������̴�.
	ComPtr<ID3D12PipelineState>			m_pd3dPipelineState;

	// �潺 �������̽� ������, �潺�� ��, �̺�Ʈ �ڵ��̴�.
	ComPtr<ID3D12Fence>	m_pd3dFence;
	UINT64				m_nFenceValue;
	HANDLE				m_hFenceEvent;

	// ������ ���� �����ӿ�ũ���� ����� Ÿ�̸��̴�.
	CGameTimer m_GameTimer;

	// ������ ������ ����Ʈ�� �� �������� ĸ�ǿ� ����ϱ� ���� ���ڿ��̴�.
	_TCHAR m_pszFrameRate[50];

	// �ĸ���۸��� ���� �潺 ���� �����ϱ� ���� ��� ����.
	UINT64 m_nFenceValues[m_nSwapChainBuffers];

	// ���� ���� ��� ����
	std::unique_ptr<CScene> m_pScene;

	// 1��Ī ���콺 �� ���� ����.
	// m_bMouseCaptured: ���콺�� ������ �߾����� ĸó ������ ����.
	// m_ptWndCenterScreen: ������ �߾��� ȭ�� ��ǥ (delta ����).
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
	// SpawnEnemiesForMap 이 만드는 최대 적 수(10) 만큼 점을 미리 생성하고
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
};
