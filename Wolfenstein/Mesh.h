#pragma once

// 정점 기본 클래스 (위치)
class CVertex {
protected:
	XMFLOAT3 m_xmf3Position;

public:
	CVertex() { m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); }
	CVertex(XMFLOAT3 xmf3Position) { m_xmf3Position = xmf3Position; }
	~CVertex() {}
};

// 색상 정점
class CDiffusedVertex : public CVertex {
protected:
	XMFLOAT4 m_xmf4Diffuse;

public:
	CDiffusedVertex() {
		m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		m_xmf4Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	CDiffusedVertex(float x, float y, float z, XMFLOAT4 xmf4Diffuse) {
		m_xmf3Position = XMFLOAT3(x, y, z);
		m_xmf4Diffuse = xmf4Diffuse;
	}

	CDiffusedVertex(XMFLOAT3 xmf3Position, XMFLOAT4 xmf4Diffuse) {
		m_xmf3Position = xmf3Position;
		m_xmf4Diffuse = xmf4Diffuse;
	}
	~CDiffusedVertex() {}
};

// 노멀 전용 정점 (slot 1 에 분리 바인딩)
class CNormalVertex {
public:
	XMFLOAT3 m_xmf3Normal;
	CNormalVertex() : m_xmf3Normal(0.0f, 0.0f, 0.0f) {}
	CNormalVertex(XMFLOAT3 n) : m_xmf3Normal(n) {}
	CNormalVertex(float x, float y, float z) : m_xmf3Normal(x, y, z) {}
};

class CMesh {
public:
	CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual ~CMesh();

public:
	void ReleaseUploadBuffers();

protected:
	ComPtr<ID3D12Resource>		m_pd3dVertexBuffer;
	ComPtr<ID3D12Resource>		m_pd3dVertexUploadBuffer;
	ComPtr<ID3D12Resource>		m_pd3dIndexBuffer;
	ComPtr<ID3D12Resource>		m_pd3dIndexUploadBuffer;

	// 노멀 병렬 버퍼 (slot 1)
	ComPtr<ID3D12Resource>		m_pd3dNormalBuffer;
	ComPtr<ID3D12Resource>		m_pd3dNormalUploadBuffer;
	bool					m_bHasNormals = false;

	D3D12_VERTEX_BUFFER_VIEW	m_d3dVertexBufferView;
	D3D12_VERTEX_BUFFER_VIEW	m_d3dNormalBufferView{};
	D3D12_INDEX_BUFFER_VIEW		m_d3dIndexBufferView;

	D3D12_PRIMITIVE_TOPOLOGY	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT	m_nSlot = 0;
	UINT	m_nVertices = 0;
	UINT	m_nStride = 0;
	UINT	m_nOffset = 0;

	UINT	m_nIndices = 0;
	UINT	m_nStartIndex = 0;
	int		m_nBaseVertex = 0;

public:
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList);
};

class CTriangleMesh : public CMesh
{
public:
	CTriangleMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual ~CTriangleMesh() {}
};

class CCubeMeshDiffused : public CMesh
{
public:
	// 가로/세로/깊이를 지정한 직육면체 메시
	CCubeMeshDiffused(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		float fWidth = 2.0f, float fHeight = 2.0f, float fDepth = 2.0f,
		bool bUseUniformColor = false,
		XMFLOAT4 xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	virtual ~CCubeMeshDiffused();
};

// 여러 큐브를 한 정점/인덱스 버퍼로 통합한 정적 메시 (드로우 콜 절감용)
class CMergedCubeMesh : public CMesh
{
public:
	struct Cube {
		XMFLOAT3 center;
		XMFLOAT3 size;
		XMFLOAT4 color;
	};

	CMergedCubeMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		const std::vector<Cube>& cubes);
	virtual ~CMergedCubeMesh() = default;
};

// Wavefront .obj 파일 로더. 면 단위 평탄 음영(flat shading)으로 노멀을 분리한다.
// xmf4x4ModelTransform 으로 모델 좌표계 → 엔진 좌표계 변환을 정점/노멀에 베이크.
class CObjMesh : public CMesh
{
public:
	CObjMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		const std::wstring& wsObjPath,
		XMFLOAT4 xmf4FallbackColor = XMFLOAT4(0.25f, 0.25f, 0.28f, 1.0f),
		const XMFLOAT4X4& xmf4x4ModelTransform = Matrix4x4::Identity());
	virtual ~CObjMesh() = default;
};

// 화면 정중앙 십자선(+) 메시. 정점이 NDC 좌표로 저장됨.
class CCrosshairMesh : public CMesh
{
public:
	CCrosshairMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		UINT nScreenWidth, UINT nScreenHeight,
		UINT nArmLengthPx = 10, UINT nThicknessPx = 2,
		XMFLOAT4 xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	virtual ~CCrosshairMesh();
};

// 라이프 바 칸 한 개 메시. 한 인스턴스가 한 칸을 표현 (NDC 좌표).
class CLifeBarMesh : public CMesh
{
public:
	CLifeBarMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		UINT nScreenWidth, UINT nScreenHeight,
		UINT nSegmentIndex, UINT nTotalSegments = 10,
		UINT nSegmentWidthPx = 22, UINT nSegmentHeightPx = 8,
		UINT nGapPx = 4, UINT nBottomMarginPx = 24,
		XMFLOAT4 xmf4Color = XMFLOAT4(0.95f, 0.25f, 0.30f, 1.0f));
	virtual ~CLifeBarMesh();
};

// NDC 좌표 직사각형 메시 (적 카운트 점, WIN 글자 세그먼트 등에 공용)
class CHudQuadMesh : public CMesh
{
public:
	CHudQuadMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		float xL, float xR, float yT, float yB,
		XMFLOAT4 xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	virtual ~CHudQuadMesh();
};
