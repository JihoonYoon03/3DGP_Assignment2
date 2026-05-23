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

// [Claude] 다수의 직육면체를 단일 정점/인덱스 버퍼로 통합한 정적 메시.
// 미로 셀(벽/바닥)은 위치/크기/색만 다르고 회전이 없으므로, 각 큐브의 정점을
// 미리 월드 좌표로 변환해 한 번의 DrawIndexedInstanced 로 그릴 수 있다.
// 이는 1800+ 개의 개별 드로우 콜을 단 1 개로 줄여 CPU bound 인 렌더링 부하를
// 극적으로 감소시킨다 (가장 큰 최적화 효과). 동적 객체(적/총알) 는 그대로 유지.
//
// 사용:
//   std::vector<CMergedCubeMesh::Cube> cubes; cubes.push_back({center, size, color});
//   auto pMesh = std::make_shared<CMergedCubeMesh>(device, cmdList, cubes);
//   auto pObj  = std::make_shared<CGameObject>(); pObj->SetMesh(pMesh);
//   // pObj 의 world 행렬은 identity 로 둔다 (정점이 이미 월드 좌표).
class CMergedCubeMesh : public CMesh
{
public:
	struct Cube {
		XMFLOAT3 center;   // 월드 중심 좌표
		XMFLOAT3 size;     // (width, height, depth) — 전체 크기
		XMFLOAT4 color;    // 정점 디퓨즈 색상
	};

	CMergedCubeMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		const std::vector<Cube>& cubes);
	virtual ~CMergedCubeMesh() = default;
};

// [Claude] Wavefront .obj 파일을 로드해 정점/노멀/인덱스 버퍼를 채우는 메시.
// 면 단위 평탄 음영(flat shading) — 각 페이스의 모든 정점이 그 페이스의 노멀을 갖도록
// 정점을 페이스 별로 분리한다(CCubeMeshDiffused 와 동일 방식).
//
// xmf4x4ModelTransform: 모델 좌표계 → 엔진 좌표계 변환을 정점/노멀에 베이크한다.
//   - 회전: 모델의 forward 축을 엔진의 +Z 로 정렬.
//   - 스케일: 원본 단위 → 게임 단위로 축소(소총은 ~1.2).
//   - 평행이동: 모델 중심을 원점으로 옮기는 보정.
// 호출자가 이 행렬을 조정해 방향이 맞지 않는 경우를 보정한다.
//
// xmf4FallbackColor: o 그룹 이름이 내장 부품 색상 테이블에 없을 때의 폴백 색.
//   ("Stock" → 우드, "Mag" → 짙은 메탈 / "Front", "Base" → 폴백 색)
//
// 지원 포맷: v / vn / f, 페이스는 v//vn 또는 v/vt/vn 또는 v/vt 또는 v 모두 허용.
// n-gon(>3) 페이스는 fan triangulation 으로 삼각형 분할.
// vn 이 없는 페이스는 정점 위치로부터 면 노멀을 계산(cross product).
// vt 와 s 는 무시. o 그룹 이름은 부품별 색상 분배에 사용.
class CObjMesh : public CMesh
{
public:
	CObjMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		const std::wstring& wsObjPath,
		XMFLOAT4 xmf4FallbackColor = XMFLOAT4(0.25f, 0.25f, 0.28f, 1.0f),
		const XMFLOAT4X4& xmf4x4ModelTransform = Matrix4x4::Identity());
	virtual ~CObjMesh() = default;
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

// NDC 좌표 (xL, xR, yT, yB) 를 직접 받아 화면에 직사각형 1개를 그리는 범용 메시.
// 좌표는 [-1, 1] 범위. yT 가 yB 보다 커야 위쪽이 위로 향한다 (NDC Y는 위가 +).
// CHudShader 와 함께 사용. 적 잔여 수 점 카운트, "WIN" 글자 세그먼트 등 HUD
// 정형 사각형이 필요한 모든 곳에 공유되는 유연한 빌딩 블록이다.
class CHudQuadMesh : public CMesh
{
public:
	CHudQuadMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		float xL, float xR, float yT, float yB,
		XMFLOAT4 xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
	virtual ~CHudQuadMesh();
};