#include "stdafx.h"
#include "Maps.h"
#include "Mesh.h"
#include "Scene.h"
#include <string>

namespace {
	// Wolfenstein 풍 미로의 기본 단위.
	constexpr float TILE = 4.0f;        // 한 셀의 가로/세로 크기
	constexpr float WALL_H = 8.0f;      // 천장까지 닿는 벽 높이
	constexpr float FLOOR_H = 0.2f;     // 바닥 두께
	constexpr float STEP_H = 0.7f;      // 계단 한 단의 높이
	constexpr float EYE_Y = 3.5f;       // 플레이어 눈높이 (1인칭 카메라 Y)

	// 한 개의 큐브 게임 오브젝트를 만들어 vector에 추가한다.
	void AddCube(
		ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		std::vector<std::shared_ptr<CGameObject>>& vObjects,
		XMFLOAT3 xmf3Position, XMFLOAT3 xmf3Size, XMFLOAT4 xmf4Color)
	{
		auto pMesh = std::make_shared<CCubeMeshDiffused>(
			pd3dDevice, pd3dCommandList,
			xmf3Size.x, xmf3Size.y, xmf3Size.z, true, xmf4Color);
		auto pObj = std::make_shared<CGameObject>();
		pObj->SetMesh(pMesh);
		pObj->SetPosition(xmf3Position);
		vObjects.push_back(std::move(pObj));
	}

	// 문자 그리드를 받아 미로 큐브를 배치한다.
	// 셀 문자 의미:
	//   'W' : 천장까지 닿는 벽
	//   '.' : 바닥만 있는 통로
	//   '1' ~ '9' : 해당 칸 위에 STEP_H * n 높이의 계단 단을 둠
	//   'P' : 계단 최상단으로 이어지는 단상 (STEP_H * 6 높이)
	// 그 외 문자는 바닥만 깐다.
	void BuildMazeFromGrid(
		ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		std::vector<std::shared_ptr<CGameObject>>& vObjects,
		const std::vector<std::string>& grid,
		XMFLOAT4 colorFloorA, XMFLOAT4 colorFloorB,
		XMFLOAT4 colorWallA,  XMFLOAT4 colorWallB,
		XMFLOAT4 colorStair,  XMFLOAT4 colorPlatform)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const float halfX = (cols - 1) * TILE * 0.5f;
		const float halfZ = (rows - 1) * TILE * 0.5f;

		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				char ch = grid[r][c];
				const float x = c * TILE - halfX;
				const float z = r * TILE - halfZ;
				const bool parity = ((r + c) % 2 == 0);

				// 모든 셀에 바닥 타일을 깐다 (체크무늬).
				const XMFLOAT4 fc = parity ? colorFloorA : colorFloorB;
				AddCube(pd3dDevice, pd3dCommandList, vObjects,
					XMFLOAT3(x, -FLOOR_H * 0.5f, z),
					XMFLOAT3(TILE, FLOOR_H, TILE), fc);

				if (ch == 'W') {
					// 천장까지 닿는 벽
					const XMFLOAT4 wc = parity ? colorWallA : colorWallB;
					AddCube(pd3dDevice, pd3dCommandList, vObjects,
						XMFLOAT3(x, WALL_H * 0.5f, z),
						XMFLOAT3(TILE, WALL_H, TILE), wc);
				}
				else if (ch >= '1' && ch <= '9') {
					// 점진적으로 높아지는 계단 단
					const int step = ch - '0';
					const float h = STEP_H * step;
					AddCube(pd3dDevice, pd3dCommandList, vObjects,
						XMFLOAT3(x, h * 0.5f, z),
						XMFLOAT3(TILE, h, TILE), colorStair);
				}
				else if (ch == 'P') {
					// 계단 끝의 단상
					const float h = STEP_H * 6.0f;
					AddCube(pd3dDevice, pd3dCommandList, vObjects,
						XMFLOAT3(x, h * 0.5f, z),
						XMFLOAT3(TILE, h, TILE), colorPlatform);
				}
				// 그 외 ('.')는 통로 - 바닥만으로 충분
			}
		}
	}

	// === 맵 1 미로 격자 (13 x 13) ===
	// 동쪽 끝의 코너 통로 (행 11)에 4단 계단과 단상이 놓인다.
	const std::vector<std::string>& Map1Grid()
	{
		static const std::vector<std::string> grid = {
			"WWWWWWWWWWWWW",
			"W...........W",
			"W.WWWWW.WWW.W",
			"W.....W.....W",
			"W.WWW.WWWWW.W",
			"W.W...W.....W",
			"W.W.WWW.WWW.W",
			"W...W.....W.W",
			"WWW.W.WWW.W.W",
			"W...W...W...W",
			"W.WWWWW.WWW.W",
			"W.....1234PWW",
			"WWWWWWWWWWWWW",
		};
		return grid;
	}

	// === 맵 2 미로 격자 (15 x 13) ===
	// 사방에서 중앙으로 모이는 십자형 통로. 남/북 두 진입로에 계단이 있고
	// 중앙 P가 가장 높은 단상이다.
	const std::vector<std::string>& Map2Grid()
	{
		static const std::vector<std::string> grid = {
			"WWWWWWWWWWWWWWW",
			"WWWWWW...WWWWWW",
			"WWWWWW...WWWWWW",
			"WWWWWW.1.WWWWWW",
			"WWW....2....WWW",
			"WWW....3....WWW",
			"WWW....P....WWW",
			"WWW....3....WWW",
			"WWW....2....WWW",
			"WWWWWW.1.WWWWWW",
			"WWWWWW...WWWWWW",
			"WWWWWW...WWWWWW",
			"WWWWWWWWWWWWWWW",
		};
		return grid;
	}
}

void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects)
{
	// 차가운 회청색 돌 미로. 어두운 바닥과 푸르스름한 석재 벽.
	BuildMazeFromGrid(pd3dDevice, pd3dCommandList, vObjects, Map1Grid(),
		XMFLOAT4(0.10f, 0.10f, 0.14f, 1.0f),  // floor A : 어두운 슬레이트
		XMFLOAT4(0.14f, 0.14f, 0.20f, 1.0f),  // floor B : 약간 밝은 슬레이트
		XMFLOAT4(0.42f, 0.46f, 0.56f, 1.0f),  // wall A  : 회청색 석재
		XMFLOAT4(0.32f, 0.36f, 0.46f, 1.0f),  // wall B  : 어두운 석재 (체크 교차)
		XMFLOAT4(0.68f, 0.55f, 0.32f, 1.0f),  // stair   : 사암 색
		XMFLOAT4(0.92f, 0.82f, 0.42f, 1.0f)); // platform: 금빛 강조
}

void BuildMap2Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects)
{
	// 따뜻한 갈색 벽돌 던전. 횃불에 비친 듯한 색감.
	BuildMazeFromGrid(pd3dDevice, pd3dCommandList, vObjects, Map2Grid(),
		XMFLOAT4(0.18f, 0.12f, 0.07f, 1.0f),  // floor A : 짙은 흙색
		XMFLOAT4(0.26f, 0.18f, 0.10f, 1.0f),  // floor B : 약간 밝은 흙색
		XMFLOAT4(0.55f, 0.32f, 0.18f, 1.0f),  // wall A  : 적벽돌
		XMFLOAT4(0.42f, 0.24f, 0.14f, 1.0f),  // wall B  : 어두운 벽돌
		XMFLOAT4(0.78f, 0.50f, 0.22f, 1.0f),  // stair   : 따뜻한 오렌지
		XMFLOAT4(1.00f, 0.78f, 0.32f, 1.0f)); // platform: 횃불 노랑
}

MapInfo GetMap1Info()
{
	// 미로 남서쪽 코너(행 11, 열 1)에서 동쪽으로 계단 끝의 단상을 바라본다.
	// 격자: 13 x 13, halfX = halfZ = 24
	MapInfo info;
	info.cameraPosition = XMFLOAT3(1.0f * TILE - 24.0f, EYE_Y, 11.0f * TILE - 24.0f);   // (-20, 3.5, 20)
	info.cameraLookAt   = XMFLOAT3(11.0f * TILE - 24.0f, EYE_Y - 0.4f, 11.0f * TILE - 24.0f); // (20, 3.1, 20)
	return info;
}

MapInfo GetMap2Info()
{
	// 북쪽 진입로(행 1, 열 7)에서 남쪽으로 십자 중앙의 단상을 바라본다.
	// 격자: 15 x 13, halfX = 28, halfZ = 24
	MapInfo info;
	info.cameraPosition = XMFLOAT3(7.0f * TILE - 28.0f, EYE_Y, 1.0f * TILE - 24.0f);    // (0, 3.5, -20)
	info.cameraLookAt   = XMFLOAT3(7.0f * TILE - 28.0f, EYE_Y - 0.6f, 6.0f * TILE - 24.0f); // (0, 2.9, 0)
	return info;
}
// ===================== ?浹 ???? =====================
// ?÷?????? ???(fFeetY) ??????? (x,z) ????? ?????? ????? ????????? ?????.
// W(o??? ??): ??? ????.
// 1~9 (??? ???) / P (???? ???): ??? ????? fFeetY + ????????? ?????? ????.
// .(????): ???? ??´?.
// ????? ??? ????? ??(±halfX, ±halfZ) ????? o?? ????? ??? ???? ????? ??????.
namespace {
	constexpr float STEP_UP_TOLERANCE = STEP_H + 0.05f;

	bool IsBlockedInGrid(const std::vector<std::string>& grid, float x, float z, float fFeetY)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const float halfX = (cols - 1) * TILE * 0.5f;
		const float halfZ = (rows - 1) * TILE * 0.5f;

		// (x,z) -> (c,r) ???. ??? ????? ????? ??????? 0.5f?? ???? ??ø? ???.
		const int c = static_cast<int>(floorf((x + halfX) / TILE + 0.5f));
		const int r = static_cast<int>(floorf((z + halfZ) / TILE + 0.5f));

		// ????? ????? ????? ?????? ??????? ????.
		if (c < 0 || c >= cols || r < 0 || r >= rows) return true;

		char ch = grid[r][c];
		if (ch == 'W') return true;
		if (ch >= '1' && ch <= '9') {
			const float topY = STEP_H * (ch - '0');
			return (topY > fFeetY + STEP_UP_TOLERANCE);
		}
		if (ch == 'P') {
			const float topY = STEP_H * 6.0f;
			return (topY > fFeetY + STEP_UP_TOLERANCE);
		}
		return false; // '.' ??? ??? ???? ???
	}
}

bool IsBlockedInMap(SceneState state, float x, float z, float fFeetY)
{
	switch (state) {
	case SceneState::MAP1: return IsBlockedInGrid(Map1Grid(), x, z, fFeetY);
	case SceneState::MAP2: return IsBlockedInGrid(Map2Grid(), x, z, fFeetY);
	default: return false; // ???? ??????? ?浹?? o?????? ??´?.
	}
}