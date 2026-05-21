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
};

void CMesh::Render(ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 메쉬의 프리미티브 유형을 설정한다.
	pd3dCommandList->IASetPrimitiveTopology(m_d3dPrimitiveTopology);

	// 메쉬의 정점 버퍼 뷰를 설정한다.
	pd3dCommandList->IASetVertexBuffers(m_nSlot, 1, &m_d3dVertexBufferView);

	// 인덱스 버퍼가 있으면 파이프라인에 연결하고 렌더링
	if (m_pd3dIndexBuffer) {
		pd3dCommandList->IASetIndexBuffer(&m_d3dIndexBufferView);
		pd3dCommandList->DrawIndexedInstanced(m_nIndices, 1, 0, 0, 0);
	}
	else {
		pd3dCommandList->DrawInstanced(m_nVertices, 1, m_nOffset, 0);
	}
}

void CMesh::ComputeFaceNormalsFromIndexed(
	const XMFLOAT3* pPositions, const UINT* pnIndices, UINT nIndices)
{
	const UINT nTris = nIndices / 3;
	m_vFaceNormals.resize(nTris);
	for (UINT t = 0; t < nTris; ++t) {
		const XMFLOAT3& p0 = pPositions[pnIndices[t * 3 + 0]];
		const XMFLOAT3& p1 = pPositions[pnIndices[t * 3 + 1]];
		const XMFLOAT3& p2 = pPositions[pnIndices[t * 3 + 2]];
		XMVECTOR v0 = XMLoadFloat3(&p0);
		XMVECTOR v1 = XMLoadFloat3(&p1);
		XMVECTOR v2 = XMLoadFloat3(&p2);
		// 외향 노멀: (p1-p0) x (p2-p0). 큐브 인덱스 와인딩이 외향 normal 을
		// 만드는지는 시각 검증 필요. 만일 안쪽으로 향하면 cross 인자 순서를 바꿀 것.
		XMVECTOR n = XMVector3Normalize(XMVector3Cross(
			XMVectorSubtract(v1, v0), XMVectorSubtract(v2, v0)));
		XMStoreFloat3(&m_vFaceNormals[t], n);
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

	// face normal 캐시 — 픽셀 셰이더가 SV_PrimitiveID 로 조회한다.
	XMFLOAT3 positions[8];
	for (int i = 0; i < 8; ++i) positions[i] = pVertices[i].GetPosition();
	ComputeFaceNormalsFromIndexed(positions, pnIndices, m_nIndices);
}

CCubeMeshDiffused::~CCubeMeshDiffused()
{
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