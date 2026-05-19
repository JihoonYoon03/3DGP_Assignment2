#pragma once

#include "Timer.h"
#include "Shader.h"

class CButtonObject;
class CCamera;
class CGameObject;

// 씬의 현재 상태를 나타내는 열거형이다.
// LANDING: 시작 화면. MAP1/MAP2: 두 가지 인게임 맵 (요구사항 2).
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

	// 맵 선택 화면에서 마우스 호버를 보고 미니어처 인덱스를 갱신한다.
	void UpdateMapSelectHover(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight, const CCamera* pCamera);
	// 클릭으로 선택된 맵 인덱스(1=MAP1, 2=MAP2, 0=없음)를 반환하고 내부 상태를 비운다.
	int ConsumeSelectedMap();
	float GetMiniatureAngle() const { return m_fMiniatureAngle; }
	int GetHoveredMiniIndex() const { return m_nHoveredMiniIndex; }

	// 씬 상태 관리: 현재 상태를 조회하거나 다른 씬으로 전환한다.
	SceneState GetCurrentState() const { return m_eCurrentState; }
	void TransitionToScene(SceneState newState);

	// 그래픽스 루트 시그너처를 생성한다.
	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);
	ID3D12RootSignature* GetGraphicsRootSignature();

	// 플레이어 모델 주입 + TPS 시점에서만 그리기.
	// CGameFramework 가 모델을 생성한 뒤 한 번 주입해 두면, Render 가 m_bPlayerVisible 에 따라
	// 인게임 맵 렌더 직후에 함께 그린다.
	void SetPlayerModel(std::shared_ptr<CGameObject> p) { m_pPlayerModel = std::move(p); }
	void SetPlayerVisible(bool b) { m_bPlayerVisible = b; }

	std::shared_ptr<CButtonObject> m_pStartButton;
	bool m_bGameStartRequested = false;

protected:
	// 현재 활성화된 씬의 상태이다 (기본값은 LANDING).
	SceneState m_eCurrentState = SceneState::LANDING;

	// 맵 선택 화면 상태: 호버 중인 인덱스 (-1=없음, 0=왼쪽, 1=오른쪽), 회전 각(rad), 클릭 요청 맵.
	int m_nHoveredMiniIndex = -1;
	float m_fMiniatureAngle = 0.0f;
	int m_nRequestedMap = 0;

	// 루트 시그너처를 나타내는 인터페이스 포인터이다.
	// Root Signature - GPU 파이프라인 각 단계가 어떤 자원을 어떤 슬롯에 넘겨받을지 정의한다.
	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature;

	// 씬 상태마다 하나의 셰이더를 두어, 현재 상태의 셰이더만 렌더링한다.
	// 인덱스는 SceneState 열거값(LANDING=0, MAP1=1, MAP2=2)을 그대로 사용한다.
	std::vector<CObjectsShader> m_vShaders;

	// TPS 모드일 때 그려지는 플레이어 모델과 그 가시성 플래그.
	std::shared_ptr<CGameObject> m_pPlayerModel;
	bool m_bPlayerVisible = false;
};
