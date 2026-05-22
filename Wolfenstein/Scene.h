#pragma once

#include "Timer.h"
#include "Shader.h"

#include <functional>

class CButtonObject;
class CCamera;
class CGameObject;

// ���� ���� ���¸� ��Ÿ���� �������̴�.
// LANDING: ���� ȭ��. MAP1/MAP2: �� ���� �ΰ��� �� (�䱸���� 2).
enum class SceneState {
	LANDING = 0,
	MAP_SELECT = 1,
	MAP1 = 2,
	MAP2 = 3,
	COUNT = 4
};

class CScene
{
public:
	CScene();
	~CScene();

	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	void ReleaseObjects();

	bool ProcessInput(UCHAR* pKeysBuffer);
	void AnimateObjects(float fTimeElapsed);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, class CCamera* pCamera);

	void ReleaseUploadBuffers();

	void HandleLeftClick(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight, const CCamera* pCamera);
	bool IsGameStartRequested() const { return m_bGameStartRequested; }
	void ClearGameStartRequest() { m_bGameStartRequested = false; }

	// �� ���� ȭ�鿡�� ���콺 ȣ���� ���� �̴Ͼ�ó �ε����� �����Ѵ�.
	void UpdateMapSelectHover(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight, const CCamera* pCamera);
	// Ŭ������ ���õ� �� �ε���(1=MAP1, 2=MAP2, 0=����)�� ��ȯ�ϰ� ���� ���¸� ����.
	int ConsumeSelectedMap();
	float GetMiniatureAngle() const { return m_fMiniatureAngle; }
	int GetHoveredMiniIndex() const { return m_nHoveredMiniIndex; }

	// �� ���� ����: ���� ���¸� ��ȸ�ϰų� �ٸ� ������ ��ȯ�Ѵ�.
	SceneState GetCurrentState() const { return m_eCurrentState; }
	void TransitionToScene(SceneState newState);

	// �׷��Ƚ� ��Ʈ �ñ׳�ó�� �����Ѵ�.
	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);
	ID3D12RootSignature* GetGraphicsRootSignature();

	// �÷��̾� �� ���� + TPS ���������� �׸���.
	// CGameFramework �� ���� ������ �� �� �� ������ �θ�, Render �� m_bPlayerVisible �� ����
	// �ΰ��� �� ���� ���Ŀ� �Բ� �׸���.
	void SetPlayerModel(std::shared_ptr<CGameObject> p) { m_pPlayerModel = std::move(p); }
	void SetPlayerVisible(bool b) { m_bPlayerVisible = b; }

	// Push a runtime object (typically a bullet) into the currently active
	// map's shader so it animates and renders alongside the static maze.
	// Returns false if the scene is not on a gameplay map.
	bool AddObjectToCurrentMap(std::shared_ptr<CGameObject> pObject);

	// 플레이어가 적 총알에 피격되었을 때 호출되는 콜백을 등록한다.
	// GameFramework 가 BuildObjects 단계에서 라이프 감소 람다를 등록한다.
	void SetOnPlayerHit(std::function<void()> fn) { m_fnOnPlayerHit = std::move(fn); }

	// 게임플레이 맵(MAP1/MAP2)의 동적 객체(Bullet/EnemyBullet/Enemy) 를 모두
	// 제거한다. LANDING 으로 되돌아갈 때 잔존 객체를 정리하기 위함이며,
	// 정적 미로 기하 자체는 보존한다.
	void ResetGameplayState();

	// 현재 활성 맵(MAP1/MAP2)에서 살아있는 적(EObjectTag::Enemy && IsAlive()) 의
	// 수를 반환한다. 게임플레이 맵이 아니면 0.
	int CountAliveEnemies() const;

	// 살아있는 적의 (월드 중심, AABB half) 쌍을 반환한다. 조준 광선이 적 AABB 와
	// 인터섹션해 LookAt 대상점을 정확히 결정하는 데 사용된다 (CGameFramework::GetAimTargetPoint).
	std::vector<std::pair<XMFLOAT3, XMFLOAT3>> GetAliveEnemyAABBs() const;

	// 현재 맵의 모든 적 CEnemyObject 에 마커 가시성을 일괄 설정한다.
	// 적 ≤3 마리일 때 GameFramework 가 true 로 호출해 머리 위 노란 기둥을 표시한다.
	void SetEnemyMarkersVisible(bool bVisible);

	std::shared_ptr<CButtonObject> m_pStartButton;
	bool m_bGameStartRequested = false;

protected:
	// ���� Ȱ��ȭ�� ���� �����̴� (�⺻���� LANDING).
	SceneState m_eCurrentState = SceneState::LANDING;

	// �� ���� ȭ�� ����: ȣ�� ���� �ε��� (-1=����, 0=����, 1=������), ȸ�� ��(rad), Ŭ�� ��û ��.
	int m_nHoveredMiniIndex = -1;
	float m_fMiniatureAngle = 0.0f;
	int m_nRequestedMap = 0;

	// ��Ʈ �ñ׳�ó�� ��Ÿ���� �������̽� �������̴�.
	// Root Signature - GPU ���������� �� �ܰ谡 � �ڿ��� � ���Կ� �Ѱܹ����� �����Ѵ�.
	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature;

	// �� ���¸��� �ϳ��� ���̴��� �ξ�, ���� ������ ���̴��� �������Ѵ�.
	// �ε����� SceneState ���Ű�(LANDING=0, MAP1=1, MAP2=2)�� �״�� ����Ѵ�.
	std::vector<CObjectsShader> m_vShaders;

	// TPS ����� �� �׷����� �÷��̾� �𵨰� �� ���ü� �÷���.
	std::shared_ptr<CGameObject> m_pPlayerModel;
	bool m_bPlayerVisible = false;

	// 적 총알이 플레이어에 적중했을 때 라이프를 깎기 위해 GameFramework 가
	// 등록한 콜백. 등록되지 않으면 피격 처리를 스킵한다.
	std::function<void()> m_fnOnPlayerHit;

	// ===== 퐁 디퓨즈 라이팅 =====
	// 단순 방향광 1개 + 환경광. 그림자/스페큘러 없음.
	// Render() 마다 한 번 b2 root constants 로 업로드된다.
	// 라이트 진행 방향: 정규화된 -Y 약간 비스듬 (위에서 비추는 햇빛).
	XMFLOAT3 m_xmf3LightDir   = { -0.4f, -1.0f, -0.3f };
	XMFLOAT3 m_xmf3LightColor = {  0.9f,  0.9f,  0.85f };
	XMFLOAT3 m_xmf3Ambient    = {  0.25f, 0.25f, 0.30f };
};
