#include "stdafx.h"
#include "Maps.h"
#include "Mesh.h"
#include "Scene.h"
#include <string>
#include <random>
#include <queue>
#include <algorithm>
#include <functional>

namespace {
	// 미로 기본 단위
	constexpr float TILE = 4.0f;
	constexpr float WALL_H = 8.0f;
	constexpr float FLOOR_H = 0.2f;
	constexpr float STEP_H = 0.7f;

	// 큐브 하나를 만들어 벡터에 추가
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

	// W : 벽
	// . : 평면 바닥
	// 1 ~ 3 : STEP_H * n 높이의 단차 바닥
	// 하나의 통합 메시로 합쳐서 최적화.
	void BuildMazeFromGrid(
		ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		std::vector<std::shared_ptr<CGameObject>>& vObjects,
		const std::vector<std::string>& grid,
		const std::vector<std::vector<float>>& wallHeights,
		XMFLOAT4 colorFloorA, XMFLOAT4 colorFloorB,
		XMFLOAT4 colorWallA,  XMFLOAT4 colorWallB,
		XMFLOAT4 colorStair,  XMFLOAT4 colorPlatform)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const float halfX = (cols - 1) * TILE * 0.5f;
		const float halfZ = (rows - 1) * TILE * 0.5f;

		auto lerp4 = [](const XMFLOAT4& a, const XMFLOAT4& b, float t) {
			XMFLOAT4 r;
			r.x = a.x * (1.0f - t) + b.x * t;
			r.y = a.y * (1.0f - t) + b.y * t;
			r.z = a.z * (1.0f - t) + b.z * t;
			r.w = 1.0f;
			return r;
		};
		(void)colorPlatform;

		std::vector<CMergedCubeMesh::Cube> aggCubes;
		aggCubes.reserve(static_cast<size_t>(rows) * cols * 2);

		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				char ch = grid[r][c];
				const float x = c * TILE - halfX;
				const float z = r * TILE - halfZ;
				const bool parity = ((r + c) % 2 == 0);

				if (ch == 'W') {
					// 벽은 바닥 큐브와 벽 큐브를 함께 배치
					const XMFLOAT4 fc = parity ? colorFloorA : colorFloorB;
					aggCubes.push_back({
						XMFLOAT3(x, -FLOOR_H * 0.5f, z),
						XMFLOAT3(TILE, FLOOR_H, TILE), fc });
					const float wh = (wallHeights[r][c] > 0.0f) ? wallHeights[r][c] : WALL_H;
					const XMFLOAT4 wc = parity ? colorWallA : colorWallB;
					aggCubes.push_back({
						XMFLOAT3(x, wh * 0.5f, z),
						XMFLOAT3(TILE, wh, TILE), wc });
					continue;
				}

				// 바닥
				int step = 0;
				if (ch >= '1' && ch <= '3') step = ch - '0';

				const float topY = step * STEP_H;
				const float blockH = topY + FLOOR_H;
				const float centerY = topY * 0.5f - FLOOR_H * 0.5f;
				const XMFLOAT4 baseColor = parity ? colorFloorA : colorFloorB;
				const float t = (step > 0) ? (0.15f + 0.15f * step) : 0.0f;
				const XMFLOAT4 fc = (step > 0) ? lerp4(baseColor, colorStair, t) : baseColor;
				aggCubes.push_back({
					XMFLOAT3(x, centerY, z),
					XMFLOAT3(TILE, blockH, TILE), fc });
			}
		}

		// 매쉬 하나로 합쳐 단일 게임 오브젝트로 등록
		if (!aggCubes.empty()) {
			auto pMesh = std::make_shared<CMergedCubeMesh>(pd3dDevice, pd3dCommandList, aggCubes);
			auto pObj  = std::make_shared<CGameObject>();
			pObj->SetMesh(pMesh);
			vObjects.push_back(std::move(pObj));
		}
	}

	// 통로 폭 넓히기: 25% 확률로 인접 벽 한 칸을 통로로 바꾼다
	void WidenCorridors(std::vector<std::string>& grid, std::mt19937& rng)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const int d4[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };

		std::vector<std::vector<bool>> wasFloor(rows, std::vector<bool>(cols, false));
		for (int r = 0; r < rows; ++r)
			for (int c = 0; c < cols; ++c)
				if (grid[r][c] == '.') wasFloor[r][c] = true;

		auto nearSpawn = [&](int r, int c) {
			return (r >= 1 && r <= 2 && c >= 1 && c <= 2);
		};

		for (int r = 1; r < rows - 1; ++r) {
			for (int c = 1; c < cols - 1; ++c) {
				if (!wasFloor[r][c]) continue;
				if (nearSpawn(r, c)) continue;
				if ((rng() % 100u) >= 25u) continue;
				int order[4] = { 0,1,2,3 };
				for (int i = 3; i > 0; --i) std::swap(order[i], order[rng() % static_cast<unsigned>(i + 1)]);
				for (int k = 0; k < 4; ++k) {
					int nr = r + d4[order[k]][0];
					int nc = c + d4[order[k]][1];
					if (nr <= 0 || nr >= rows - 1 || nc <= 0 || nc >= cols - 1) continue;
					if (grid[nr][nc] != 'W') continue;
					grid[nr][nc] = '.';
					break;
				}
			}
		}
	}

	// 미로 안에 2x2 ~ 4x4 직사각 방을 6~10개 정도 배치
	void PlantRooms(std::vector<std::string>& grid, std::mt19937& rng)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const int nRooms = 6 + static_cast<int>(rng() % 5u);

		std::vector<std::pair<int, int>> placed;
		placed.reserve(nRooms);
		const int kMinSpacing = 5;

		for (int tries = 0; tries < nRooms * 6 && static_cast<int>(placed.size()) < nRooms; ++tries) {
			const int w = 2 + static_cast<int>(rng() % 3u);
			const int h = 2 + static_cast<int>(rng() % 3u);
			if (rows - h - 2 <= 0 || cols - w - 2 <= 0) break;
			const int r0 = 1 + static_cast<int>(rng() % static_cast<unsigned>(rows - h - 2));
			const int c0 = 1 + static_cast<int>(rng() % static_cast<unsigned>(cols - w - 2));

			bool tooClose = false;
			for (size_t i = 0; i < placed.size(); ++i) {
				int dr = placed[i].first - r0;
				int dc = placed[i].second - c0;
				if (dr * dr + dc * dc < kMinSpacing * kMinSpacing) { tooClose = true; break; }
			}
			if (tooClose) continue;

			for (int r = r0; r < r0 + h; ++r)
				for (int c = c0; c < c0 + w; ++c)
					grid[r][c] = '.';
			placed.emplace_back(r0, c0);
		}
	}

	// 보행 가능한 영역을 3~7칸 단위로 잘게 나누어 0~3 STEP_H 높이를 무작위 부여
	void PartitionFloorRegions(std::vector<std::string>& grid, std::mt19937& rng)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const int d4[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };

		// 보행 가능 셀 모으기
		std::vector<std::pair<int, int>> walkable;
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				if (grid[r][c] == '.') walkable.emplace_back(r, c);
			}
		}
		if (walkable.empty()) return;

		// 시드 순회 순서를 무작위로 섞기
		std::vector<int> order(walkable.size());
		for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
		for (int i = static_cast<int>(order.size()) - 1; i > 0; --i) {
			int j = static_cast<int>(rng() % static_cast<unsigned>(i + 1));
			std::swap(order[i], order[j]);
		}

		// 그리디 BFS 로 3~7 크기 영역 키우기
		std::vector<std::vector<int>> regionId(rows, std::vector<int>(cols, -1));
		std::vector<std::vector<std::pair<int, int>>> regionCells;
		for (int idx : order) {
			const int sr = walkable[idx].first;
			const int sc = walkable[idx].second;
			if (regionId[sr][sc] != -1) continue;

			const int rid = static_cast<int>(regionCells.size());
			regionCells.emplace_back();
			regionId[sr][sc] = rid;
			regionCells[rid].emplace_back(sr, sc);

			const int target = 3 + static_cast<int>(rng() % 5u);
			std::queue<std::pair<int, int>> bq;
			bq.emplace(sr, sc);
			while (static_cast<int>(regionCells[rid].size()) < target && !bq.empty()) {
				std::pair<int, int> cur = bq.front(); bq.pop();
				int dorder[4] = { 0,1,2,3 };
				for (int i = 3; i > 0; --i) std::swap(dorder[i], dorder[rng() % static_cast<unsigned>(i + 1)]);
				for (int k = 0; k < 4; ++k) {
					if (static_cast<int>(regionCells[rid].size()) >= target) break;
					int nr = cur.first + d4[dorder[k]][0];
					int nc = cur.second + d4[dorder[k]][1];
					if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
					if (grid[nr][nc] != '.') continue;
					if (regionId[nr][nc] != -1) continue;
					regionId[nr][nc] = rid;
					regionCells[rid].emplace_back(nr, nc);
					bq.emplace(nr, nc);
				}
			}
		}

		const int K = static_cast<int>(regionCells.size());

		// 인접 영역 그래프
		std::vector<std::vector<int>> adj(K);
		auto addEdge = [&](int a, int b) {
			if (a == b) return;
			for (int x : adj[a]) if (x == b) return;
			adj[a].push_back(b);
			adj[b].push_back(a);
		};
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				if (regionId[r][c] < 0) continue;
				for (int k = 0; k < 4; ++k) {
					int nr = r + d4[k][0], nc = c + d4[k][1];
					if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
					if (regionId[nr][nc] < 0) continue;
					addEdge(regionId[r][c], regionId[nr][nc]);
				}
			}
		}

		// 너무 작은 영역을 이웃에 흡수
		auto mergeInto = [&](int src, int dst) {
			for (auto& cell : regionCells[src]) regionId[cell.first][cell.second] = dst;
			regionCells[dst].insert(regionCells[dst].end(), regionCells[src].begin(), regionCells[src].end());
			regionCells[src].clear();
			for (int nb : adj[src]) {
				if (nb == dst) continue;
				adj[nb].erase(std::remove(adj[nb].begin(), adj[nb].end(), src), adj[nb].end());
				addEdge(dst, nb);
			}
			adj[src].clear();
		};
		bool changed = true;
		while (changed) {
			changed = false;
			for (int rid = 0; rid < K; ++rid) {
				const int sz = static_cast<int>(regionCells[rid].size());
				if (sz == 0 || sz >= 3) continue;
				int bestUnder = -1, bestUnderSize = 0x7fffffff;
				int bestAny = -1, bestAnySize = 0x7fffffff;
				for (int nb : adj[rid]) {
					if (regionCells[nb].empty()) continue;
					const int nsz = static_cast<int>(regionCells[nb].size());
					if (nsz + sz <= 7 && nsz < bestUnderSize) { bestUnder = nb; bestUnderSize = nsz; }
					if (nsz < bestAnySize) { bestAny = nb; bestAnySize = nsz; }
				}
				int dst = (bestUnder >= 0) ? bestUnder : bestAny;
				if (dst < 0) continue;
				mergeInto(rid, dst);
				changed = true;
			}
		}

		// 영역별 높이 부여 (인접과 다른 높이 우선)
		std::vector<int> heights(K, -1);
		for (int rid = 0; rid < K; ++rid) {
			if (regionCells[rid].empty()) continue;
			bool taken[4] = { false, false, false, false };
			for (int nb : adj[rid]) {
				if (heights[nb] >= 0 && heights[nb] < 4) taken[heights[nb]] = true;
			}
			int options[4]; int nOpt = 0;
			for (int h = 0; h < 4; ++h) if (!taken[h]) options[nOpt++] = h;
			if (nOpt > 0) {
				heights[rid] = options[rng() % static_cast<unsigned>(nOpt)];
			} else {
				heights[rid] = static_cast<int>(rng() % 4u);
			}
		}

		// 스폰 셀 영역은 0 으로 강제
		if (1 < rows && 1 < cols && regionId[1][1] >= 0) {
			heights[regionId[1][1]] = 0;
		}

		// 결과를 그리드에 반영
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				if (grid[r][c] != '.') continue;
				int rid = regionId[r][c];
				if (rid < 0) continue;
				int h = heights[rid];
				if (h > 0) grid[r][c] = static_cast<char>('0' + h);
			}
		}
	}

	// DFS 로 미로 골격을 만들고 후처리
	std::vector<std::string> GenerateMaze(int rows, int cols, unsigned int seed)
	{
		std::vector<std::string> grid(rows, std::string(cols, 'W'));
		std::mt19937 rng(seed);

		auto inBounds = [&](int r, int c) { return r > 0 && r < rows - 1 && c > 0 && c < cols - 1; };

		std::vector<std::pair<int, int>> stack;
		const int startR = 1, startC = 1;
		grid[startR][startC] = '.';
		stack.emplace_back(startR, startC);

		const int dirs[4][2] = { {-2, 0}, {2, 0}, {0, -2}, {0, 2} };

		while (!stack.empty()) {
			int r = stack.back().first;
			int c = stack.back().second;

			std::vector<int> options;
			for (int i = 0; i < 4; ++i) {
				int nr = r + dirs[i][0];
				int nc = c + dirs[i][1];
				if (inBounds(nr, nc) && grid[nr][nc] == 'W') options.push_back(i);
			}
			if (options.empty()) {
				stack.pop_back();
				continue;
			}
			int pick = options[rng() % options.size()];
			int nr = r + dirs[pick][0];
			int nc = c + dirs[pick][1];
			// 두 셀 사이 벽도 같이 허물어 경로를 만듦
			grid[r + dirs[pick][0] / 2][c + dirs[pick][1] / 2] = '.';
			grid[nr][nc] = '.';
			stack.emplace_back(nr, nc);
		}

		WidenCorridors(grid, rng);
		PlantRooms(grid, rng);
		PartitionFloorRegions(grid, rng);

		return grid;
	}

	// 맵 1
	const std::vector<std::string>& Map1Grid()
	{
		static const std::vector<std::string> grid = GenerateMaze(36, 36, 1u);
		return grid;
	}

	// 맵 2
	const std::vector<std::string>& Map2Grid()
	{
		static const std::vector<std::string> grid = GenerateMaze(36, 36, 2u);
		return grid;
	}

	// 벽 군집(10칸 이상)에 통일된 높이를 부여
	std::vector<std::vector<float>> ComputeWallHeights(const std::vector<std::string>& grid, unsigned int seed)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		std::vector<std::vector<float>> out(rows, std::vector<float>(cols, 0.0f));
		std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
		std::mt19937 rng(seed ^ 0x9E3779B9u);
		const int d4[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };

		for (int r0 = 0; r0 < rows; ++r0) {
			for (int c0 = 0; c0 < cols; ++c0) {
				if (grid[r0][c0] != 'W' || visited[r0][c0]) continue;

				std::vector<std::pair<int, int>> comp;
				std::queue<std::pair<int, int>> q;
				q.emplace(r0, c0);
				visited[r0][c0] = true;
				while (!q.empty()) {
					std::pair<int, int> cur = q.front(); q.pop();
					comp.push_back(cur);
					for (int k = 0; k < 4; ++k) {
						int nr = cur.first + d4[k][0], nc = cur.second + d4[k][1];
						if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
						if (visited[nr][nc] || grid[nr][nc] != 'W') continue;
						visited[nr][nc] = true;
						q.emplace(nr, nc);
					}
				}
				if (static_cast<int>(comp.size()) >= 10) {
					const float h = WALL_H + static_cast<float>(rng() % 4u) * STEP_H * 2.0f;
					for (size_t i = 0; i < comp.size(); ++i) {
						out[comp[i].first][comp[i].second] = h;
					}
				}
			}
		}
		return out;
	}
}

// ============================================================================
// ============================================================================
void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects)
{
	const auto& grid = Map1Grid();
	const auto wallH = ComputeWallHeights(grid, 1u);
	BuildMazeFromGrid(pd3dDevice, pd3dCommandList, vObjects, grid, wallH,
		XMFLOAT4(0.10f, 0.10f, 0.14f, 1.0f),  // 맨 아래 바닥1 : 어두운 콘크리트
		XMFLOAT4(0.14f, 0.14f, 0.20f, 1.0f),  // 맨 아래 바닥2 : 약간 밝은 콘크리트
		XMFLOAT4(0.42f, 0.46f, 0.56f, 1.0f),  // 벽1  : 회청색 강철
		XMFLOAT4(0.32f, 0.36f, 0.46f, 1.0f),  // 벽2  : 어두운 강철
		XMFLOAT4(0.38f, 0.45f, 0.52f, 1.0f),  // 단차가 있는 경우 바닥 색상
		XMFLOAT4(0.32f, 0.62f, 0.82f, 1.0f));
}

void BuildMap2Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects)
{
	const auto& grid = Map2Grid();
	const auto wallH = ComputeWallHeights(grid, 2u);
	BuildMazeFromGrid(pd3dDevice, pd3dCommandList, vObjects, grid, wallH,
		XMFLOAT4(0.18f, 0.12f, 0.07f, 1.0f),
		XMFLOAT4(0.26f, 0.18f, 0.10f, 1.0f),
		XMFLOAT4(0.55f, 0.32f, 0.18f, 1.0f),
		XMFLOAT4(0.42f, 0.24f, 0.14f, 1.0f),
		XMFLOAT4(0.78f, 0.50f, 0.22f, 1.0f),
		XMFLOAT4(1.00f, 0.78f, 0.32f, 1.0f));
}

MapInfo GetMap1Info()
{
	MapInfo info;
	info.cameraPosition = XMFLOAT3(1.0f * TILE - 70.0f, MAP_EYE_HEIGHT, 1.0f * TILE - 70.0f);
	info.cameraLookAt   = XMFLOAT3(1.0f * TILE - 70.0f, MAP_EYE_HEIGHT - 0.2f, 3.0f * TILE - 70.0f);
	return info;
}

MapInfo GetMap2Info()
{
	MapInfo info;
	info.cameraPosition = XMFLOAT3(1.0f * TILE - 70.0f, MAP_EYE_HEIGHT, 1.0f * TILE - 70.0f);
	info.cameraLookAt   = XMFLOAT3(3.0f * TILE - 70.0f, MAP_EYE_HEIGHT - 0.2f, 1.0f * TILE - 70.0f);
	return info;
}

// 이동 가능 여부 판정
namespace {
	constexpr float STEP_UP_TOLERANCE = STEP_H + 0.05f;

	bool IsBlockedInGrid(const std::vector<std::string>& grid, float x, float z, float fFeetY)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const float halfX = (cols - 1) * TILE * 0.5f;
		const float halfZ = (rows - 1) * TILE * 0.5f;

		const int c = static_cast<int>(floorf((x + halfX) / TILE + 0.5f));
		const int r = static_cast<int>(floorf((z + halfZ) / TILE + 0.5f));

		if (c < 0 || c >= cols || r < 0 || r >= rows) return true;

		char ch = grid[r][c];
		if (ch == 'W') return true;
		if (ch >= '1' && ch <= '5') {
			const float topY = STEP_H * (ch - '0');
			return (topY > fFeetY + STEP_UP_TOLERANCE);
		}
		return false;
	}
}

bool IsBlockedInMap(SceneState state, float x, float z, float fFeetY)
{
	switch (state) {
	case SceneState::MAP1: return IsBlockedInGrid(Map1Grid(), x, z, fFeetY);
	case SceneState::MAP2: return IsBlockedInGrid(Map2Grid(), x, z, fFeetY);
	default: return false;
	}
}

// 현재 위치의 바닥 Y 좌표
namespace {
	float GetFloorHeightInGrid(const std::vector<std::string>& grid, float x, float z)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const float halfX = (cols - 1) * TILE * 0.5f;
		const float halfZ = (rows - 1) * TILE * 0.5f;
		const int c = static_cast<int>(floorf((x + halfX) / TILE + 0.5f));
		const int r = static_cast<int>(floorf((z + halfZ) / TILE + 0.5f));
		if (c < 0 || c >= cols || r < 0 || r >= rows) return 0.0f;

		char ch = grid[r][c];
		if (ch >= '1' && ch <= '5') return STEP_H * static_cast<float>(ch - '0');
		return 0.0f;
	}
}

float GetFloorHeightAt(SceneState state, float x, float z)
{
	switch (state) {
	case SceneState::MAP1: return GetFloorHeightInGrid(Map1Grid(), x, z);
	case SceneState::MAP2: return GetFloorHeightInGrid(Map2Grid(), x, z);
	default: return 0.0f;
	}
}

float ClampDistanceAgainstWalls(SceneState state,
	XMFLOAT3 fromXZ, XMFLOAT3 dirXZ, float maxDist, float eyeY)
{
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return maxDist;

	const float dirLen = sqrtf(dirXZ.x * dirXZ.x + dirXZ.z * dirXZ.z);
	if (dirLen < 1e-5f) return 0.0f;
	const float invLen = 1.0f / dirLen;
	const XMFLOAT3 dn{ dirXZ.x * invLen, 0.0f, dirXZ.z * invLen };

	const float kStep = TILE * 0.25f;
	const float kInset = 0.4f;
	for (float d = kStep; d <= maxDist; d += kStep) {
		const float sx = fromXZ.x + dn.x * d;
		const float sz = fromXZ.z + dn.z * d;
		if (IsBlockedInMap(state, sx, sz, eyeY)) {
			const float r = d - kInset;
			return (r < 0.0f) ? 0.0f : r;
		}
	}
	return maxDist;
}

// 두 점 사이의 벽 검사 (자기 칸 제외)
bool HasLineOfSight(SceneState state, XMFLOAT3 from, XMFLOAT3 to, float eyeY)
{
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return true;

	const XMFLOAT3 delta{ to.x - from.x, 0.0f, to.z - from.z };
	const float dist = sqrtf(delta.x * delta.x + delta.z * delta.z);
	if (dist < 1e-5f) return true;
	const float invLen = 1.0f / dist;
	const XMFLOAT3 dn{ delta.x * invLen, 0.0f, delta.z * invLen };

	const float kStep = TILE * 0.25f;
	const float kSkip = TILE * 0.5f;
	const float dStart = kSkip;
	const float dEnd = dist - kSkip;
	if (dEnd <= dStart) return true;

	for (float d = dStart; d <= dEnd; d += kStep) {
		const float sx = from.x + dn.x * d;
		const float sz = from.z + dn.z * d;
		if (IsBlockedInMap(state, sx, sz, eyeY)) return false;
	}
	return true;
}

// 적 스폰 위치 무작위 선택
std::vector<XMFLOAT3> PickEnemySpawnPositions(SceneState state,
	XMFLOAT3 xmf3PlayerStart, int nMax, float fHalfBodyY)
{
	std::vector<XMFLOAT3> result;
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return result;

	const std::vector<std::string>& grid =
		(state == SceneState::MAP1) ? Map1Grid() : Map2Grid();
	const int rows = static_cast<int>(grid.size());
	const int cols = static_cast<int>(grid[0].size());
	const float halfX = (cols - 1) * TILE * 0.5f;
	const float halfZ = (rows - 1) * TILE * 0.5f;

	const int playerC = static_cast<int>(floorf((xmf3PlayerStart.x + halfX) / TILE + 0.5f));
	const int playerR = static_cast<int>(floorf((xmf3PlayerStart.z + halfZ) / TILE + 0.5f));

	std::vector<XMFLOAT3> candidates;
	candidates.reserve(rows * cols);
	for (int r = 0; r < rows; ++r) {
		for (int c = 0; c < cols; ++c) {
			const char ch = grid[r][c];
			const bool walkable = (ch == '.') || (ch >= '1' && ch <= '3');
			if (!walkable) continue;

			const int dr = abs(r - playerR);
			const int dc = abs(c - playerC);
			const int cheb = (dr > dc) ? dr : dc;
			if (cheb < 5) continue;

			const float x = c * TILE - halfX;
			const float z = r * TILE - halfZ;
			const float floorY = GetFloorHeightAt(state, x, z);
			candidates.emplace_back(x, floorY + fHalfBodyY, z);
		}
	}

	// 무작위로 앞에서 nMax 개 추출
	std::mt19937 rng{ std::random_device{}() };
	std::shuffle(candidates.begin(), candidates.end(), rng);
	const size_t cap = (nMax < 0) ? 0u : static_cast<size_t>(nMax);
	const size_t take = (candidates.size() < cap) ? candidates.size() : cap;
	result.assign(candidates.begin(), candidates.begin() + take);
	return result;
}

// A* 그리드 셀 기반 최단 경로
std::vector<XMFLOAT3> FindPathAStar(
    SceneState state,
    const XMFLOAT3& start,
    const XMFLOAT3& goal,
    int nMaxNodes)
{
    if (state != SceneState::MAP1 && state != SceneState::MAP2) return {};

    const std::vector<std::string>& grid =
        (state == SceneState::MAP1) ? Map1Grid() : Map2Grid();
    const int rows = static_cast<int>(grid.size());
    const int cols = static_cast<int>(grid[0].size());
    const float halfX = (cols - 1) * TILE * 0.5f;
    const float halfZ = (rows - 1) * TILE * 0.5f;

    auto WorldToGrid = [&](const XMFLOAT3& pos, int& outC, int& outR) {
        outC = static_cast<int>(floorf((pos.x + halfX) / TILE + 0.5f));
        outR = static_cast<int>(floorf((pos.z + halfZ) / TILE + 0.5f));
    };

    auto GridToWorld = [&](int c, int r) -> XMFLOAT3 {
        const float worldX = c * TILE - halfX;
        const float worldZ = r * TILE - halfZ;
        const float floorY = GetFloorHeightAt(state, worldX, worldZ);
        return XMFLOAT3(worldX, floorY, worldZ);
    };

    int startC, startR, goalC, goalR;
    WorldToGrid(start, startC, startR);
    WorldToGrid(goal,  goalC,  goalR);

    if (startC < 0 || startC >= cols || startR < 0 || startR >= rows) return {};
    if (goalC  < 0 || goalC  >= cols || goalR  < 0 || goalR  >= rows) return {};
    if (startC == goalC && startR == goalR) return {};

    // 'W' 만 불통, 단차는 통과 가능
    auto isWalkable = [&](int c, int r) -> bool {
        if (c < 0 || c >= cols || r < 0 || r >= rows) return false;
        return grid[r][c] != 'W';
    };

    auto idx2 = [&](int c, int r) -> int { return r * cols + c; };
    const int N = rows * cols;

    std::vector<float> gCost(N, 1e30f);
    std::vector<bool>  closed(N, false);
    std::vector<int>   parent(N, -1);

    using PQItem = std::pair<float, int>;
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> openSet;

    auto heuristic = [&](int c, int r) -> float {
        return static_cast<float>(abs(c - goalC) + abs(r - goalR)) * TILE;
    };

    const int startIdx = idx2(startC, startR);
    gCost[startIdx] = 0.0f;
    openSet.emplace(heuristic(startC, startR), startIdx);

    // 4방향 이웃
    const int dc[4] = {  0,  0,  1, -1 };
    const int dr[4] = {  1, -1,  0,  0 };

    int nodesExpanded = 0;
    const int goalIdx = idx2(goalC, goalR);

    while (!openSet.empty() && nodesExpanded < nMaxNodes) {
        const PQItem topItem = openSet.top();
        const int   curIdx   = topItem.second;
        openSet.pop();

        if (closed[curIdx]) continue;
        closed[curIdx] = true;
        ++nodesExpanded;

        if (curIdx == goalIdx) {
            std::vector<XMFLOAT3> path;
            int pIdx = goalIdx;
            while (pIdx != startIdx) {
                const int pc = pIdx % cols;
                const int pr = pIdx / cols;
                path.push_back(GridToWorld(pc, pr));
                pIdx = parent[pIdx];
                if (pIdx < 0) break;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        const int cc = curIdx % cols;
        const int cr = curIdx / cols;

        for (int i = 0; i < 4; ++i) {
            const int nc = cc + dc[i];
            const int nr = cr + dr[i];
            if (!isWalkable(nc, nr)) continue;
            const int nIdx = idx2(nc, nr);
            if (closed[nIdx]) continue;

            const float newG = gCost[curIdx] + TILE;
            if (newG < gCost[nIdx]) {
                gCost[nIdx]  = newG;
                parent[nIdx] = curIdx;
                openSet.emplace(newG + heuristic(nc, nr), nIdx);
            }
        }
    }

    return {};
}
