#pragma once

#include "Timer.h"
#include "Shader.h"

#include <functional>

class CButtonObject;
class CCamera;
class CGameObject;

// 현재 무슨 씬인지 나타냄
enum class SceneState {
	LANDING,
	MAP_SELECT,
	MAP1,
	MAP2,
	COUNT
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

	// 맵 선택 화면 갱신 및 선택
	void UpdateMapSelectHover(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight, const CCamera* pCamera);
	int ConsumeSelectedMap();
	float GetMiniatureAngle() const { return m_fMiniatureAngle; }
	int GetHoveredMiniIndex() const { return m_nHoveredMiniIndex; }

	// 씬 상태 관리
	SceneState GetCurrentState() const { return m_eCurrentState; }
	void TransitionToScene(SceneState newState);

	// 그래픽스 루트 시그니처 생성/Get
	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);
	ID3D12RootSignature* GetGraphicsRootSignature();

	void SetPlayerModel(std::shared_ptr<CGameObject> p) { m_pPlayerModel = std::move(p); }
	void SetPlayerVisible(bool b) { m_bPlayerVisible = b; }

	bool AddObjectToCurrentMap(std::shared_ptr<CGameObject> pObject);

	// 플레이어 피격 콜백 등록
	void SetOnPlayerHit(std::function<void()> fn) { m_fnOnPlayerHit = std::move(fn); }

	void ResetGameplayState();

	int CountAliveEnemies() const;
	std::vector<std::pair<XMFLOAT3, XMFLOAT3>> GetAliveEnemyAABBs() const;
	void SetEnemyMarkersVisible(bool bVisible);

	std::shared_ptr<CButtonObject> m_pStartButton;
	bool m_bGameStartRequested = false;

protected:
	// 현재 활성 씬 상태
	SceneState m_eCurrentState = SceneState::LANDING;
	
	int m_nHoveredMiniIndex = -1;
	float m_fMiniatureAngle = 0.0f;
	int m_nRequestedMap = 0;

	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature;

	// 각 씬마다 별도의 셰이더 (SceneState 값을 인덱스로 사용)
	std::vector<CObjectsShader> m_vShaders;

	std::shared_ptr<CGameObject> m_pPlayerModel;
	bool m_bPlayerVisible = false;

	// 플레이어 피격 콜백
	std::function<void()> m_fnOnPlayerHit;

	// 빛 관련
	XMFLOAT3 m_xmf3LightDir   = { -0.4f, -1.0f, -0.3f };
	XMFLOAT3 m_xmf3LightColor = {  0.9f,  0.9f,  0.85f };
	XMFLOAT3 m_xmf3Ambient    = {  0.25f, 0.25f, 0.30f };
};
