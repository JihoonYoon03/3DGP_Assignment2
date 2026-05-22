#include "stdafx.h"
#include "Mesh.h"

// ====================================================================================
CMesh::CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
// ====================================================================================
{

}

CMesh::~CMesh()
{
	if (m_pd3dIndexBuffer) m_pd3dIndexBuffer.Reset();
	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer.Reset();
}

void CMesh::ReleaseUploadBuffers()
{
	// 정점 버퍼를 위한 업로드 버퍼를 소멸시킨다.
	if (m_pd3dVertexUploadBuffer)
		m_pd3dVertexUploadBuffer.Reset();

	if (m_pd3dIndexUploadBuffer)
		m_pd3dIndexUploadBuffer.Reset();

	// 노멀 업로드 버퍼(존재 시) 해제
	if (m_pd3dNormalUploadBuffer)
		m_pd3dNormalUploadBuffer.Reset();
};

void CMesh::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 메쉬의 프리미티브 유형을 설정한다.
	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);

	// 위치/색 정점 버퍼(slot 0) 바인딩.
	pd3dCommandList->IASetVertexBuffers(m_nSlot, 1, &m_d3dVertexBufferView);

	// 노멀이 있으면 slot 1 에 추가 바인딩 (라이팅 셰이더용).
	if (m_bHasNormals) {
		pd3dCommandList->IASetVertexBuffers(1, 1, &m_d3dNormalBufferView);
	}

	// 인덱스 버퍼가 있으면 파이프라인에 연결하고 렌더링
	if (m_pd3dIndexBuffer) {
		pd3dCommandList->IASetIndexBuffer(&m_d3dIndexBufferView);
		pd3dCommandList->DrawIndexedInstanced(m_nIndices, 1, 0, 0, 0);
	}
	else {
		pd3dCommandList->DrawInstanced(m_nVertices, 1, m_nOffset, 0);
	}
}


// ====================================================================================
// ====================================================================================
CTriangleMesh::CTriangleMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	//삼각형 메쉬를 정의한다.
	m_nVertices = 3;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	/* 정점(삼각형의 꼭지점)의 색상은 시계방향 순서대로 빨간색, 녹색, 파란색으로 지정한다.
	RGBA(Red, Green, Blue, Alpha) 4개의 파라메터를 사용하여 색상을 표현한다.
	각 파라메터는 0.0~1.0 사이의 실수값을 가진다.*/
	CDiffusedVertex pVertices[3];
	pVertices[0] = CDiffusedVertex(XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f));
	pVertices[1] = CDiffusedVertex(XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f));
	pVertices[2] = CDiffusedVertex(XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(Colors::Blue));

	// 삼각형 메쉬를 리소스(정점 버퍼)로 생성한다.
	m_pd3dVertexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pVertices, m_nStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		m_pd3dVertexUploadBuffer.GetAddressOf()
	);

	// 정점 버퍼 뷰를 생성한다.
	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;
}


// ====================================================================================
// ====================================================================================
CCubeMeshDiffused::CCubeMeshDiffused(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	float fWidth, float fHeight, float fDepth,
	bool bUseUniformColor, XMFLOAT4 xmf4Color)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	m_nVertices = 8;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	float fx = fWidth * 0.5f, fy = fHeight * 0.5f, fz = fDepth * 0.5f;
	
	// 직육면체의 꼭지점 8개의 정점 데이터
	// bUseUniformColor : true means all 8 vertices share the same uniform color;
	// false keeps the original per-vertex random color behaviour.
	auto VertexColor = [&]() -> XMFLOAT4 {
		return bUseUniformColor ? xmf4Color : RANDOM_COLOR;
	};

	CDiffusedVertex pVertices[8];
	pVertices[0] = CDiffusedVertex(XMFLOAT3(-fx, +fy, -fz), VertexColor());
	pVertices[1] = CDiffusedVertex(XMFLOAT3(+fx, +fy, -fz), VertexColor());
	pVertices[2] = CDiffusedVertex(XMFLOAT3(+fx, +fy, +fz), VertexColor());
	pVertices[3] = CDiffusedVertex(XMFLOAT3(-fx, +fy, +fz), VertexColor());
	pVertices[4] = CDiffusedVertex(XMFLOAT3(-fx, -fy, -fz), VertexColor());
	pVertices[5] = CDiffusedVertex(XMFLOAT3(+fx, -fy, -fz), VertexColor());
	pVertices[6] = CDiffusedVertex(XMFLOAT3(+fx, -fy, +fz), VertexColor());
	pVertices[7] = CDiffusedVertex(XMFLOAT3(-fx, -fy, +fz), VertexColor());

	m_pd3dVertexBuffer = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		pVertices,
		m_nStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dVertexUploadBuffer
	);

	// 정점 버퍼 뷰를 생성한다
	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	/*
	인덱스 버퍼는 6개 면에 대한 기하 정보를 가짐.
	삼각형 리스트로 직육면체를 표현하므로 각 면마다 2개의 삼각형,
	즉 36개의 인덱스를 가짐(6 * 2 * 3)
	*/
	m_nIndices = 36;

	UINT pnIndices[36] = {
		// 앞면(Front) 사각형의 위쪽 삼각형
		3, 1, 0,
		// 앞면(Front) 사각형의 아래쪽 삼각형
		2, 1, 3,
		// 윗면(Top) 사각형의 위쪽 삼각형
		0, 5, 4,
		// 윗면(Top) 사각형의 아래쪽 삼각형
		1, 5, 0,
		// 뒷면(Back) 사각형의 위쪽 삼각형
		3, 4, 7,
		// 뒷면(Back) 사각형의 아래쪽 삼각형
		0, 4, 3,
		// 아래면(Bottom) 사각형의 위쪽 삼각형
		1, 6, 5,
		// 아래면(Bottom) 사각형의 아래쪽 삼각형
		2, 6, 1,
		// 옆면(Left) 사각형의 위쪽 삼각형
		2, 7, 6,
		// 옆면(Left) 사각형의 아래쪽 삼각형
		3, 7, 2,
		// 옆면(Right) 사각형의 위쪽 삼각형
		6, 4, 5,
		// 옆면(Right) 사각형의 아래쪽 삼각형
		7, 4, 6
	};

	// 인덱스 버퍼를 생성한다
	m_pd3dIndexBuffer = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		pnIndices,
		sizeof(UINT) * m_nIndices,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_pd3dIndexUploadBuffer);

	// 인덱스 버퍼 뷰를 생성한다
	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;

	// 노멀 병렬 버퍼 생성 (slot 1).
	// 8 정점 큐브에서 각 정점의 노멀은 인접 면 평균 = 큐브 중심에서 정점 위치로 향하는
	// 단위 벡터로 계산한다. 결과적으로 (+-1, +-1, +-1)/sqrt(3) 으로 정규화된 8 개 노멀.
	CNormalVertex pNormals[8];
	const float kInvSqrt3 = 1.0f / sqrtf(3.0f);
	pNormals[0] = CNormalVertex(-kInvSqrt3, +kInvSqrt3, -kInvSqrt3);
	pNormals[1] = CNormalVertex(+kInvSqrt3, +kInvSqrt3, -kInvSqrt3);
	pNormals[2] = CNormalVertex(+kInvSqrt3, +kInvSqrt3, +kInvSqrt3);
	pNormals[3] = CNormalVertex(-kInvSqrt3, +kInvSqrt3, +kInvSqrt3);
	pNormals[4] = CNormalVertex(-kInvSqrt3, -kInvSqrt3, -kInvSqrt3);
	pNormals[5] = CNormalVertex(+kInvSqrt3, -kInvSqrt3, -kInvSqrt3);
	pNormals[6] = CNormalVertex(+kInvSqrt3, -kInvSqrt3, +kInvSqrt3);
	pNormals[7] = CNormalVertex(-kInvSqrt3, -kInvSqrt3, +kInvSqrt3);

	const UINT normalStride = sizeof(CNormalVertex);
	m_pd3dNormalBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pNormals, normalStride * 8,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dNormalUploadBuffer);

	m_d3dNormalBufferView.BufferLocation = m_pd3dNormalBuffer->GetGPUVirtualAddress();
	m_d3dNormalBufferView.StrideInBytes  = normalStride;
	m_d3dNormalBufferView.SizeInBytes    = normalStride * 8;
	m_bHasNormals = true;
}

CCubeMeshDiffused::~CCubeMeshDiffused()
{
}


// ====================================================================================
// [Claude] CMergedCubeMesh — 다수 직육면체를 단일 메시로 통합
// ====================================================================================
// 각 큐브의 8개 정점을 미리 월드 좌표로 변환해 단일 정점/인덱스/노멀 버퍼로 합친다.
// N 개의 큐브 = 8N 정점 + 36N 인덱스 + 8N 노멀, 단 1 회의 DrawIndexedInstanced 로 렌더.
// 노멀은 큐브 중심에서 정점 위치로 향하는 단위 벡터 (각 정점마다 (±1,±1,±1)/sqrt(3)).
CMergedCubeMesh::CMergedCubeMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	const std::vector<Cube>& cubes)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	const size_t nCubes = cubes.size();
	m_nVertices = static_cast<UINT>(nCubes * 8);
	m_nIndices  = static_cast<UINT>(nCubes * 36);

	if (nCubes == 0) {
		// 빈 메시여도 Render 호출 시 안전하도록 dummy 버퍼라도 만들어두지는 않는다 —
		// CMesh::Render 가 m_pd3dVertexBuffer 가 없으면 IASetVertexBuffers 에서 크래시.
		// 호출부에서 nCubes > 0 인 경우에만 인스턴스화해야 한다.
		return;
	}

	// 큐브 인덱스 패턴 (CCubeMeshDiffused 와 동일). 정점 8 개 단위 오프셋만 더하면 된다.
	const UINT kBaseIndices[36] = {
		3, 1, 0,   2, 1, 3,   // 앞면(Front)
		0, 5, 4,   1, 5, 0,   // 윗면(Top)
		3, 4, 7,   0, 4, 3,   // 뒷면(Back)
		1, 6, 5,   2, 6, 1,   // 아래면(Bottom)
		2, 7, 6,   3, 7, 2,   // 옆면(Left)
		6, 4, 5,   7, 4, 6    // 옆면(Right)
	};

	// 노멀: 큐브 중심 → 정점 방향 단위 벡터. CCubeMeshDiffused 와 동일.
	const float kInvSqrt3 = 1.0f / sqrtf(3.0f);
	const XMFLOAT3 kBaseNormals[8] = {
		{ -kInvSqrt3, +kInvSqrt3, -kInvSqrt3 },
		{ +kInvSqrt3, +kInvSqrt3, -kInvSqrt3 },
		{ +kInvSqrt3, +kInvSqrt3, +kInvSqrt3 },
		{ -kInvSqrt3, +kInvSqrt3, +kInvSqrt3 },
		{ -kInvSqrt3, -kInvSqrt3, -kInvSqrt3 },
		{ +kInvSqrt3, -kInvSqrt3, -kInvSqrt3 },
		{ +kInvSqrt3, -kInvSqrt3, +kInvSqrt3 },
		{ -kInvSqrt3, -kInvSqrt3, +kInvSqrt3 },
	};

	std::vector<CDiffusedVertex> vertices;
	std::vector<CNormalVertex>   normals;
	std::vector<UINT>            indices;
	vertices.reserve(m_nVertices);
	normals.reserve(m_nVertices);
	indices.reserve(m_nIndices);

	for (size_t i = 0; i < nCubes; ++i) {
		const Cube& c = cubes[i];
		const float fx = c.size.x * 0.5f;
		const float fy = c.size.y * 0.5f;
		const float fz = c.size.z * 0.5f;
		const float cx = c.center.x;
		const float cy = c.center.y;
		const float cz = c.center.z;

		// 8 개 정점을 월드 좌표로 직접 저장. world 행렬은 호출부에서 identity 사용.
		vertices.emplace_back(XMFLOAT3(cx - fx, cy + fy, cz - fz), c.color);
		vertices.emplace_back(XMFLOAT3(cx + fx, cy + fy, cz - fz), c.color);
		vertices.emplace_back(XMFLOAT3(cx + fx, cy + fy, cz + fz), c.color);
		vertices.emplace_back(XMFLOAT3(cx - fx, cy + fy, cz + fz), c.color);
		vertices.emplace_back(XMFLOAT3(cx - fx, cy - fy, cz - fz), c.color);
		vertices.emplace_back(XMFLOAT3(cx + fx, cy - fy, cz - fz), c.color);
		vertices.emplace_back(XMFLOAT3(cx + fx, cy - fy, cz + fz), c.color);
		vertices.emplace_back(XMFLOAT3(cx - fx, cy - fy, cz + fz), c.color);

		for (int j = 0; j < 8; ++j) normals.emplace_back(kBaseNormals[j]);

		const UINT base = static_cast<UINT>(i * 8);
		for (int j = 0; j < 36; ++j) indices.push_back(kBaseIndices[j] + base);
	}

	// ===== 정점 버퍼 (slot 0) =====
	m_pd3dVertexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		vertices.data(), m_nStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dVertexUploadBuffer);
	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes  = m_nStride;
	m_d3dVertexBufferView.SizeInBytes    = m_nStride * m_nVertices;

	// ===== 인덱스 버퍼 =====
	m_pd3dIndexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		indices.data(), sizeof(UINT) * m_nIndices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_pd3dIndexUploadBuffer);
	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format         = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes    = sizeof(UINT) * m_nIndices;

	// ===== 노멀 버퍼 (slot 1) =====
	const UINT normalStride = sizeof(CNormalVertex);
	m_pd3dNormalBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		normals.data(), normalStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dNormalUploadBuffer);
	m_d3dNormalBufferView.BufferLocation = m_pd3dNormalBuffer->GetGPUVirtualAddress();
	m_d3dNormalBufferView.StrideInBytes  = normalStride;
	m_d3dNormalBufferView.SizeInBytes    = normalStride * m_nVertices;
	m_bHasNormals = true;
}


// ====================================================================================
// CCrosshairMesh : 화면 정중앙 고정 십자선(+) 조준점
// ====================================================================================
CCrosshairMesh::CCrosshairMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	UINT nScreenWidth, UINT nScreenHeight,
	UINT nArmLengthPx, UINT nThicknessPx,
	XMFLOAT4 xmf4Color)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	// 가로 막대 + 세로 막대 = 8 개 정점, 4 개 삼각형 = 12 개 인덱스.
	m_nVertices = 8;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// 화면 픽셀 단위 길이를 NDC([-1,1]) 로 환산.
	// NDC 의 1 단위 = 화면 해상도의 절반에 해당하므로 px * 2 / 해상도 가 된다.
	if (nScreenWidth == 0) nScreenWidth = 1;
	if (nScreenHeight == 0) nScreenHeight = 1;
	const float armX = float(nArmLengthPx) * 2.0f / float(nScreenWidth);
	const float armY = float(nArmLengthPx) * 2.0f / float(nScreenHeight);
	const float thickX = float(nThicknessPx) * 2.0f / float(nScreenWidth);
	const float thickY = float(nThicknessPx) * 2.0f / float(nScreenHeight);
	const float hx = armX;          // 가로 막대 반길이 (x)
	const float vt = thickY * 0.5f; // 가로 막대 반두께 (y)
	const float vx = thickX * 0.5f; // 세로 막대 반두께 (x)
	const float vy = armY;          // 세로 막대 반길이 (y)

	// 정점 z 는 0 으로 두고, VSHud 가 그대로 NDC 로 사용한다.
	CDiffusedVertex pVertices[8];
	// 가로 막대 (좌상,우상,우하,좌하)
	pVertices[0] = CDiffusedVertex(XMFLOAT3(-hx, +vt, 0.0f), xmf4Color);
	pVertices[1] = CDiffusedVertex(XMFLOAT3(+hx, +vt, 0.0f), xmf4Color);
	pVertices[2] = CDiffusedVertex(XMFLOAT3(+hx, -vt, 0.0f), xmf4Color);
	pVertices[3] = CDiffusedVertex(XMFLOAT3(-hx, -vt, 0.0f), xmf4Color);
	// 세로 막대 (좌상,우상,우하,좌하)
	pVertices[4] = CDiffusedVertex(XMFLOAT3(-vx, +vy, 0.0f), xmf4Color);
	pVertices[5] = CDiffusedVertex(XMFLOAT3(+vx, +vy, 0.0f), xmf4Color);
	pVertices[6] = CDiffusedVertex(XMFLOAT3(+vx, -vy, 0.0f), xmf4Color);
	pVertices[7] = CDiffusedVertex(XMFLOAT3(-vx, -vy, 0.0f), xmf4Color);

	m_pd3dVertexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pVertices, m_nStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dVertexUploadBuffer);

	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	// 두 직사각형 = 4 삼각형 = 12 인덱스. CCW 정렬.
	m_nIndices = 12;
	UINT pnIndices[12] = {
		// 가로 막대
		0, 1, 2,
		0, 2, 3,
		// 세로 막대
		4, 5, 6,
		4, 6, 7,
	};

	m_pd3dIndexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pnIndices, sizeof(UINT) * m_nIndices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_pd3dIndexUploadBuffer);

	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;
}

CCrosshairMesh::~CCrosshairMesh()
{
}


// ====================================================================================
// CLifeBarMesh : 화면 중앙 하단의 라이프 칸 1개
// ====================================================================================
CLifeBarMesh::CLifeBarMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	UINT nScreenWidth, UINT nScreenHeight,
	UINT nSegmentIndex, UINT nTotalSegments,
	UINT nSegmentWidthPx, UINT nSegmentHeightPx,
	UINT nGapPx, UINT nBottomMarginPx,
	XMFLOAT4 xmf4Color)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	// 한 칸 = 직사각형 1개 = 정점 4 / 인덱스 6.
	m_nVertices = 4;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	if (nScreenWidth == 0) nScreenWidth = 1;
	if (nScreenHeight == 0) nScreenHeight = 1;
	if (nTotalSegments == 0) nTotalSegments = 1;

	// 라이프 바 전체 폭 / 한 칸 폭 / 간격을 NDC 단위로 환산.
	// NDC 1 단위 = 해상도 절반 이므로 px * 2 / 해상도 가 된다.
	const float segW = float(nSegmentWidthPx) * 2.0f / float(nScreenWidth);
	const float segH = float(nSegmentHeightPx) * 2.0f / float(nScreenHeight);
	const float gap = float(nGapPx) * 2.0f / float(nScreenWidth);
	const float bottomMargin = float(nBottomMarginPx) * 2.0f / float(nScreenHeight);

	// 가로 중앙 정렬: 전체 폭 = 칸N개 + 간격(N-1)개.
	const float totalW = segW * float(nTotalSegments) + gap * float(nTotalSegments - 1);
	const float leftEdge = -totalW * 0.5f;
	// 칸 i 의 좌측 X = 좌측 끝 + i * (segW + gap).
	const float xL = leftEdge + float(nSegmentIndex) * (segW + gap);
	const float xR = xL + segW;
	// 하단에서 bottomMargin 만큼 떨어진 곳에 위치.
	const float yB = -1.0f + bottomMargin;
	const float yT = yB + segH;

	// 정점은 NDC 좌표로 직접 저장. VSHud 가 그대로 사용한다.
	CDiffusedVertex pVertices[4];
	pVertices[0] = CDiffusedVertex(XMFLOAT3(xL, yT, 0.0f), xmf4Color); // 좌상
	pVertices[1] = CDiffusedVertex(XMFLOAT3(xR, yT, 0.0f), xmf4Color); // 우상
	pVertices[2] = CDiffusedVertex(XMFLOAT3(xR, yB, 0.0f), xmf4Color); // 우하
	pVertices[3] = CDiffusedVertex(XMFLOAT3(xL, yB, 0.0f), xmf4Color); // 좌하

	m_pd3dVertexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pVertices, m_nStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dVertexUploadBuffer);

	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	// 직사각형 1개 = 삼각형 2개 = 인덱스 6개 (CCW).
	m_nIndices = 6;
	UINT pnIndices[6] = { 0, 1, 2, 0, 2, 3 };

	m_pd3dIndexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pnIndices, sizeof(UINT) * m_nIndices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_pd3dIndexUploadBuffer);

	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;
}

CLifeBarMesh::~CLifeBarMesh()
{
}


// ====================================================================================
// CHudQuadMesh : NDC 좌표를 직접 받는 범용 직사각형
// ====================================================================================
CHudQuadMesh::CHudQuadMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	float xL, float xR, float yT, float yB,
	XMFLOAT4 xmf4Color)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	// 직사각형 1개 = 정점 4 / 인덱스 6. CLifeBarMesh 와 동일한 패턴이며
	// 좌표 계산만 호출부에서 미리 끝내는 형태이다.
	m_nVertices = 4;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	CDiffusedVertex pVertices[4];
	pVertices[0] = CDiffusedVertex(XMFLOAT3(xL, yT, 0.0f), xmf4Color); // 좌상
	pVertices[1] = CDiffusedVertex(XMFLOAT3(xR, yT, 0.0f), xmf4Color); // 우상
	pVertices[2] = CDiffusedVertex(XMFLOAT3(xR, yB, 0.0f), xmf4Color); // 우하
	pVertices[3] = CDiffusedVertex(XMFLOAT3(xL, yB, 0.0f), xmf4Color); // 좌하

	m_pd3dVertexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pVertices, m_nStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dVertexUploadBuffer);

	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes = m_nStride;
	m_d3dVertexBufferView.SizeInBytes = m_nStride * m_nVertices;

	m_nIndices = 6;
	UINT pnIndices[6] = { 0, 1, 2, 0, 2, 3 };

	m_pd3dIndexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pnIndices, sizeof(UINT) * m_nIndices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_pd3dIndexUploadBuffer);

	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes = sizeof(UINT) * m_nIndices;
}

CHudQuadMesh::~CHudQuadMesh()
{
}