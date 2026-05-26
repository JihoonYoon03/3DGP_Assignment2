#pragma once

#include "Timer.h"
#include "Shader.h"

#include <functional>

class CButtonObject;
class CCamera;
class CGameObject;

// 현재 씬의 상태를 나타내는 열거형이다.
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

	// 맵 선택 화면 호버 갱신 및 선택된 맵 소비
	void UpdateMapSelectHover(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight, const CCamera* pCamera);
	int ConsumeSelectedMap();
	float GetMiniatureAngle() const { return m_fMiniatureAngle; }
	int GetHoveredMiniIndex() const { return m_nHoveredMiniIndex; }

	// 씬 상태 관리
	SceneState GetCurrentState() const { return m_eCurrentState; }
	void TransitionToScene(SceneState newState);

	// 그래픽스 루트 시그니처 생성/조회
	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);
	ID3D12RootSignature* GetGraphicsRootSignature();

	// TPS 모드에서 그릴 플레이어 모델 주입과 가시성
	void SetPlayerModel(std::shared_ptr<CGameObject> p) { m_pPlayerModel = std::move(p); }
	void SetPlayerVisible(bool b) { m_bPlayerVisible = b; }

	// 동적 객체(주로 총알)를 현재 활성 맵에 추가한다
	bool AddObjectToCurrentMap(std::shared_ptr<CGameObject> pObject);

	// 플레이어 피격 콜백 등록
	void SetOnPlayerHit(std::function<void()> fn) { m_fnOnPlayerHit = std::move(fn); }

	// 게임플레이 맵의 동적 객체를 모두 제거 (정적 미로는 보존)
	void ResetGameplayState();

	// 살아있는 적의 수
	int CountAliveEnemies() const;

	// 살아있는 적의 (월드 중심, AABB half) 쌍 목록
	std::vector<std::pair<XMFLOAT3, XMFLOAT3>> GetAliveEnemyAABBs() const;

	// 모든 적의 머리 위 마커 가시성 일괄 설정
	void SetEnemyMarkersVisible(bool bVisible);

	std::shared_ptr<CButtonObject> m_pStartButton;
	bool m_bGameStartRequested = false;

protected:
	// 현재 활성 씬 상태
	SceneState m_eCurrentState = SceneState::LANDING;

	// 맵 선택 화면 호버 상태
	int m_nHoveredMiniIndex = -1;
	float m_fMiniatureAngle = 0.0f;
	int m_nRequestedMap = 0;

	// 루트 시그니처
	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature;

	// 각 씬마다 별도의 셰이더 (SceneState 값을 인덱스로 사용)
	std::vector<CObjectsShader> m_vShaders;

	// TPS 모드에서 그릴 플레이어 모델과 가시성 플래그
	std::shared_ptr<CGameObject> m_pPlayerModel;
	bool m_bPlayerVisible = false;

	// 플레이어 피격 콜백
	std::function<void()> m_fnOnPlayerHit;

	// 디퓨즈 라이팅: 방향광 1개 + 환경광
	XMFLOAT3 m_xmf3LightDir   = { -0.4f, -1.0f, -0.3f };
	XMFLOAT3 m_xmf3LightColor = {  0.9f,  0.9f,  0.85f };
	XMFLOAT3 m_xmf3Ambient    = {  0.25f, 0.25f, 0.30f };
};
