#pragma once

constexpr float ASPECT_RATIO = FRAME_BUFFER_WIDTH / FRAME_BUFFER_HEIGHT;

// Vertex Shader Const Buffer Info
struct VS_CB_CAMERA_INFO
{
	XMFLOAT4X4 m_xmf4x4View;
	XMFLOAT4X4 m_xmf4x4Projection;
};

class CCamera {
protected:
	// 카메라 변환 행렬
	XMFLOAT4X4		m_xmf4x4View;
	// 투영 변환 행렬
	XMFLOAT4X4		m_xmf4x4Projection;

	D3D12_VIEWPORT	m_d3dViewport;
	D3D12_RECT		m_d3dScissorRect;

public:
	CCamera();
	virtual ~CCamera();

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseShaderVariables();
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	void GenerateViewMatrix(XMFLOAT3 xmf3Position, XMFLOAT3 xmf3LookAt, XMFLOAT3 xmf3Up);
	void GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fAspectRatio, float fFOVAngle);
	
	void SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float fMinZ =	0.0f, float fMaxZ = 1.0f);
	void SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom);
	
	virtual void SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pd3dCommandList);
};

