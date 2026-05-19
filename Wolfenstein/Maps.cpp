#include "stdafx.h"
#include "Maps.h"
#include "Mesh.h"
#include "Scene.h"
#include <string>
#include <random>
#include <queue>
#include <algorithm>

namespace {
	// Wolfenstein 풍 미로의 기본 단위.
	constexpr float TILE = 4.0f;        // 한 셀의 가로/세로 크기
	constexpr float WALL_H = 8.0f;      // 천장까지 닿는 벽 기본 높이
	constexpr float FLOOR_H = 0.2f;     // 바닥 두께
	constexpr float STEP_H = 0.7f;      // 계단 한 단의 높이

	// 한 개의 큐브 게임 오브젝트를 만들어 vector 에 추가한다.
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
	//   'W' : 벽 (높이는 wallHeights 에서 셀별 지정, 0 이면 기본 WALL_H)
	//   '.' : 통로 (높이 0)
	//   '1' ~ '9' : 해당 칸 위에 STEP_H * n 높이의 단을 둠
	//   'P' : 단상 (STEP_H * 6 높이)
	// 그 외 문자는 바닥만 깐다.
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
					// 벽 높이: 10칸 이상의 연결 성분이면 ComputeWallHeights 가 부여한 값,
					// 그렇지 않으면 기본 WALL_H 를 사용한다.
					const float wh = (wallHeights[r][c] > 0.0f) ? wallHeights[r][c] : WALL_H;
					const XMFLOAT4 wc = parity ? colorWallA : colorWallB;
					AddCube(pd3dDevice, pd3dCommandList, vObjects,
						XMFLOAT3(x, wh * 0.5f, z),
						XMFLOAT3(TILE, wh, TILE), wc);
				}
				else if (ch >= '1' && ch <= '9') {
					// 영역별 균일 높이로 부여된 보행 단
					const int step = ch - '0';
					const float h = STEP_H * step;
					AddCube(pd3dDevice, pd3dCommandList, vObjects,
						XMFLOAT3(x, h * 0.5f, z),
						XMFLOAT3(TILE, h, TILE), colorStair);
				}
				else if (ch == 'P') {
					// 단상
					const float h = STEP_H * 6.0f;
					AddCube(pd3dDevice, pd3dCommandList, vObjects,
						XMFLOAT3(x, h * 0.5f, z),
						XMFLOAT3(TILE, h, TILE), colorPlatform);
				}
				// 그 외 ('.') : 바닥만으로 충분
			}
		}
	}

	// === 보행 영역 분할 (요구 A) ===
	// 모든 '.' 셀을 다중 시드 BFS 로 K 개의 영역으로 나누고,
	// 인접 영역 사이의 높이 차가 +-1 단계 이내가 되도록 BFS 로 높이를 전파한다.
	// 결과: 연결된 보행 구역마다 균일한 높이를 가지며, 영역 사이는 STEP_UP_TOLERANCE 이내에서
	// 차이가 발생하므로 플레이어가 자연스럽게 걸어다닐 수 있다.
	void PartitionFloorRegions(std::vector<std::string>& grid, std::mt19937& rng)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());

		// 1. 보행 셀 수집 (시작 셀은 (1,1))
		const int startR = 1, startC = 1;
		std::vector<std::pair<int, int>> walkable;
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				if (grid[r][c] == '.') walkable.emplace_back(r, c);
			}
		}
		if (walkable.empty()) return;

		// 2. K 개의 시드 선택. 시작 셀은 항상 첫 시드로 고정.
		const int K = (std::max)(4, static_cast<int>(walkable.size()) / 50);
		std::vector<std::pair<int, int>> seeds;
		seeds.emplace_back(startR, startC);
		std::vector<int> idx(walkable.size());
		for (size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);
		for (int i = static_cast<int>(idx.size()) - 1; i > 0; --i) {
			int j = static_cast<int>(rng() % static_cast<unsigned>(i + 1));
			std::swap(idx[i], idx[j]);
		}
		for (size_t i = 0; static_cast<int>(seeds.size()) < K && i < idx.size(); ++i) {
			auto p = walkable[idx[i]];
			if (p.first == startR && p.second == startC) continue;
			seeds.push_back(p);
		}

		// 3. 다중 소스 BFS: 모든 '.' 셀에 가장 가까운 시드의 regionId 를 부여.
		std::vector<std::vector<int>> regionId(rows, std::vector<int>(cols, -1));
		std::queue<std::pair<int, int>> bfs;
		for (int i = 0; i < static_cast<int>(seeds.size()); ++i) {
			regionId[seeds[i].first][seeds[i].second] = i;
			bfs.push(seeds[i]);
		}
		const int d4[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };
		while (!bfs.empty()) {
			std::pair<int, int> cur = bfs.front(); bfs.pop();
			const int r = cur.first, c = cur.second;
			for (int k = 0; k < 4; ++k) {
				int nr = r + d4[k][0], nc = c + d4[k][1];
				if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
				if (grid[nr][nc] != '.') continue;
				if (regionId[nr][nc] != -1) continue;
				regionId[nr][nc] = regionId[r][c];
				bfs.emplace(nr, nc);
			}
		}

		// 4. 영역 인접 그래프 구성
		const int K_REG = static_cast<int>(seeds.size());
		std::vector<std::vector<int>> adj(K_REG);
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

		// 5. 시작 영역에서 BFS 로 높이 전파. 인접 사이 차이를 -1, 0, +1 중 하나로 제한.
		std::vector<int> heightStep(K_REG, -1);
		const int startRegion = regionId[startR][startC];
		heightStep[startRegion] = 0;
		std::queue<int> hq;
		hq.push(startRegion);
		while (!hq.empty()) {
			int cur = hq.front(); hq.pop();
			for (int nb : adj[cur]) {
				if (heightStep[nb] != -1) continue;
				int delta = static_cast<int>(rng() % 3u) - 1; // -1, 0, +1
				int cand = heightStep[cur] + delta;
				if (cand < 0) cand = 0;
				if (cand > 3) cand = 3;
				heightStep[nb] = cand;
				hq.push(nb);
			}
		}

		// 6. 결과를 그리드에 기록. 높이 0 은 '.', 1~3 은 '1'~'3' 으로 저장.
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				if (grid[r][c] != '.') continue;
				int rid = regionId[r][c];
				if (rid < 0) continue;
				int h = heightStep[rid];
				if (h > 0) grid[r][c] = static_cast<char>('0' + h);
			}
		}
	}

	// === DFS 미로 생성 + 영역 분할 ===
	// 30x30 격자를 모두 벽(W)으로 채운 뒤, DFS 로 보행 통로를 뚫고,
	// 보행 영역들에 균일한 높이를 부여한다.
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
			// 두 셀 사이의 벽을 뚫어 통로를 만든다.
			grid[r + dirs[pick][0] / 2][c + dirs[pick][1] / 2] = '.';
			grid[nr][nc] = '.';
			stack.emplace_back(nr, nc);
		}

		// 기존의 단일 셀 랜덤 단('1') 산포 대신, 연결 영역 단위로 균일한 높이를 부여한다.
		PartitionFloorRegions(grid, rng);

		return grid;
	}

	// 맵 1 (30x30) - 시드 1 로 생성된 미로.
	const std::vector<std::string>& Map1Grid()
	{
		static const std::vector<std::string> grid = GenerateMaze(30, 30, 1u);
		return grid;
	}

	// 맵 2 (30x30) - 다른 시드(2) 로 생성된 동일 형식의 미로.
	const std::vector<std::string>& Map2Grid()
	{
		static const std::vector<std::string> grid = GenerateMaze(30, 30, 2u);
		return grid;
	}

	// === 벽 연결 성분별 높이 계산 (요구 B) ===
	// 'W' 셀들을 4방향 인접으로 플러드필 하여 연결 성분을 찾고,
	// 성분 크기가 10 셀 이상이면 그 성분 전체에 단일 높이를 부여한다.
	// 10 미만의 짧은 벽 조각은 0.0f 를 남겨 BuildMazeFromGrid 가 기본 WALL_H 를 사용하도록 한다.
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
					// 8.0, 9.4, 10.8, 12.2 중 하나로 성분 전체를 고른다.
					const float h = WALL_H + static_cast<float>(rng() % 4u) * STEP_H * 2.0f;
					for (size_t i = 0; i < comp.size(); ++i) {
						out[comp[i].first][comp[i].second] = h;
					}
				}
				// 10 미만이면 out 은 0 그대로 -> 기본 WALL_H 사용
			}
		}
		return out;
	}
} // namespace

void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects)
{
	// 차가운 회청색 돌 미로. 어두운 바닥과 푸르스름한 석재 벽.
	const auto& grid = Map1Grid();
	const auto wallH = ComputeWallHeights(grid, 1u);
	BuildMazeFromGrid(pd3dDevice, pd3dCommandList, vObjects, grid, wallH,
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
	const auto& grid = Map2Grid();
	const auto wallH = ComputeWallHeights(grid, 2u);
	BuildMazeFromGrid(pd3dDevice, pd3dCommandList, vObjects, grid, wallH,
		XMFLOAT4(0.18f, 0.12f, 0.07f, 1.0f),  // floor A : 짙은 흙색
		XMFLOAT4(0.26f, 0.18f, 0.10f, 1.0f),  // floor B : 약간 밝은 흙색
		XMFLOAT4(0.55f, 0.32f, 0.18f, 1.0f),  // wall A  : 적벽돌
		XMFLOAT4(0.42f, 0.24f, 0.14f, 1.0f),  // wall B  : 어두운 벽돌
		XMFLOAT4(0.78f, 0.50f, 0.22f, 1.0f),  // stair   : 따뜻한 오렌지
		XMFLOAT4(1.00f, 0.78f, 0.32f, 1.0f)); // platform: 횃불 노랑
}

MapInfo GetMap1Info()
{
	// 30x30 미로의 (열1, 행1) 시작 위치에서 정면(+Z) 방향을 바라본다.
	// halfX = halfZ = (30-1) * TILE * 0.5 = 58
	MapInfo info;
	info.cameraPosition = XMFLOAT3(1.0f * TILE - 58.0f, MAP_EYE_HEIGHT, 1.0f * TILE - 58.0f);
	info.cameraLookAt   = XMFLOAT3(1.0f * TILE - 58.0f, MAP_EYE_HEIGHT - 0.2f, 3.0f * TILE - 58.0f);
	return info;
}

MapInfo GetMap2Info()
{
	// 30x30 미로의 시작 위치에서 동쪽(+X)을 본다. 다른 시드로 만들어진 미로지만 동일한 좌표 체계.
	MapInfo info;
	info.cameraPosition = XMFLOAT3(1.0f * TILE - 58.0f, MAP_EYE_HEIGHT, 1.0f * TILE - 58.0f);
	info.cameraLookAt   = XMFLOAT3(3.0f * TILE - 58.0f, MAP_EYE_HEIGHT - 0.2f, 1.0f * TILE - 58.0f);
	return info;
}
// ===================== 충돌 처리 =====================
// 플레이어 발 높이(fFeetY) 를 기반으로 (x,z) 에서 이동이 막히는지 판정한다.
// W : 항상 막힘.
// 1~9 / P : 단의 윗면이 fFeetY + STEP_UP_TOLERANCE 보다 높으면 막힘.
// '.' : 항상 통과.
// 격자 바깥은 모두 막힘.
namespace {
	constexpr float STEP_UP_TOLERANCE = STEP_H + 0.05f;

	bool IsBlockedInGrid(const std::vector<std::string>& grid, float x, float z, float fFeetY)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const float halfX = (cols - 1) * TILE * 0.5f;
		const float halfZ = (rows - 1) * TILE * 0.5f;

		// (x,z) -> (c,r) 변환. 셀 중앙을 기준으로 0.5f 오프셋 적용.
		const int c = static_cast<int>(floorf((x + halfX) / TILE + 0.5f));
		const int r = static_cast<int>(floorf((z + halfZ) / TILE + 0.5f));

		// 격자 바깥은 막힘으로 처리.
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
		return false; // '.' 는 통과
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

// (x,z) 에서 발이 닿을 Y 좌표를 반환. ProcessInput 에서 중력 처리와 카메라 Y 보정에 사용한다.
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
		if (ch >= '1' && ch <= '9') return STEP_H * static_cast<float>(ch - '0');
		if (ch == 'P') return STEP_H * 6.0f;
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
