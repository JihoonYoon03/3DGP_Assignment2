#pragma once

#include "Timer.h"
#include "Shader.h"

class CButtonObject;
class CCamera;

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

	// �׷��� ��Ʈ �ñ׳��ĸ� �����Ѵ�.
	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);
	ID3D12RootSignature* GetGraphicsRootSignature();



	std::shared_ptr<CButtonObject> m_pStartButton;
	bool m_bGameStartRequested = false;

protected:
	// ��Ʈ �ñ׳��ĸ� ��Ÿ���� �������̽� �������̴�. 
	// Root Signature - GPU ���������ΰ� ������ ������ ���? ���?
	// ���̴� ���� �� � ������ �����͸� � ���Կ� �Ѱܹ��� ������ ����.
	// GPU�� ���� �������� ������.
	// Root Parameter - DescriptorTable(DescHeap�� ����), Rood Descriptor(CBV), Root Constant(���?
	// DescTable - ���̴��� DescHeap������ ���?���?������ �о������?���� ����
	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature;

	// Batch ó���� �ϱ� ���� ���� ���̴����� ����Ʈ�� ǥ���Ѵ�
	std::vector<CObjectsShader> m_vShaders;
};