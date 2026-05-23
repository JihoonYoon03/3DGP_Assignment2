#include "stdafx.h"
#include "Mesh.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

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
	// [Claude 수정] 면 단위 평탄 음영(flat shading) 을 위해 정점을 8 개 → 24 개로 분리.
	// 기존엔 정점 8 개가 인접한 3 면에서 공유되어 노멀이 (±1,±1,±1)/√3 대각선 방향으로
	// 평균되었고, 픽셀 셰이더에서 보간된 노멀이 면 사이를 매끄럽게 가로지르며 음영이
	// 둥글둥글해졌다 — 면 단위 구분이 안 됨. 이제 각 면마다 독립된 4 개 정점을 두고,
	// 그 4 정점에 그 면의 법선(±X / ±Y / ±Z 축 단위 벡터)을 부여한다.
	m_nVertices = 24;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	float fx = fWidth * 0.5f, fy = fHeight * 0.5f, fz = fDepth * 0.5f;

	// bUseUniformColor=false 일 때 RANDOM_COLOR 의 정점별 색상 분포를 보존하기 위해
	// 원본 8 코너의 색을 한 번씩만 미리 만들어두고, 분리된 24 정점에 위치별로 매핑한다.
	XMFLOAT4 cornerColor[8];
	for (int i = 0; i < 8; ++i) cornerColor[i] = bUseUniformColor ? xmf4Color : RANDOM_COLOR;

	// 원본 코너 좌표 (인덱스 매핑 주석용)
	//   0:(-fx,+fy,-fz) 1:(+fx,+fy,-fz) 2:(+fx,+fy,+fz) 3:(-fx,+fy,+fz)
	//   4:(-fx,-fy,-fz) 5:(+fx,-fy,-fz) 6:(+fx,-fy,+fz) 7:(-fx,-fy,+fz)
	CDiffusedVertex pVertices[24];
	CNormalVertex   pNormals[24];

	// ── 윗면 (+Y) ─ 원본 코너 0,1,2,3
	pVertices[0] = CDiffusedVertex(XMFLOAT3(-fx, +fy, -fz), cornerColor[0]);
	pVertices[1] = CDiffusedVertex(XMFLOAT3(+fx, +fy, -fz), cornerColor[1]);
	pVertices[2] = CDiffusedVertex(XMFLOAT3(+fx, +fy, +fz), cornerColor[2]);
	pVertices[3] = CDiffusedVertex(XMFLOAT3(-fx, +fy, +fz), cornerColor[3]);
	for (int i = 0; i < 4; ++i) pNormals[i] = CNormalVertex(0.0f, +1.0f, 0.0f);

	// ── -Z 면 ─ 원본 코너 0,1,5,4
	pVertices[4] = CDiffusedVertex(XMFLOAT3(-fx, +fy, -fz), cornerColor[0]);
	pVertices[5] = CDiffusedVertex(XMFLOAT3(+fx, +fy, -fz), cornerColor[1]);
	pVertices[6] = CDiffusedVertex(XMFLOAT3(+fx, -fy, -fz), cornerColor[5]);
	pVertices[7] = CDiffusedVertex(XMFLOAT3(-fx, -fy, -fz), cornerColor[4]);
	for (int i = 4; i < 8; ++i) pNormals[i] = CNormalVertex(0.0f, 0.0f, -1.0f);

	// ── -X 면 ─ 원본 코너 0,3,7,4
	pVertices[8]  = CDiffusedVertex(XMFLOAT3(-fx, +fy, -fz), cornerColor[0]);
	pVertices[9]  = CDiffusedVertex(XMFLOAT3(-fx, +fy, +fz), cornerColor[3]);
	pVertices[10] = CDiffusedVertex(XMFLOAT3(-fx, -fy, +fz), cornerColor[7]);
	pVertices[11] = CDiffusedVertex(XMFLOAT3(-fx, -fy, -fz), cornerColor[4]);
	for (int i = 8; i < 12; ++i) pNormals[i] = CNormalVertex(-1.0f, 0.0f, 0.0f);

	// ── +X 면 ─ 원본 코너 1,2,6,5
	pVertices[12] = CDiffusedVertex(XMFLOAT3(+fx, +fy, -fz), cornerColor[1]);
	pVertices[13] = CDiffusedVertex(XMFLOAT3(+fx, +fy, +fz), cornerColor[2]);
	pVertices[14] = CDiffusedVertex(XMFLOAT3(+fx, -fy, +fz), cornerColor[6]);
	pVertices[15] = CDiffusedVertex(XMFLOAT3(+fx, -fy, -fz), cornerColor[5]);
	for (int i = 12; i < 16; ++i) pNormals[i] = CNormalVertex(+1.0f, 0.0f, 0.0f);

	// ── +Z 면 ─ 원본 코너 3,2,6,7
	pVertices[16] = CDiffusedVertex(XMFLOAT3(-fx, +fy, +fz), cornerColor[3]);
	pVertices[17] = CDiffusedVertex(XMFLOAT3(+fx, +fy, +fz), cornerColor[2]);
	pVertices[18] = CDiffusedVertex(XMFLOAT3(+fx, -fy, +fz), cornerColor[6]);
	pVertices[19] = CDiffusedVertex(XMFLOAT3(-fx, -fy, +fz), cornerColor[7]);
	for (int i = 16; i < 20; ++i) pNormals[i] = CNormalVertex(0.0f, 0.0f, +1.0f);

	// ── -Y 면 (바닥) ─ 원본 코너 4,5,6,7
	pVertices[20] = CDiffusedVertex(XMFLOAT3(-fx, -fy, -fz), cornerColor[4]);
	pVertices[21] = CDiffusedVertex(XMFLOAT3(+fx, -fy, -fz), cornerColor[5]);
	pVertices[22] = CDiffusedVertex(XMFLOAT3(+fx, -fy, +fz), cornerColor[6]);
	pVertices[23] = CDiffusedVertex(XMFLOAT3(-fx, -fy, +fz), cornerColor[7]);
	for (int i = 20; i < 24; ++i) pNormals[i] = CNormalVertex(0.0f, -1.0f, 0.0f);

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
	즉 36개의 인덱스를 가짐(6 * 2 * 3).
	각 면 4 정점을 별도 슬롯으로 사용하므로 인덱스는 새로운 24 정점 인덱싱을 사용.
	winding order(시계방향=front, FrontCounterClockwise=FALSE) 는 기존과 동일하게 보존.
	*/
	m_nIndices = 36;

	UINT pnIndices[36] = {
		// 윗면(+Y): 새 정점 0..3 — 원본 (3,1,0)/(2,1,3) 매핑 유지
		3, 1, 0,   2, 1, 3,
		// -Z 면: 새 정점 4..7 — 원본 (0,5,4)/(1,5,0) → (4,6,7)/(5,6,4)
		4, 6, 7,   5, 6, 4,
		// -X 면: 새 정점 8..11 — 원본 (3,4,7)/(0,4,3) → (9,11,10)/(8,11,9)
		9, 11, 10, 8, 11, 9,
		// +X 면: 새 정점 12..15 — 원본 (1,6,5)/(2,6,1) → (12,14,15)/(13,14,12)
		12, 14, 15, 13, 14, 12,
		// +Z 면: 새 정점 16..19 — 원본 (2,7,6)/(3,7,2) → (17,19,18)/(16,19,17)
		17, 19, 18, 16, 19, 17,
		// -Y 면(바닥): 새 정점 20..23 — 원본 (6,4,5)/(7,4,6) → (22,20,21)/(23,20,22)
		22, 20, 21, 23, 20, 22,
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

	// 노멀 병렬 버퍼 생성 (slot 1). 24 정점 전부 위에서 면별로 설정 완료.
	const UINT normalStride = sizeof(CNormalVertex);
	m_pd3dNormalBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		pNormals, normalStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dNormalUploadBuffer);

	m_d3dNormalBufferView.BufferLocation = m_pd3dNormalBuffer->GetGPUVirtualAddress();
	m_d3dNormalBufferView.StrideInBytes  = normalStride;
	m_d3dNormalBufferView.SizeInBytes    = normalStride * m_nVertices;
	m_bHasNormals = true;
}

CCubeMeshDiffused::~CCubeMeshDiffused()
{
}


// ====================================================================================
// [Claude] CMergedCubeMesh — 다수 직육면체를 단일 메시로 통합
// ====================================================================================
// 각 큐브의 정점을 미리 월드 좌표로 변환해 단일 정점/인덱스/노멀 버퍼로 합친다.
// [Claude 수정] 면 단위 평탄 음영을 위해 큐브당 정점을 8 → 24 로 분리. 각 면의 4 정점은
//   동일한 위치를 공유하지만 노멀이 그 면의 축 법선(±X / ±Y / ±Z) 으로 고정된다.
//   N 개의 큐브 = 24N 정점 + 36N 인덱스 + 24N 노멀, 단 1 회의 DrawIndexedInstanced 로 렌더.
CMergedCubeMesh::CMergedCubeMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	const std::vector<Cube>& cubes)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	const size_t nCubes = cubes.size();
	m_nVertices = static_cast<UINT>(nCubes * 24);
	m_nIndices  = static_cast<UINT>(nCubes * 36);

	if (nCubes == 0) {
		// 빈 메시여도 Render 호출 시 안전하도록 dummy 버퍼라도 만들어두지는 않는다 —
		// CMesh::Render 가 m_pd3dVertexBuffer 가 없으면 IASetVertexBuffers 에서 크래시.
		// 호출부에서 nCubes > 0 인 경우에만 인스턴스화해야 한다.
		return;
	}

	// 큐브당 24 정점 인덱스 패턴 (CCubeMeshDiffused 의 새 인덱스와 동일). 면별 4 정점이
	// 분리되어 있으므로 새 인덱싱(0..23) 을 사용하고, 큐브 i 의 정점 베이스는 i*24.
	const UINT kBaseIndices[36] = {
		// 윗면(+Y): 0..3
		3, 1, 0,   2, 1, 3,
		// -Z 면: 4..7
		4, 6, 7,   5, 6, 4,
		// -X 면: 8..11
		9, 11, 10, 8, 11, 9,
		// +X 면: 12..15
		12, 14, 15, 13, 14, 12,
		// +Z 면: 16..19
		17, 19, 18, 16, 19, 17,
		// -Y 면(바닥): 20..23
		22, 20, 21, 23, 20, 22,
	};

	// 큐브당 24 정점의 노멀. 각 면의 4 정점은 동일한 면 법선을 공유한다.
	const XMFLOAT3 kBaseNormals[24] = {
		// 윗면(+Y) — 4 정점
		{ 0.0f, +1.0f,  0.0f }, { 0.0f, +1.0f,  0.0f }, { 0.0f, +1.0f,  0.0f }, { 0.0f, +1.0f,  0.0f },
		// -Z 면
		{ 0.0f,  0.0f, -1.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f,  0.0f, -1.0f },
		// -X 면
		{-1.0f,  0.0f,  0.0f }, {-1.0f,  0.0f,  0.0f }, {-1.0f,  0.0f,  0.0f }, {-1.0f,  0.0f,  0.0f },
		// +X 면
		{+1.0f,  0.0f,  0.0f }, {+1.0f,  0.0f,  0.0f }, {+1.0f,  0.0f,  0.0f }, {+1.0f,  0.0f,  0.0f },
		// +Z 면
		{ 0.0f,  0.0f, +1.0f }, { 0.0f,  0.0f, +1.0f }, { 0.0f,  0.0f, +1.0f }, { 0.0f,  0.0f, +1.0f },
		// -Y 면(바닥)
		{ 0.0f, -1.0f,  0.0f }, { 0.0f, -1.0f,  0.0f }, { 0.0f, -1.0f,  0.0f }, { 0.0f, -1.0f,  0.0f },
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

		// 원본 8 코너 좌표를 미리 계산해 24 정점 슬롯에 매핑한다.
		const XMFLOAT3 P0(cx - fx, cy + fy, cz - fz);  // 좌상앞
		const XMFLOAT3 P1(cx + fx, cy + fy, cz - fz);  // 우상앞
		const XMFLOAT3 P2(cx + fx, cy + fy, cz + fz);  // 우상뒤
		const XMFLOAT3 P3(cx - fx, cy + fy, cz + fz);  // 좌상뒤
		const XMFLOAT3 P4(cx - fx, cy - fy, cz - fz);  // 좌하앞
		const XMFLOAT3 P5(cx + fx, cy - fy, cz - fz);  // 우하앞
		const XMFLOAT3 P6(cx + fx, cy - fy, cz + fz);  // 우하뒤
		const XMFLOAT3 P7(cx - fx, cy - fy, cz + fz);  // 좌하뒤

		// 윗면(+Y) — 코너 0,1,2,3
		vertices.emplace_back(P0, c.color);
		vertices.emplace_back(P1, c.color);
		vertices.emplace_back(P2, c.color);
		vertices.emplace_back(P3, c.color);
		// -Z 면 — 코너 0,1,5,4
		vertices.emplace_back(P0, c.color);
		vertices.emplace_back(P1, c.color);
		vertices.emplace_back(P5, c.color);
		vertices.emplace_back(P4, c.color);
		// -X 면 — 코너 0,3,7,4
		vertices.emplace_back(P0, c.color);
		vertices.emplace_back(P3, c.color);
		vertices.emplace_back(P7, c.color);
		vertices.emplace_back(P4, c.color);
		// +X 면 — 코너 1,2,6,5
		vertices.emplace_back(P1, c.color);
		vertices.emplace_back(P2, c.color);
		vertices.emplace_back(P6, c.color);
		vertices.emplace_back(P5, c.color);
		// +Z 면 — 코너 3,2,6,7
		vertices.emplace_back(P3, c.color);
		vertices.emplace_back(P2, c.color);
		vertices.emplace_back(P6, c.color);
		vertices.emplace_back(P7, c.color);
		// -Y 면(바닥) — 코너 4,5,6,7
		vertices.emplace_back(P4, c.color);
		vertices.emplace_back(P5, c.color);
		vertices.emplace_back(P6, c.color);
		vertices.emplace_back(P7, c.color);

		for (int j = 0; j < 24; ++j) normals.emplace_back(kBaseNormals[j]);

		const UINT base = static_cast<UINT>(i * 24);
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
// [Claude] CObjMesh — Wavefront .obj 파일 로더
// ====================================================================================
// Resources/*.obj 같은 외부 메시 파일을 런타임에 읽어 정점/노멀/인덱스 버퍼를 채운다.
// 텍스처가 없는 파이프라인이므로 색상은 (1) o 그룹 이름 → 내장 부품 색상 테이블,
// (2) 일치 없으면 호출자가 넘긴 폴백 색으로 결정한다. 면 단위 평탄 음영을 위해
// 페이스마다 정점을 새로 생성한다(공유 X).
namespace {

// 대소문자 무시 비교용 — std::tolower 는 char 가 음수일 때 UB 이므로 unsigned char 캐스트.
inline char ToLowerASCII(char c) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool EqualsIgnoreCase(const std::string& a, const char* b) {
	const size_t la = a.size();
	for (size_t i = 0; i < la; ++i) {
		if (b[i] == '\0') return false;
		if (ToLowerASCII(a[i]) != ToLowerASCII(b[i])) return false;
	}
	return b[la] == '\0';
}

// 부품 이름 → 색상. 일치하는 항목이 없으면 fallback 반환.
// (대소문자 무시) — Blender export 의 o 그룹 이름과 매칭한다.
XMFLOAT4 ResolveGunPartColor(const std::string& sName, XMFLOAT4 fallback) {
	// 나무 개머리판
	if (EqualsIgnoreCase(sName, "Stock"))        return XMFLOAT4(0.35f, 0.22f, 0.12f, 1.0f);
	// 짙은 메탈 매거진
	if (EqualsIgnoreCase(sName, "Mag"))          return XMFLOAT4(0.12f, 0.12f, 0.13f, 1.0f);
	if (EqualsIgnoreCase(sName, "Magazine"))     return XMFLOAT4(0.12f, 0.12f, 0.13f, 1.0f);
	// 검은 폴리머 — 트리거/그립/안전장치
	if (EqualsIgnoreCase(sName, "Trigger"))      return XMFLOAT4(0.10f, 0.08f, 0.08f, 1.0f);
	if (EqualsIgnoreCase(sName, "Grip"))         return XMFLOAT4(0.10f, 0.08f, 0.08f, 1.0f);
	if (EqualsIgnoreCase(sName, "Safety"))       return XMFLOAT4(0.10f, 0.08f, 0.08f, 1.0f);
	// 블루드 강철 — 조준기
	if (EqualsIgnoreCase(sName, "IronSight"))    return XMFLOAT4(0.10f, 0.10f, 0.12f, 1.0f);
	if (EqualsIgnoreCase(sName, "Sight"))        return XMFLOAT4(0.10f, 0.10f, 0.12f, 1.0f);
	// 액센트 회색 — 노리쇠/장전 손잡이/전진기
	if (EqualsIgnoreCase(sName, "ChargingHandle")) return XMFLOAT4(0.20f, 0.20f, 0.22f, 1.0f);
	if (EqualsIgnoreCase(sName, "Bolt"))         return XMFLOAT4(0.20f, 0.20f, 0.22f, 1.0f);
	if (EqualsIgnoreCase(sName, "ForwardAssist")) return XMFLOAT4(0.20f, 0.20f, 0.22f, 1.0f);
	// 그 외 — 호출자 폴백 (Front, Base, Body 등 메인 바디 부품)
	return fallback;
}

// "v/vt/vn" / "v//vn" / "v/vt" / "v" 형식의 페이스 토큰에서 (정점 idx, 노멀 idx) 추출.
// idx 는 1-based 원본 그대로 반환(0 = 없음/생략). 음수(상대 인덱스) 도 그대로 전달.
struct FaceToken { int v; int n; };
FaceToken ParseFaceToken(const std::string& tok) {
	FaceToken r{ 0, 0 };
	const char* s = tok.c_str();
	const char* end = s + tok.size();

	// 첫 슬래시 전까지 = v
	const char* p = s;
	while (p < end && *p != '/') ++p;
	if (p > s) r.v = std::atoi(std::string(s, p).c_str());
	if (p >= end) return r;

	// 첫 슬래시 다음, 두 번째 슬래시 전까지 = vt (사용 안 함). v//vn 이면 빈 문자열.
	++p; // skip first '/'
	const char* p2 = p;
	while (p2 < end && *p2 != '/') ++p2;
	// vt 무시
	if (p2 >= end) return r; // v/vt 형식 (vn 없음)

	// 두 번째 슬래시 다음 = vn
	++p2;
	if (p2 < end) r.n = std::atoi(std::string(p2, end).c_str());
	return r;
}

} // namespace

CObjMesh::CObjMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	const std::wstring& wsObjPath,
	XMFLOAT4 xmf4FallbackColor,
	const XMFLOAT4X4& xmf4x4ModelTransform)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// ─────────────────────────────────────────────────────────────────────────
	// 1) 파일 로드. 실패 시 빈 메시로 두고 종료(렌더 안전).
	std::ifstream ifs(wsObjPath);
	if (!ifs.is_open()) {
		OutputDebugStringW(L"[CObjMesh] failed to open: ");
		OutputDebugStringW(wsObjPath.c_str());
		OutputDebugStringW(L"\n");
		return;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 2) 라인 파싱: v/vn/f/o 만 의미 있게 처리, 나머지는 무시.
	std::vector<XMFLOAT3> positions;
	std::vector<XMFLOAT3> normals;

	// 각 페이스의 (정점/노멀) 인덱스 — fan triangulation 결과를 모은다.
	// 인덱스는 1-based 원본 값. 음수는 상대 인덱스로 처리.
	// 또한 페이스마다 현재 o 그룹 색상을 함께 기록한다.
	struct Tri { FaceToken a, b, c; XMFLOAT4 color; };
	std::vector<Tri> tris;
	tris.reserve(1024);

	XMFLOAT4 currentColor = xmf4FallbackColor;
	std::string line;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
		// 줄 끝의 \r 제거(Windows export 호환).
		while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
		if (line.empty()) continue;
		if (line[0] == '#') continue;

		std::istringstream iss(line);
		std::string tag;
		iss >> tag;
		if (tag == "v") {
			XMFLOAT3 p;
			iss >> p.x >> p.y >> p.z;
			positions.push_back(p);
		}
		else if (tag == "vn") {
			XMFLOAT3 n;
			iss >> n.x >> n.y >> n.z;
			normals.push_back(n);
		}
		else if (tag == "o") {
			std::string name;
			iss >> name;
			currentColor = ResolveGunPartColor(name, xmf4FallbackColor);
		}
		else if (tag == "f") {
			// 페이스 정점들을 모두 읽고 fan triangulation.
			std::vector<FaceToken> verts;
			std::string tok;
			while (iss >> tok) verts.push_back(ParseFaceToken(tok));
			if (verts.size() < 3) continue;
			for (size_t i = 1; i + 1 < verts.size(); ++i) {
				tris.push_back({ verts[0], verts[i], verts[i + 1], currentColor });
			}
		}
		// vt, s, g, usemtl, mtllib 는 무시.
	}

	if (tris.empty() || positions.empty()) {
		OutputDebugStringW(L"[CObjMesh] empty mesh after parsing\n");
		return;
	}

	// ─────────────────────────────────────────────────────────────────────────
	// 3) 모델 변환 행렬을 로드. 위치는 TransformCoord, 노멀은 TransformNormal 후 정규화.
	const XMMATRIX mXform = XMLoadFloat4x4(&xmf4x4ModelTransform);

	// 1-based 인덱스 → 0-based 절대 인덱스. 음수는 (크기 + idx) 로 변환.
	auto ResolveVertIdx = [&](int idx) -> int {
		if (idx > 0) return idx - 1;
		if (idx < 0) return static_cast<int>(positions.size()) + idx;
		return -1;
	};
	auto ResolveNormIdx = [&](int idx) -> int {
		if (idx > 0) return idx - 1;
		if (idx < 0) return static_cast<int>(normals.size()) + idx;
		return -1;
	};

	// ─────────────────────────────────────────────────────────────────────────
	// 4) 삼각형마다 정점 3 개를 새로 만든다(공유 X → flat shading 보장).
	std::vector<CDiffusedVertex> vertices;
	std::vector<CNormalVertex>   meshNormals;
	std::vector<UINT>            indices;
	vertices.reserve(tris.size() * 3);
	meshNormals.reserve(tris.size() * 3);
	indices.reserve(tris.size() * 3);

	for (const Tri& t : tris) {
		const int ia = ResolveVertIdx(t.a.v);
		const int ib = ResolveVertIdx(t.b.v);
		const int ic = ResolveVertIdx(t.c.v);
		if (ia < 0 || ib < 0 || ic < 0) continue;
		if (ia >= (int)positions.size() || ib >= (int)positions.size() || ic >= (int)positions.size()) continue;

		// 위치를 변환 행렬로 베이크.
		const XMVECTOR vA = XMVector3TransformCoord(XMLoadFloat3(&positions[ia]), mXform);
		const XMVECTOR vB = XMVector3TransformCoord(XMLoadFloat3(&positions[ib]), mXform);
		const XMVECTOR vC = XMVector3TransformCoord(XMLoadFloat3(&positions[ic]), mXform);
		XMFLOAT3 pa, pb, pc;
		XMStoreFloat3(&pa, vA);
		XMStoreFloat3(&pb, vB);
		XMStoreFloat3(&pc, vC);

		// 노멀 결정: OBJ 의 vn 이 있으면 그것을, 없으면 면 노멀 계산.
		XMVECTOR vN;
		const int na = ResolveNormIdx(t.a.n);
		const int nb = ResolveNormIdx(t.b.n);
		const int nc = ResolveNormIdx(t.c.n);
		if (na >= 0 && nb >= 0 && nc >= 0
			&& na < (int)normals.size() && nb < (int)normals.size() && nc < (int)normals.size()) {
			// 평탄 음영 — 세 노멀 평균을 면 노멀로 사용한다(같은 값이면 그대로).
			XMVECTOR vNa = XMLoadFloat3(&normals[na]);
			XMVECTOR vNb = XMLoadFloat3(&normals[nb]);
			XMVECTOR vNc = XMLoadFloat3(&normals[nc]);
			vN = XMVectorAdd(XMVectorAdd(vNa, vNb), vNc);
			vN = XMVector3TransformNormal(vN, mXform);
			vN = XMVector3Normalize(vN);
		}
		else {
			// vn 누락 — (B - A) × (C - A) 로 면 노멀 계산(이미 베이크된 좌표 기준).
			const XMVECTOR vAB = XMVectorSubtract(vB, vA);
			const XMVECTOR vAC = XMVectorSubtract(vC, vA);
			vN = XMVector3Normalize(XMVector3Cross(vAB, vAC));
		}
		XMFLOAT3 n3;
		XMStoreFloat3(&n3, vN);

		const UINT base = static_cast<UINT>(vertices.size());
		vertices.emplace_back(pa, t.color);
		vertices.emplace_back(pb, t.color);
		vertices.emplace_back(pc, t.color);
		meshNormals.emplace_back(n3);
		meshNormals.emplace_back(n3);
		meshNormals.emplace_back(n3);
		indices.push_back(base + 0);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
	}

	m_nVertices = static_cast<UINT>(vertices.size());
	m_nIndices  = static_cast<UINT>(indices.size());
	if (m_nVertices == 0 || m_nIndices == 0) return;

	// ─────────────────────────────────────────────────────────────────────────
	// 5) GPU 버퍼 생성 — CCubeMeshDiffused/CMergedCubeMesh 와 동일 패턴.
	// 정점 버퍼 (slot 0)
	m_pd3dVertexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		vertices.data(), m_nStride * m_nVertices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&m_pd3dVertexUploadBuffer);
	m_d3dVertexBufferView.BufferLocation = m_pd3dVertexBuffer->GetGPUVirtualAddress();
	m_d3dVertexBufferView.StrideInBytes  = m_nStride;
	m_d3dVertexBufferView.SizeInBytes    = m_nStride * m_nVertices;

	// 인덱스 버퍼
	m_pd3dIndexBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		indices.data(), sizeof(UINT) * m_nIndices,
		D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&m_pd3dIndexUploadBuffer);
	m_d3dIndexBufferView.BufferLocation = m_pd3dIndexBuffer->GetGPUVirtualAddress();
	m_d3dIndexBufferView.Format         = DXGI_FORMAT_R32_UINT;
	m_d3dIndexBufferView.SizeInBytes    = sizeof(UINT) * m_nIndices;

	// 노멀 버퍼 (slot 1)
	const UINT normalStride = sizeof(CNormalVertex);
	m_pd3dNormalBuffer = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList,
		meshNormals.data(), normalStride * m_nVertices,
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