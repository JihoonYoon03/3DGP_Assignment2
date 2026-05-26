#include "stdafx.h"
#include "Landing.h"
#include "Camera.h"
#include <algorithm>

constexpr int LETTER_GRID_COLS = 5;
constexpr int LETTER_GRID_ROWS = 7;

struct LetterPattern {
	char cLetter;
	std::vector<std::vector<bool>> pattern;
};

class LetterPatternTable {
public:
	static LetterPatternTable& GetInstance() {
		static LetterPatternTable instance;
		return instance;
	}

	const std::vector<std::vector<bool>>& GetPattern(char c) {
		if (m_patterns.empty()) InitializePatterns();
		for (const auto& p : m_patterns) {
			if (p.cLetter == c) return p.pattern;
		}
		return m_emptyPattern;
	}

private:
	std::vector<LetterPattern> m_patterns;
	std::vector<std::vector<bool>> m_emptyPattern;

	void InitializePatterns() {
		m_emptyPattern.resize(LETTER_GRID_ROWS, std::vector<bool>(LETTER_GRID_COLS, false));

		LetterPattern T_pat;
		T_pat.cLetter = 'T';
		T_pat.pattern = {
			{true, true, true, true, true},
			{false, false, true, false, false},
			{false, false, true, false, false},
			{false, false, true, false, false},
			{false, false, true, false, false},
			{false, false, true, false, false},
			{false, false, true, false, false}
		};
		m_patterns.push_back(T_pat);

		LetterPattern H_pat;
		H_pat.cLetter = 'H';
		H_pat.pattern = {
			{true, false, false, false, true},
			{true, false, false, false, true},
			{true, true, true, true, true},
			{true, false, false, false, true},
			{true, false, false, false, true},
			{true, false, false, false, true},
			{true, false, false, false, true}
		};
		m_patterns.push_back(H_pat);

		LetterPattern E_pat;
		E_pat.cLetter = 'E';
		E_pat.pattern = {
			{true, true, true, true, true},
			{true, false, false, false, false},
			{true, true, true, false, false},
			{true, false, false, false, false},
			{true, false, false, false, false},
			{true, false, false, false, false},
			{true, true, true, true, true}
		};
		m_patterns.push_back(E_pat);

		LetterPattern M_pat;
		M_pat.cLetter = 'M';
		M_pat.pattern = {
			{true, false, false, false, true},
			{true, true, false, true, true},
			{true, false, true, false, true},
			{true, false, true, false, true},
			{true, false, false, false, true},
			{true, false, false, false, true},
			{true, false, false, false, true}
		};
		m_patterns.push_back(M_pat);

		LetterPattern A_pat;
		A_pat.cLetter = 'A';
		A_pat.pattern = {
			{false, false, true, false, false},
			{false, true, false, true, false},
			{true, false, false, false, true},
			{true, true, true, true, true},
			{true, false, false, false, true},
			{true, false, false, false, true},
			{true, false, false, false, true}
		};
		m_patterns.push_back(A_pat);

		LetterPattern Z_pat;
		Z_pat.cLetter = 'Z';
		Z_pat.pattern = {
			{true, true, true, true, true},
			{false, false, false, true, false},
			{false, false, true, false, false},
			{false, true, false, false, false},
			{true, false, false, false, false},
			{true, false, false, false, false},
			{true, true, true, true, true}
		};
		m_patterns.push_back(Z_pat);

		LetterPattern G_pat;
		G_pat.cLetter = 'G';
		G_pat.pattern = {
			{false, true, true, true, true},
			{true, false, false, false, false},
			{true, false, false, false, false},
			{true, false, true, true, true},
			{true, false, false, false, true},
			{true, false, false, false, true},
			{false, true, true, true, true}
		};
		m_patterns.push_back(G_pat);

		LetterPattern S_pat;
		S_pat.cLetter = 'S';
		S_pat.pattern = {
			{false, true, true, true, false},
			{true, false, false, false, true},
			{true, false, false, false, false},
			{false, true, true, true, false},
			{false, false, false, false, true},
			{true, false, false, false, true},
			{false, true, true, true, false}
		};
		m_patterns.push_back(S_pat);

		LetterPattern R_pat;
		R_pat.cLetter = 'R';
		R_pat.pattern = {
			{true, true, true, true, false},
			{true, false, false, false, true},
			{true, false, false, false, true},
			{true, true, true, true, false},
			{true, false, true, false, false},
			{true, false, false, true, false},
			{true, false, false, false, true}
		};
		m_patterns.push_back(R_pat);
	}
};

const std::vector<std::vector<bool>>& GetLetterPattern(char cLetter) {
	return LetterPatternTable::GetInstance().GetPattern(cLetter);
}

CTextLetterMesh::CTextLetterMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	const std::vector<std::vector<bool>>& vPattern, float fPixelSize, XMFLOAT4 xmf4Color)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	std::vector<CDiffusedVertex> vVertices;
	std::vector<UINT> vIndices;

	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	int nQuadIndex = 0;
	for (int row = 0; row < LETTER_GRID_ROWS; ++row) {
		for (int col = 0; col < LETTER_GRID_COLS; ++col) {
			if (vPattern[row][col]) {
				float x = col * fPixelSize;
				float y = -row * fPixelSize;

				vVertices.push_back(CDiffusedVertex(XMFLOAT3(x, y, 0.0f), xmf4Color));
				vVertices.push_back(CDiffusedVertex(XMFLOAT3(x + fPixelSize, y, 0.0f), xmf4Color));
				vVertices.push_back(CDiffusedVertex(XMFLOAT3(x + fPixelSize, y - fPixelSize, 0.0f), xmf4Color));
				vVertices.push_back(CDiffusedVertex(XMFLOAT3(x, y - fPixelSize, 0.0f), xmf4Color));

				UINT base = nQuadIndex * 4;
				vIndices.push_back(base + 0);
				vIndices.push_back(base + 1);
				vIndices.push_back(base + 2);
				vIndices.push_back(base + 0);
				vIndices.push_back(base + 2);
				vIndices.push_back(base + 3);

				++nQuadIndex;
			}
		}
	}

	m_nVertices = static_cast<UINT>(vVertices.size());
	m_nIndices = static_cast<UINT>(vIndices.size());

	if (m_nVertices == 0) return;

	m_pd3dVertexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		vVertices.data(), m_nStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		m_pd3dVertexUploadBuffer.GetAddressOf()
	);

	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	m_pd3dIndexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		vIndices.data(), sizeof(UINT) * m_nIndices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		m_pd3dIndexUploadBuffer.GetAddressOf()
	);

	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;

	// 텍스트 정점 노멀: 셰이더가 NORMAL slot 을 요구하므로 +Y 방향으로 채워 가독성 유지
	std::vector<CNormalVertex> vNormals(m_nVertices, CNormalVertex(0.0f, 1.0f, 0.0f));
	const UINT normalStride = sizeof(CNormalVertex);
	m_pd3dNormalBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		vNormals.data(), normalStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		m_pd3dNormalUploadBuffer.GetAddressOf());
	m_d3dNormalBufferView.BufferLocation = m_pd3dNormalBuffer->GetGPUVirtualAddress();
	m_d3dNormalBufferView.StrideInBytes  = normalStride;
	m_d3dNormalBufferView.SizeInBytes    = normalStride * m_nVertices;
	m_bHasNormals = true;
}

CTextLetterMesh::~CTextLetterMesh() {
}

CLetterObject::CLetterObject() : CGameObject() {
}

CLetterObject::~CLetterObject() {
}

void CLetterObject::SetBasePosition(XMFLOAT3 xmf3Position) {
	m_xmf3BasePosition = xmf3Position;
	SetPosition(xmf3Position);
}

void CLetterObject::SetWaveAnimation(float fAmplitude, float fFrequency, float fPhaseOffset) {
	m_fWaveAmplitude = fAmplitude;
	m_fWaveFrequency = fFrequency;
	m_fWavePhaseOffset = fPhaseOffset;
	m_bAnimating = (fAmplitude > 0.0f && fFrequency > 0.0f);
}

void CLetterObject::Animate(float fTimeElapsed) {
	if (m_bAnimating) {
		m_fAccumulatedTime += fTimeElapsed;
		float fPhase = 2.0f * XM_PI * m_fWaveFrequency * m_fAccumulatedTime + m_fWavePhaseOffset;
		float fY = m_xmf3BasePosition.y + m_fWaveAmplitude * sinf(fPhase);
		SetPosition(m_xmf3BasePosition.x, fY, m_xmf3BasePosition.z);
	}
	CGameObject::Animate(fTimeElapsed);
}

CTextStringObject::CTextStringObject() : CGameObject() {
}

CTextStringObject::~CTextStringObject() {
}

void CTextStringObject::Build(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	const std::string& strText, XMFLOAT3 xmf3Position, float fPixelSize,
	float fLetterPitch, XMFLOAT4 xmf4Color, bool bAnimateWithWave,
	float fWaveAmplitude, float fWaveFrequency, float fWavePhaseOffset) {
	SetPosition(xmf3Position);
	m_fPixelSize = fPixelSize;

	for (size_t i = 0; i < strText.length(); ++i) {
		const auto& pattern = GetLetterPattern(strText[i]);
		auto pMesh = std::make_shared<CTextLetterMesh>(pd3dDevice, pd3dCommandList, pattern, fPixelSize, xmf4Color);

		auto pLetter = std::make_shared<CLetterObject>();
		pLetter->SetMesh(pMesh);

		XMFLOAT3 xmf3LetterPos = xmf3Position;
		xmf3LetterPos.x += static_cast<float>(i) * fLetterPitch * fPixelSize;
		pLetter->SetBasePosition(xmf3LetterPos);

		if (bAnimateWithWave) {
			float fPhaseOffset = fWavePhaseOffset * static_cast<float>(i);
			pLetter->SetWaveAnimation(fWaveAmplitude, fWaveFrequency, fPhaseOffset);
		}

		m_vLetters.push_back(pLetter);
	}

	UpdateBoundingBox();
}

void CTextStringObject::Animate(float fTimeElapsed) {
	for (auto& pLetter : m_vLetters) {
		pLetter->Animate(fTimeElapsed);
	}
}

void CTextStringObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera) {
	for (auto& pLetter : m_vLetters) {
		pLetter->Render(pd3dCommandList, pCamera);
	}
}

void CTextStringObject::UpdateBoundingBox() {
	if (m_vLetters.empty()) return;

	XMFLOAT3 first = m_vLetters[0]->GetPosition();
	m_xmf3BoundingMin = first;
	m_xmf3BoundingMax = first;

	for (auto& pLetter : m_vLetters) {
		XMFLOAT3 pos = pLetter->GetPosition();
		if (pos.x < m_xmf3BoundingMin.x) m_xmf3BoundingMin.x = pos.x;
		if (pos.y < m_xmf3BoundingMin.y) m_xmf3BoundingMin.y = pos.y;
		if (pos.z < m_xmf3BoundingMin.z) m_xmf3BoundingMin.z = pos.z;

		if (pos.x > m_xmf3BoundingMax.x) m_xmf3BoundingMax.x = pos.x;
		if (pos.y > m_xmf3BoundingMax.y) m_xmf3BoundingMax.y = pos.y;
		if (pos.z > m_xmf3BoundingMax.z) m_xmf3BoundingMax.z = pos.z;
	}

	// CTextLetterMesh 가 시작점 (px, py) 에서 +X 방향으로 LETTER_GRID_COLS * pixelSize 폭,
	// -Y 방향으로 LETTER_GRID_ROWS * pixelSize 깊이로 글자를 그린다.
	// 전체 문자열 너비 계산 시에는 글자 사이 일정 픽셀 간격을 끼워 넣는다.
	const float fGlyphW = m_fPixelSize * static_cast<float>(LETTER_GRID_COLS);
	const float fGlyphH = m_fPixelSize * static_cast<float>(LETTER_GRID_ROWS);
	m_xmf3BoundingMax.x += fGlyphW;
	m_xmf3BoundingMin.y -= fGlyphH;
}

CButtonObject::CButtonObject() : CTextStringObject() {
}

CButtonObject::~CButtonObject() {
}

XMFLOAT3 CButtonObject::UnprojectScreenToWorld(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight,
	const XMFLOAT4X4& xmf4x4View, const XMFLOAT4X4& xmf4x4Projection) {
	// 화면 좌표를 z=0 평면 위의 월드 좌표로 변환한다.
	float fNDCX = 2.0f * nMouseX / nScreenWidth - 1.0f;
	float fNDCY = 1.0f - 2.0f * nMouseY / nScreenHeight;

	XMMATRIX xmProj = XMLoadFloat4x4(&xmf4x4Projection);
	XMMATRIX xmView = XMLoadFloat4x4(&xmf4x4View);
	// inverse(view * proj) 로 클립 공간 좌표를 월드 공간 좌표로 되돌린다.
	XMMATRIX xmInvViewProj = XMMatrixInverse(nullptr, XMMatrixMultiply(xmView, xmProj));

	// 근평면(z=0) 과 원평면(z=1) 양쪽 점을 각각 월드 공간 좌표로 환산한 뒤 그 차를 광선 방향으로 삼는다.
	XMVECTOR vClipNear = XMVectorSet(fNDCX, fNDCY, 0.0f, 1.0f);
	XMVECTOR vClipFar  = XMVectorSet(fNDCX, fNDCY, 1.0f, 1.0f);
	XMVECTOR vWorldNear = XMVector4Transform(vClipNear, xmInvViewProj);
	vWorldNear = XMVectorScale(vWorldNear, 1.0f / XMVectorGetW(vWorldNear));
	XMVECTOR vWorldFar  = XMVector4Transform(vClipFar,  xmInvViewProj);
	vWorldFar  = XMVectorScale(vWorldFar,  1.0f / XMVectorGetW(vWorldFar));

	// 광 원점이 z=0 평면과 만나는 위치를 구함. 광선 방향의 월드 z=0 면 교점이 클릭 좌표가 된다.
	XMVECTOR vDir = XMVectorSubtract(vWorldFar, vWorldNear);
	float fDirZ  = XMVectorGetZ(vDir);
	float fNearZ = XMVectorGetZ(vWorldNear);
	XMFLOAT3 xmf3Result{ 0.0f, 0.0f, 0.0f };
	if (fabsf(fDirZ) > 1e-6f) {
		float t = -fNearZ / fDirZ;
		XMVECTOR vHit = XMVectorAdd(vWorldNear, XMVectorScale(vDir, t));
		XMStoreFloat3(&xmf3Result, vHit);
	}
	return xmf3Result;
}

bool CButtonObject::HitTest(int nMouseX, int nMouseY, int nScreenWidth, int nScreenHeight,
	const XMFLOAT4X4& xmf4x4View, const XMFLOAT4X4& xmf4x4Projection) {
	XMFLOAT3 xmf3WorldPos = UnprojectScreenToWorld(nMouseX, nMouseY, nScreenWidth, nScreenHeight, xmf4x4View, xmf4x4Projection);

	float fMinX = m_xmf3BoundingMin.x - LandingParams::BUTTON_HITBOX_PADDING_X;
	float fMaxX = m_xmf3BoundingMax.x + LandingParams::BUTTON_HITBOX_PADDING_X;
	float fMinY = m_xmf3BoundingMin.y - LandingParams::BUTTON_HITBOX_PADDING_Y;
	float fMaxY = m_xmf3BoundingMax.y + LandingParams::BUTTON_HITBOX_PADDING_Y;

	return (xmf3WorldPos.x >= fMinX && xmf3WorldPos.x <= fMaxX &&
		xmf3WorldPos.y >= fMinY && xmf3WorldPos.y <= fMaxY);
}

void BuildLandingObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects, std::shared_ptr<CButtonObject>& pStartButton) {
	auto pTitle = std::make_shared<CTextStringObject>();
	pTitle->Build(pd3dDevice, pd3dCommandList, "THEMAZE",
		LandingParams::TITLE_ANCHOR_POSITION,
		LandingParams::TITLE_PIXEL_SIZE,
		LandingParams::TITLE_LETTER_PITCH_PIXELS,
		LandingParams::TITLE_COLOR,
		true,
		LandingParams::TITLE_WAVE_AMPLITUDE,
		LandingParams::TITLE_WAVE_FREQUENCY_HZ,
		LandingParams::TITLE_WAVE_PHASE_OFFSET_PER_LETTER);
	vObjects.push_back(pTitle);

	pStartButton = std::make_shared<CButtonObject>();
	pStartButton->Build(pd3dDevice, pd3dCommandList, "GAMESTART",
		LandingParams::BUTTON_ANCHOR_POSITION,
		LandingParams::BUTTON_PIXEL_SIZE,
		LandingParams::BUTTON_LETTER_PITCH_PIXELS,
		LandingParams::BUTTON_COLOR,
		false, 0.0f, 0.0f, 0.0f);
	vObjects.push_back(pStartButton);
}
