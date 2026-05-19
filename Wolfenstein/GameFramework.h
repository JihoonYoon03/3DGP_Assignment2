#pragma once

#include "Scene.h"

#include "Timer.h"
#include "Maps.h"

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

	// ����/�߷� ����.
	// m_fVerticalVelocity: ���� �ӵ�(����/sec). 0���� ũ�� ��� ��.
	// m_bGrounded: ���� �ٴڿ� ��� �ִ��� ����.
	// m_bPrevSpacePressed: �����̽� Ű�� edge ����� ���� ������ ����.
	float m_fVerticalVelocity = 0.0f;
	bool m_bGrounded = true;
	bool m_bPrevSpacePressed = false;

	// === �÷��̾� ��ġ �� ���� ��� (�䱸 C) ===
	// m_xmf3PlayerPos: �÷��̾��� ���� �ִ� ��ġ(������ ����). ī�޶� TPS �� ��
	// �ڷ� ���� ��ġ�� �̵��ϴ���, �̵�/�浹/�߷��� �� ������ ����Ѵ�.
	// m_pPlayerModel: TPS ��忡���� �׷����� �÷��̾��� �ð� ��(���� ť��).
	// m_bPrevVPressed: 'V' Ű�� edge ������ OnProcessingKeyboardMessage WM_KEYUP ���� ó���ϹǷ� ���� �ʿ� ������,
	// ���� Ȯ���� ���� ����.
	XMFLOAT3 m_xmf3PlayerPos{ 0.0f, MAP_EYE_HEIGHT, 0.0f };
	std::shared_ptr<CGameObject> m_pPlayerModel;

	// === Shooting / crosshair ===
	// Shared mesh for every spawned bullet so the GPU side stays cheap.
	std::shared_ptr<CMesh> m_pBulletMesh;
	// Cooldown so a held click does not spew bullets every frame.
	float m_fFireCooldown = 0.0f;
	// 화면 정중앙에 회전 없이 고정되는 십자선(+) 조준점.
	// CCrosshairMesh(NDC 좌표) + CHudShader(월드/뷰/투영 무시) 조합으로 그려지므로
	// 매 프레임 위치/회전 갱신 없이 항상 화면 정가운데에 표시된다.
	std::shared_ptr<CGameObject> m_pCrosshair;

	// === 적 / 라이프 시스템 ===
	// m_pEnemyMesh: 모든 적이 공유하는 큐브 메시. 색은 어두운 보라 계열.
	// m_pEnemyBulletMesh: 적 총알 전용 메시. 플레이어 총알과 색을 구분 (붉은 계열).
	// m_pHudShader: 십자선과 라이프 바 칸이 공유하는 HUD 셰이더.
	// m_pLifeBarSegments: 라이프 칸 10개. m_nPlayerLife 개수만큼 앞에서부터 렌더.
	// m_nPlayerLife: 남은 라이프. 적 총알 1발에 1 감소. 0 이 되면 LANDING 으로 복귀.
	// m_bResetPending: 라이프 0 으로 죽었을 때 다음 프레임에 LANDING 으로 보낼 트리거.
	std::shared_ptr<CMesh>   m_pEnemyMesh;
	std::shared_ptr<CMesh>   m_pEnemyBulletMesh;
	std::shared_ptr<CShader> m_pHudShader;
	std::vector<std::shared_ptr<CGameObject>> m_pLifeBarSegments;
	int  m_nPlayerLife = 10;
	bool m_bResetPending = false;
};
