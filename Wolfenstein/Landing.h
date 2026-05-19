#pragma once

#include "Mesh.h"
#include "GameObject.h"

namespace LandingParams {
	// Camera configuration for landing screen
	constexpr XMFLOAT3 CAMERA_POSITION{ 0.0f, 5.0f, -25.0f };
	constexpr XMFLOAT3 CAMERA_LOOK_AT{ 0.0f, 0.0f, 0.0f };
	constexpr XMFLOAT3 CAMERA_UP{ 0.0f, 1.0f, 0.0f };
	constexpr float CAMERA_FOV_Y_DEG = 45.0f;

	// Title ("THE MAZE") configuration
	constexpr XMFLOAT3 TITLE_ANCHOR_POSITION{ -50.0f, 20.0f, 0.0f };
	constexpr float TITLE_PIXEL_SIZE = 1.2f;
	constexpr float TITLE_LETTER_PITCH_PIXELS = 6.5f;
	constexpr XMFLOAT4 TITLE_COLOR{ 1.0f, 1.0f, 1.0f, 1.0f };

	// Title wave animation
	constexpr float TITLE_WAVE_AMPLITUDE = 1.0f;
	constexpr float TITLE_WAVE_FREQUENCY_HZ = 2.0f;
	constexpr float TITLE_WAVE_PHASE_OFFSET_PER_LETTER = 0.35f;

	// Button ("GAME START") configuration
	constexpr XMFLOAT3 BUTTON_ANCHOR_POSITION{ 12.0f, -15.0f, 0.0f };
	constexpr float BUTTON_PIXEL_SIZE = 0.5f;
	constexpr float BUTTON_LETTER_PITCH_PIXELS = 6.0f;
	constexpr XMFLOAT4 BUTTON_COLOR{ 0.2f, 1.0f, 0.2f, 1.0f };

	// Button hit-test configuration
	constexpr float BUTTON_HITBOX_PADDING_X = 0.5f;
	constexpr float BUTTON_HITBOX_PADDING_Y = 0.5f;
};

class CTextLetterMesh : public CMesh {
public:
	CTextLetterMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		const std::vector<std::vector<bool>>& vPattern, float fPixelSize, XMFLOAT4 xmf4Color);
	virtual ~CTextLetterMesh();
};

class CLetterObject : public CGameObject {
public:
	CLetterObject();
	virtual ~CLetterObject();

	void SetBasePosition(XMFLOAT3 xmf3Position);
	void SetWaveAnimation(float fAmplitude, float fFrequency, float fPhaseOffset);
	virtual void Animate(float fTimeElapsed);

private:
	float m_fWaveAmplitude = 0.0f;
	float m_fWaveFrequency = 0.0f;
	float m_fWavePhaseOffset = 0.0f;
	float m_fAccumulatedTime = 0.0f;
	XMFLOAT3 m_xmf3BasePosition{ 0.0f, 0.0f, 0.0f };
	bool m_bAnimating = false;
};

class CTextStringObject : public CGameObject {
public:
	CTextStringObject();
	virtual ~CTextStringObject();

	void Build(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		const std::string& strText, XMFLOAT3 xmf3Position, float fPixelSize,
		float fLetterPitch, XMFLOAT4 xmf4Color, bool bAnimateWithWave = false,
		float fWaveAmplitude = 0.0f, float fWaveFrequency = 0.0f, float fWavePhaseOffset = 0.0f);

	virtual void Animate(float fTimeElapsed);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

	const XMFLOAT3& GetBoundingBoxMin() const { return m_xmf3BoundingMin; }
	const XMFLOAT3& GetBoundingBoxMax() const { return m_xmf3BoundingMax; }

protected:
	std::vector<std::shared_ptr<CLetterObject>> m_vLetters;
	XMFLOAT3 m_xmf3BoundingMin{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 m_xmf3BoundingMax{ 0.0f, 0.0f, 0.0f };
	// ���� �޽� �� �ȼ��� ���� ���� ũ��. �ٿ�� �ڽ� ������ ����Ѵ�(unit/pixel).
	float m_fPixelSize = 0.0f;

	void UpdateBoundingBox();
};

class CButtonObject : public CTextStringObject {
public:
	CButtonObject();
	virtual ~CButtonObject();

	bool HitTest(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight,
		const XMFLOAT4X4& xmf4x4View, const XMFLOAT4X4& xmf4x4Projection);

	void SetPressed(bool bPressed) { m_bPressed = bPressed; }
	bool IsPressed() const { return m_bPressed; }

private:
	bool m_bPressed = false;

	XMFLOAT3 UnprojectScreenToWorld(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight,
		const XMFLOAT4X4& xmf4x4View, const XMFLOAT4X4& xmf4x4Projection);
};

void BuildLandingObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects, std::shared_ptr<CButtonObject>& pStartButton);

const std::vector<std::vector<bool>>& GetLetterPattern(char cLetter);
