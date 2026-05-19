#pragma once

constexpr float ASPECT_RATIO = FRAME_BUFFER_WIDTH / FRAME_BUFFER_HEIGHT;

// Vertex Shader Const Buffer Info
struct VS_CB_CAMERA_INFO
{
	XMFLOAT4X4 m_xmf4x4View;
	XMFLOAT4X4 m_xmf4x4Projection;
};

// ī�޶� ���� ���. FPS = 1��Ī(�� ��ġ = �÷��̾� ��ġ), TPS = 3��Ī(�÷��̾� �ڷ� ���� ��ġ).
// ��ġ ����� CGameFramework::ProcessInput ���� ��忡 ���� �б��Ѵ�.
enum class ECameraMode { FPS, TPS };

class CCamera {
protected:
	// �� ���
	XMFLOAT4X4		m_xmf4x4View;
	// ���� ���
	XMFLOAT4X4		m_xmf4x4Projection;

	D3D12_VIEWPORT	m_d3dViewport;
	D3D12_RECT		m_d3dScissorRect;

	// ī�޶��� ��ġ�� ���յ� ���� ���͵�.
	XMFLOAT3	m_xmf3Position{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3	m_xmf3Right{ 1.0f, 0.0f, 0.0f };
	XMFLOAT3	m_xmf3Up{ 0.0f, 1.0f, 0.0f };
	XMFLOAT3	m_xmf3Look{ 0.0f, 0.0f, 1.0f };
	// Pitch / Yaw (����)
	float		m_fPitch = 0.0f;
	float		m_fYaw = 0.0f;

	// ���� ���� ���. �⺻�� FPS.
	ECameraMode	m_eMode = ECameraMode::FPS;

public:
	CCamera();
	virtual ~CCamera();

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseShaderVariables();
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	void GenerateViewMatrix(XMFLOAT3 xmf3Position, XMFLOAT3 xmf3LookAt, XMFLOAT3 xmf3Up);

	// 1��Ī �̵� / ȸ�� API.
	void Move(const XMFLOAT3& xmf3Shift);
	void SetPosition(const XMFLOAT3& xmf3Position);
	void Rotate(float fPitchDelta, float fYawDelta);
	void RegenerateViewMatrix();

	const XMFLOAT3& GetPosition() const { return m_xmf3Position; }
	const XMFLOAT3& GetLook() const { return m_xmf3Look; }
	const XMFLOAT3& GetRight() const { return m_xmf3Right; }
	// Returns the camera yaw used by the TPS path to keep the player model
	// rotation in sync with the view direction.
	float GetYaw() const { return m_fYaw; }
	void GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fAspectRatio, float fFOVAngle);

	void SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float fMinZ =	0.0f, float fMaxZ = 1.0f);
	void SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom);

	virtual void SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pd3dCommandList);

	const XMFLOAT4X4& GetViewMatrix() const { return m_xmf4x4View; }
	const XMFLOAT4X4& GetProjectionMatrix() const { return m_xmf4x4Projection; }

	// ���� ��� ��ۿ� ������.
	ECameraMode GetMode() const { return m_eMode; }
	void SetMode(ECameraMode eMode) { m_eMode = eMode; }
};
