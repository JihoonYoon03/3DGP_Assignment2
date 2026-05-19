#pragma once

#include "Timer.h"
#include "Shader.h"

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
};
