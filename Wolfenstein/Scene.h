#pragma once

#include "Timer.h"
#include "Shader.h"

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

	// 그래픽 루트 시그너쳐를 생성한다.
	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);
	ID3D12RootSignature* GetGraphicsRootSignature();

protected:
	// 루트 시그너쳐를 나타내는 인터페이스 포인터이다. 
	// Root Signature - GPU 파이프라인과 데이터 사이의 통로, 계약서
	// 셰이더 실행 시 어떤 종류의 데이터를 어떤 슬롯에 넘겨받을 것인지 정의.
	// GPU가 읽을 데이터의 목차임.
	// Root Parameter - DescriptorTable(DescHeap의 집합), Rood Descriptor(CBV), Root Constant(상수)
	// DescTable - 셰이더가 DescHeap에서의 어디서부터 어디까지 읽어들일지 범위 지정
	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature;

	// Batch 처리를 하기 위해 씬을 셰이더들의 리스트로 표현한다
	std::vector<CObjectsShader> m_vShaders;
};