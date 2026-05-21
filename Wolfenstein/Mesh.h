#pragma once

// ������ ǥ���ϱ� ���� Ŭ������ �����Ѵ�.
class CVertex {
protected:
	// ������ ��ġ �����̴� (��� ������ �ּ��� ��ġ ���͸� ������ �Ѵ�).
	XMFLOAT3 m_xmf3Position;

public:
	CVertex() { m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); }
	CVertex(XMFLOAT3 xmf3Position) { m_xmf3Position = xmf3Position; }
	~CVertex() {}
};

class CDiffusedVertex : public CVertex {
protected:
	// ������ �����̴�.
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

// 노멀 전용 정점 구조.
// 위치/색상 버퍼(slot 0)와 분리하여 slot 1 에 병렬로 바인딩한다.
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

	// 노멀 병렬 버퍼 (slot 1). m_bHasNormals 가 true 일 때만 바인딩.
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
	// 직육면체의 가로, 세로, 깊이 길이를 지정하여 직육면체 메시를 생성한다.
	CCubeMeshDiffused(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		float fWidth = 2.0f, float fHeight = 2.0f, float fDepth = 2.0f,
		bool bUseUniformColor = false,
		XMFLOAT4 xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	virtual ~CCubeMeshDiffused();
};

// 화면 정중앙에 회전 없이 고정되는 십자선(+) 조준점 메시.
// 정점은 NDC(클립 공간) 좌표로 직접 저장되므로 월드/뷰/투영 행렬을 거치지 않는
// VSHud 셰이더와 함께 사용해야 한다. 두 직사각형(가로 막대 + 세로 막대) 으로
// 구성되며, 두 막대의 길이/두께는 픽셀 단위로 지정하고 현재 화면 해상도를 이용해
// 정사각형 비율의 십자선이 되도록 NDC 좌표를 계산한다.
class CCrosshairMesh : public CMesh
{
public:
	CCrosshairMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		UINT nScreenWidth, UINT nScreenHeight,
		UINT nArmLengthPx = 10, UINT nThicknessPx = 2,
		XMFLOAT4 xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	virtual ~CCrosshairMesh();
};

// 화면 중앙 하단에 위치하는 라이프 바 칸 한 개(작은 직사각형) 메시.
// 한 인스턴스가 칸 1개를 NDC 좌표로 표현한다. nSegmentIndex(0..nTotalSegments-1)
// 로 좌→우 순서를 정해 가로로 정렬된다. 활성 칸만 그리는 단순한 방식이므로
// 잃은 라이프 칸은 GameFramework 측에서 그리지 않도록 분기 처리한다.
// CHudShader 와 함께 사용 (월드/뷰/투영 무시, 깊이 OFF).
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