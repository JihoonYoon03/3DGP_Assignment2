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

	// 노멀 업로드 버퍼 해제
	if (m_pd3dNormalUploadBuffer)
		m_pd3dNormalUploadBuffer.Reset();
};

void CMesh::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 메쉬의 프리미티브 유형을 설정한다.
	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);

	// 위치/색 정점 버퍼(slot 0) 바인딩.
	pd3dCommandList->IASetVertexBuffers(m_nSlot, 1, &m_d3dVertexBufferView);
	// 에러 수정 필요

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
	// 삼각형 메쉬를 정의한다.
	m_nVertices = 3;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

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
	// 면 단위 음영을 위해 정점을 8개에서 24개로
	m_nVertices = 24;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	float fx = fWidth * 0.5f, fy = fHeight * 0.5f, fz = fDepth * 0.5f;

	XMFLOAT4 cornerColor[8];
	for (int i = 0; i < 8; ++i) cornerColor[i] = bUseUniformColor ? xmf4Color : RANDOM_COLOR;

	CDiffusedVertex pVertices[24];
	CNormalVertex   pNormals[24];

	// 윗면
	pVertices[0] = CDiffusedVertex(XMFLOAT3(-fx, +fy, -fz), cornerColor[0]);
	pVertices[1] = CDiffusedVertex(XMFLOAT3(+fx, +fy, -fz), cornerColor[1]);
	pVertices[2] = CDiffusedVertex(XMFLOAT3(+fx, +fy, +fz), cornerColor[2]);
	pVertices[3] = CDiffusedVertex(XMFLOAT3(-fx, +fy, +fz), cornerColor[3]);
	for (int i = 0; i < 4; ++i) pNormals[i] = CNormalVertex(0.0f, +1.0f, 0.0f);

	// 앞면 ─ 원본 코너 0,1,5,4
	pVertices[4] = CDiffusedVertex(XMFLOAT3(-fx, +fy, -fz), cornerColor[0]);
	pVertices[5] = CDiffusedVertex(XMFLOAT3(+fx, +fy, -fz), cornerColor[1]);
	pVertices[6] = CDiffusedVertex(XMFLOAT3(+fx, -fy, -fz), cornerColor[5]);
	pVertices[7] = CDiffusedVertex(XMFLOAT3(-fx, -fy, -fz), cornerColor[4]);
	for (int i = 4; i < 8; ++i) pNormals[i] = CNormalVertex(0.0f, 0.0f, -1.0f);

	// 왼쪽면
	pVertices[8]  = CDiffusedVertex(XMFLOAT3(-fx, +fy, -fz), cornerColor[0]);
	pVertices[9]  = CDiffusedVertex(XMFLOAT3(-fx, +fy, +fz), cornerColor[3]);
	pVertices[10] = CDiffusedVertex(XMFLOAT3(-fx, -fy, +fz), cornerColor[7]);
	pVertices[11] = CDiffusedVertex(XMFLOAT3(-fx, -fy, -fz), cornerColor[4]);
	for (int i = 8; i < 12; ++i) pNormals[i] = CNormalVertex(-1.0f, 0.0f, 0.0f);

	// 오른쪽면
	pVertices[12] = CDiffusedVertex(XMFLOAT3(+fx, +fy, -fz), cornerColor[1]);
	pVertices[13] = CDiffusedVertex(XMFLOAT3(+fx, +fy, +fz), cornerColor[2]);
	pVertices[14] = CDiffusedVertex(XMFLOAT3(+fx, -fy, +fz), cornerColor[6]);
	pVertices[15] = CDiffusedVertex(XMFLOAT3(+fx, -fy, -fz), cornerColor[5]);
	for (int i = 12; i < 16; ++i) pNormals[i] = CNormalVertex(+1.0f, 0.0f, 0.0f);

	// 뒷면
	pVertices[16] = CDiffusedVertex(XMFLOAT3(-fx, +fy, +fz), cornerColor[3]);
	pVertices[17] = CDiffusedVertex(XMFLOAT3(+fx, +fy, +fz), cornerColor[2]);
	pVertices[18] = CDiffusedVertex(XMFLOAT3(+fx, -fy, +fz), cornerColor[6]);
	pVertices[19] = CDiffusedVertex(XMFLOAT3(-fx, -fy, +fz), cornerColor[7]);
	for (int i = 16; i < 20; ++i) pNormals[i] = CNormalVertex(0.0f, 0.0f, +1.0f);

	// 아랫면
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
	*/
	m_nIndices = 36;

	UINT pnIndices[36] = {
		// 윗면
		3, 1, 0,   2, 1, 3,
		// 앞면
		4, 6, 7,   5, 6, 4,
		// 왼쪽면
		9, 11, 10, 8, 11, 9,
		// 오른쪽면
		12, 14, 15, 13, 14, 12,
		// 뒷면
		17, 19, 18, 16, 19, 17,
		// 바닥
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

	// 노멀 병렬 버퍼 생성
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


// CMergedCubeMesh: 여러 큐브를 단일 정점/인덱스/노멀 버퍼로 통합
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
		return;
	}

	const UINT kBaseIndices[36] = {
		// 윗면
		3, 1, 0,   2, 1, 3,
		// 앞면
		4, 6, 7,   5, 6, 4,
		// 왼쪽면
		9, 11, 10, 8, 11, 9,
		// 오른쪽면
		12, 14, 15, 13, 14, 12,
		// 뒷면
		17, 19, 18, 16, 19, 17,
		// 바닥
		22, 20, 21, 23, 20, 22,
	};

	// 각 면의 정점은 동일한 노멀을 공유한다.
	const XMFLOAT3 kBaseNormals[24] = {
		// 윗면
		{ 0.0f, +1.0f,  0.0f }, { 0.0f, +1.0f,  0.0f }, { 0.0f, +1.0f,  0.0f }, { 0.0f, +1.0f,  0.0f },
		// 앞면
		{ 0.0f,  0.0f, -1.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f,  0.0f, -1.0f }, { 0.0f,  0.0f, -1.0f },
		// 왼쪽면
		{-1.0f,  0.0f,  0.0f }, {-1.0f,  0.0f,  0.0f }, {-1.0f,  0.0f,  0.0f }, {-1.0f,  0.0f,  0.0f },
		// 오른쪽면
		{+1.0f,  0.0f,  0.0f }, {+1.0f,  0.0f,  0.0f }, {+1.0f,  0.0f,  0.0f }, {+1.0f,  0.0f,  0.0f },
		// 뒷면
		{ 0.0f,  0.0f, +1.0f }, { 0.0f,  0.0f, +1.0f }, { 0.0f,  0.0f, +1.0f }, { 0.0f,  0.0f, +1.0f },
		// 바닥
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

		const XMFLOAT3 P0(cx - fx, cy + fy, cz - fz);  // 좌상앞
		const XMFLOAT3 P1(cx + fx, cy + fy, cz - fz);  // 우상앞
		const XMFLOAT3 P2(cx + fx, cy + fy, cz + fz);  // 우상뒤
		const XMFLOAT3 P3(cx - fx, cy + fy, cz + fz);  // 좌상뒤
		const XMFLOAT3 P4(cx - fx, cy - fy, cz - fz);  // 좌하앞
		const XMFLOAT3 P5(cx + fx, cy - fy, cz - fz);  // 우하앞
		const XMFLOAT3 P6(cx + fx, cy - fy, cz + fz);  // 우하뒤
		const XMFLOAT3 P7(cx - fx, cy - fy, cz + fz);  // 좌하뒤

		// 윗면
		vertices.emplace_back(P0, c.color);
		vertices.emplace_back(P1, c.color);
		vertices.emplace_back(P2, c.color);
		vertices.emplace_back(P3, c.color);
		// 앞면
		vertices.emplace_back(P0, c.color);
		vertices.emplace_back(P1, c.color);
		vertices.emplace_back(P5, c.color);
		vertices.emplace_back(P4, c.color);
		// 왼쪽면
		vertices.emplace_back(P0, c.color);
		vertices.emplace_back(P3, c.color);
		vertices.emplace_back(P7, c.color);
		vertices.emplace_back(P4, c.color);
		// 오른쪽면
		vertices.emplace_back(P1, c.color);
		vertices.emplace_back(P2, c.color);
		vertices.emplace_back(P6, c.color);
		vertices.emplace_back(P5, c.color);
		// 뒷면
		vertices.emplace_back(P3, c.color);
		vertices.emplace_back(P2, c.color);
		vertices.emplace_back(P6, c.color);
		vertices.emplace_back(P7, c.color);
		// 바닥
		vertices.emplace_back(P4, c.color);
		vertices.emplace_back(P5, c.color);
		vertices.emplace_back(P6, c.color);
		vertices.emplace_back(P7, c.color);

		for (int j = 0; j < 24; ++j) normals.emplace_back(kBaseNormals[j]);

		const UINT base = static_cast<UINT>(i * 24);
		for (int j = 0; j < 36; ++j) indices.push_back(kBaseIndices[j] + base);
	}

	// 정점 버퍼
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

	// 노멀 버퍼
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


// CObjMesh: Wavefront .obj 파일 로드
namespace {

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

// 부품 이름별 색상 매핑 (블렌더에서 export시 그룹 이름 기준)
XMFLOAT4 ResolveGunPartColor(const std::string& sName, XMFLOAT4 fallback) {
	// 개머리판
	if (EqualsIgnoreCase(sName, "Stock"))        return XMFLOAT4(0.10f, 0.08f, 0.08f, 1.0f);
	// 탄창
	if (EqualsIgnoreCase(sName, "Mag"))          return XMFLOAT4(0.12f, 0.12f, 0.13f, 1.0f);
	if (EqualsIgnoreCase(sName, "Magazine"))     return XMFLOAT4(0.12f, 0.12f, 0.13f, 1.0f);
	// 방아쇠/그립/안전장치
	if (EqualsIgnoreCase(sName, "Trigger"))      return XMFLOAT4(0.10f, 0.08f, 0.08f, 1.0f);
	if (EqualsIgnoreCase(sName, "Grip"))         return XMFLOAT4(0.10f, 0.08f, 0.08f, 1.0f);
	if (EqualsIgnoreCase(sName, "Safety"))       return XMFLOAT4(0.10f, 0.08f, 0.08f, 1.0f);
	// 가늠자/쇠
	if (EqualsIgnoreCase(sName, "IronSight"))    return XMFLOAT4(0.10f, 0.10f, 0.12f, 1.0f);
	if (EqualsIgnoreCase(sName, "Sight"))        return XMFLOAT4(0.10f, 0.10f, 0.12f, 1.0f);
	// 노리쇠/장전손잡이/전진기
	if (EqualsIgnoreCase(sName, "ChargingHandle")) return XMFLOAT4(0.20f, 0.20f, 0.22f, 1.0f);
	if (EqualsIgnoreCase(sName, "Bolt"))         return XMFLOAT4(0.20f, 0.20f, 0.22f, 1.0f);
	if (EqualsIgnoreCase(sName, "ForwardAssist")) return XMFLOAT4(0.20f, 0.20f, 0.22f, 1.0f);
	// 그 외
	return fallback;
}

struct FaceToken { int v; int n; };
FaceToken ParseFaceToken(const std::string& tok) {
	FaceToken r{ 0, 0 };
	const char* s = tok.c_str();
	const char* end = s + tok.size();

	const char* p = s;
	while (p < end && *p != '/') ++p;
	if (p > s) r.v = std::atoi(std::string(s, p).c_str());
	if (p >= end) return r;

	++p;
	const char* p2 = p;
	while (p2 < end && *p2 != '/') ++p2;
	if (p2 >= end) return r;

	++p2;
	if (p2 < end) r.n = std::atoi(std::string(p2, end).c_str());
	return r;
}

}

CObjMesh::CObjMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	const std::wstring& wsObjPath,
	XMFLOAT4 xmf4FallbackColor,
	const XMFLOAT4X4& xmf4x4ModelTransform)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	std::ifstream ifs(wsObjPath);
	if (!ifs.is_open()) {
		OutputDebugStringW(L"[CObjMesh] failed to open: ");
		OutputDebugStringW(wsObjPath.c_str());
		OutputDebugStringW(L"\n");
		return;
	}

	std::vector<XMFLOAT3> positions;
	std::vector<XMFLOAT3> normals;

	struct Tri { FaceToken a, b, c; XMFLOAT4 color; };
	std::vector<Tri> tris;
	tris.reserve(1024);

	XMFLOAT4 currentColor = xmf4FallbackColor;
	std::string line;
	while (std::getline(ifs, line)) {
		if (line.empty()) continue;
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
			std::vector<FaceToken> verts;
			std::string tok;
			while (iss >> tok) verts.push_back(ParseFaceToken(tok));
			if (verts.size() < 3) continue;
			for (size_t i = 1; i + 1 < verts.size(); ++i) {
				tris.push_back({ verts[0], verts[i], verts[i + 1], currentColor });
			}
		}
	}

	if (tris.empty() || positions.empty()) {
		OutputDebugStringW(L"[CObjMesh] empty mesh after parsing\n");
		return;
	}

	const XMMATRIX mXform = XMLoadFloat4x4(&xmf4x4ModelTransform);

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

		const XMVECTOR vA = XMVector3TransformCoord(XMLoadFloat3(&positions[ia]), mXform);
		const XMVECTOR vB = XMVector3TransformCoord(XMLoadFloat3(&positions[ib]), mXform);
		const XMVECTOR vC = XMVector3TransformCoord(XMLoadFloat3(&positions[ic]), mXform);
		XMFLOAT3 pa, pb, pc;
		XMStoreFloat3(&pa, vA);
		XMStoreFloat3(&pb, vB);
		XMStoreFloat3(&pc, vC);

		XMVECTOR vN;
		const int na = ResolveNormIdx(t.a.n);
		const int nb = ResolveNormIdx(t.b.n);
		const int nc = ResolveNormIdx(t.c.n);
		if (na >= 0 && nb >= 0 && nc >= 0
			&& na < (int)normals.size() && nb < (int)normals.size() && nc < (int)normals.size()) {
			XMVECTOR vNa = XMLoadFloat3(&normals[na]);
			XMVECTOR vNb = XMLoadFloat3(&normals[nb]);
			XMVECTOR vNc = XMLoadFloat3(&normals[nc]);
			vN = XMVectorAdd(XMVectorAdd(vNa, vNb), vNc);
			vN = XMVector3TransformNormal(vN, mXform);
			vN = XMVector3Normalize(vN);
		}
		else {
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

	// 정점 버퍼
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

	// 노멀 버퍼
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
// ====================================================================================
CCrosshairMesh::CCrosshairMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	UINT nScreenWidth, UINT nScreenHeight,
	UINT nArmLengthPx, UINT nThicknessPx,
	XMFLOAT4 xmf4Color)
	: CMesh(pd3dDevice, pd3dCommandList)
{
	m_nVertices = 8;
	m_nStride = sizeof(CDiffusedVertex);
	m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	if (nScreenWidth == 0) nScreenWidth = 1;
	if (nScreenHeight == 0) nScreenHeight = 1;
	const float armX = float(nArmLengthPx) * 2.0f / float(nScreenWidth);
	const float armY = float(nArmLengthPx) * 2.0f / float(nScreenHeight);
	const float thickX = float(nThicknessPx) * 2.0f / float(nScreenWidth);
	const float thickY = float(nThicknessPx) * 2.0f / float(nScreenHeight);
	const float hx = armX;          
	const float vt = thickY * 0.5f; 
	const float vx = thickX * 0.5f; 
	const float vy = armY;          

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