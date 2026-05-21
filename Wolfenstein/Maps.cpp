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
	// Wolfenstein ǳ �̷��� �⺻ ����.
	constexpr float TILE = 4.0f;        // �� ���� ����/���� ũ��
	constexpr float WALL_H = 8.0f;      // õ����� ��� �� �⺻ ����
	constexpr float FLOOR_H = 0.2f;     // �ٴ� �β�
	constexpr float STEP_H = 0.7f;      // ��� �� ���� ����

	// �� ���� ť�� ���� ������Ʈ�� ����� vector �� �߰��Ѵ�.
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

	// 격자 그리드를 받아 미로 큐브를 배치한다.
	// 셀 의미:
	//   'W' : 벽 (높이는 wallHeights 가 0 이 아니면 그 값, 0 이면 기본 WALL_H)
	//   '.' : 평면 바닥 (높이 0)
	//   '1' ~ '3' : 바닥 큐브 자체가 STEP_H * n 높이로 솟아오른 단차 바닥
	//               (별도의 슬랩을 위에 얹지 않고 큐브 윗면이 그 높이에 위치)
	// 그 외 문자는 만나면 평면 바닥으로 처리.
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

		// 단차 바닥은 '1'..'3' 문자로 1~3 STEP_H 높이를 인코딩한다.
		// 별도의 슬랩(판)을 위에 올리지 않고 바닥 큐브 자체의 두께를 늘려
		// 윗면이 step * STEP_H 에 오도록 한다.
		// colorStair 는 그라데이션의 끝점 색으로 재사용되고, colorPlatform 은 사용 X.
		auto lerp4 = [](const XMFLOAT4& a, const XMFLOAT4& b, float t) {
			XMFLOAT4 r;
			r.x = a.x * (1.0f - t) + b.x * t;
			r.y = a.y * (1.0f - t) + b.y * t;
			r.z = a.z * (1.0f - t) + b.z * t;
			r.w = 1.0f;
			return r;
		};
		(void)colorPlatform;

		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				char ch = grid[r][c];
				const float x = c * TILE - halfX;
				const float z = r * TILE - halfZ;
				const bool parity = ((r + c) % 2 == 0);

				if (ch == 'W') {
					// Wall cell. Keep a thin base tile under it so floor and
					// wall corners stay flush at floor level 0.
					const XMFLOAT4 fc = parity ? colorFloorA : colorFloorB;
					AddCube(pd3dDevice, pd3dCommandList, vObjects,
						XMFLOAT3(x, -FLOOR_H * 0.5f, z),
						XMFLOAT3(TILE, FLOOR_H, TILE), fc);
					const float wh = (wallHeights[r][c] > 0.0f) ? wallHeights[r][c] : WALL_H;
					const XMFLOAT4 wc = parity ? colorWallA : colorWallB;
					AddCube(pd3dDevice, pd3dCommandList, vObjects,
						XMFLOAT3(x, wh * 0.5f, z),
						XMFLOAT3(TILE, wh, TILE), wc);
					continue;
				}

				// 평면('.') 또는 단차 바닥('1'..'3'). step == 0 은 '.' (기본 높이).
				int step = 0;
				if (ch >= '1' && ch <= '3') step = ch - '0';

				const float topY = step * STEP_H;
				const float blockH = topY + FLOOR_H;
				const float centerY = topY * 0.5f - FLOOR_H * 0.5f;
				const XMFLOAT4 baseColor = parity ? colorFloorA : colorFloorB;
				const float t = (step > 0) ? (0.15f + 0.15f * step) : 0.0f;
				const XMFLOAT4 fc = (step > 0) ? lerp4(baseColor, colorStair, t) : baseColor;
				AddCube(pd3dDevice, pd3dCommandList, vObjects,
					XMFLOAT3(x, centerY, z),
					XMFLOAT3(TILE, blockH, TILE), fc);
			}
		}
	}

	// === ���� ���� ���� (�䱸 A) ===
	// ��� '.' ���� ���� �õ� BFS �� K ���� �������� ������,
	// ���� ���� ������ ���� ���� +-1 �ܰ� �̳��� �ǵ��� BFS �� ���̸� �����Ѵ�.
	// ���: ����� ���� �������� ������ ���̸� ������, ���� ���̴� STEP_UP_TOLERANCE �̳�����
	// ���̰� �߻��ϹǷ� �÷��̾ �ڿ������� �ɾ�ٴ� �� �ִ�.
	// Post-DFS widening pass: visit each '.' cell and occasionally knock a
	// neighboring wall down. This breaks the strict 1-tile corridor look.
	// Outer ring is preserved. Spawn (1,1) and its immediate area are kept.
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

	// Plant several rectangular rooms (2x2 .. 4x4) inside the maze.
	// Connectivity is preserved because the DFS skeleton stays intact.
	void PlantRooms(std::vector<std::string>& grid, std::mt19937& rng)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const int nRooms = 6 + static_cast<int>(rng() % 5u); // 6 .. 10

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

	// === 영역별 랜덤 높이 부여 (소영역 + 균등 무작위) ===
	// 보행 가능한 '.' 셀을 3~7칸 크기의 작은 영역으로 무작위 그리디 BFS 로 분할한 뒤,
	// 각 영역에 0~3 STEP_H 범위의 무작위 높이를 부여한다.
	// 같은 높이를 공유하는 연결 영역의 크기를 최소 3, 최대 7 로 유지하여
	// "특정 구역에 두꺼운 판이 붙은" 외형 대신 곳곳이 잘게 변화하는 지형을 만든다.
	// 점프 정점이 3 * STEP_H 이므로 모든 인접 영역 사이를 점프로 통과할 수 있다.
	// 인접 영역끼리는 가능하면 다른 높이를 갖도록 보정해 시각적 단차를 강조한다.
	void PartitionFloorRegions(std::vector<std::string>& grid, std::mt19937& rng)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const int d4[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };

		// 1. 보행 가능한 '.' 셀 모음
		std::vector<std::pair<int, int>> walkable;
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				if (grid[r][c] == '.') walkable.emplace_back(r, c);
			}
		}
		if (walkable.empty()) return;

		// 2. 시드 순회 순서를 무작위로 섞는다 (Fisher-Yates).
		std::vector<int> order(walkable.size());
		for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
		for (int i = static_cast<int>(order.size()) - 1; i > 0; --i) {
			int j = static_cast<int>(rng() % static_cast<unsigned>(i + 1));
			std::swap(order[i], order[j]);
		}

		// 3. 그리디 BFS 성장: 미할당 셀을 시드로 잡아 목표 3~7 크기까지 키운다.
		//    매 pop 마다 4-방향 순서를 섞어 길쭉한 편향을 줄인다.
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

			const int target = 3 + static_cast<int>(rng() % 5u); // 3..7
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

		// 4. 인접 영역 그래프 (병합 및 색칠에 사용)
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

		// 5. 너무 작은 영역(1~2칸)을 이웃에 흡수한다.
		//    합쳐도 7 을 넘지 않는 이웃 중 가장 작은 것을 우선. 모두 포화면
		//    가장 작은 이웃에 합쳐 8~9 정도까지 허용.
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
				if (dst < 0) continue; // 고립된 영역: 그대로 둠 (드문 케이스)
				mergeInto(rid, dst);
				changed = true;
			}
		}

		// 6. 영역별 높이 부여 [0..3]. 인접 영역과 다른 높이를 우선 선택해
		//    시각적으로 평탄한 덩어리가 생기지 않도록 한다.
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
				// 4 색이 모두 막힌 매우 드문 경우: 그냥 무작위.
				heights[rid] = static_cast<int>(rng() % 4u);
			}
		}

		// 7. 스폰 셀 (1,1) 소속 영역은 0 으로 강제 (스폰 직후 공중에 뜨지 않게).
		if (1 < rows && 1 < cols && regionId[1][1] >= 0) {
			heights[regionId[1][1]] = 0;
		}

		// 8. 결과를 그리드에 반영. 높이 0 은 '.' 유지, 1~3 은 '1'~'3' 으로 기록.
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

	// === DFS �̷� ���� + ���� ���� ===
	// 30x30 ���ڸ� ��� ��(W)���� ä�� ��, DFS �� ���� ��θ� �հ�,
	// ���� �����鿡 ������ ���̸� �ο��Ѵ�.
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
			// �� �� ������ ���� �վ� ��θ� �����.
			grid[r + dirs[pick][0] / 2][c + dirs[pick][1] / 2] = '.';
			grid[nr][nc] = '.';
			stack.emplace_back(nr, nc);
		}

		// Break the strict 1-tile corridor look and plant a few rooms so the
		// space reads as varied in width. PartitionFloorRegions still runs on
		// the resulting '.' cells.
		WidenCorridors(grid, rng);
		PlantRooms(grid, rng);

		// ������ ���� �� ���� ��('1') ���� ���, ���� ���� ������ ������ ���̸� �ο��Ѵ�.
		PartitionFloorRegions(grid, rng);

		return grid;
	}

	// �� 1 (30x30) - �õ� 1 �� ������ �̷�.
	const std::vector<std::string>& Map1Grid()
	{
		static const std::vector<std::string> grid = GenerateMaze(30, 30, 1u);
		return grid;
	}

	// �� 2 (30x30) - �ٸ� �õ�(2) �� ������ ���� ������ �̷�.
	const std::vector<std::string>& Map2Grid()
	{
		static const std::vector<std::string> grid = GenerateMaze(30, 30, 2u);
		return grid;
	}

	// === �� ���� ���к� ���� ��� (�䱸 B) ===
	// 'W' ������ 4���� �������� �÷����� �Ͽ� ���� ������ ã��,
	// ���� ũ�Ⱑ 10 �� �̻��̸� �� ���� ��ü�� ���� ���̸� �ο��Ѵ�.
	// 10 �̸��� ª�� �� ������ 0.0f �� ���� BuildMazeFromGrid �� �⺻ WALL_H �� ����ϵ��� �Ѵ�.
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
					// 8.0, 9.4, 10.8, 12.2 �� �ϳ��� ���� ��ü�� ������.
					const float h = WALL_H + static_cast<float>(rng() % 4u) * STEP_H * 2.0f;
					for (size_t i = 0; i < comp.size(); ++i) {
						out[comp[i].first][comp[i].second] = h;
					}
				}
				// 10 �̸��̸� out �� 0 �״�� -> �⺻ WALL_H ���
			}
		}
		return out;
	}
} // namespace

void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects)
{
	// ������ ȸû�� �� �̷�. ��ο� �ٴڰ� Ǫ�������� ���� ��.
	const auto& grid = Map1Grid();
	const auto wallH = ComputeWallHeights(grid, 1u);
	BuildMazeFromGrid(pd3dDevice, pd3dCommandList, vObjects, grid, wallH,
		XMFLOAT4(0.10f, 0.10f, 0.14f, 1.0f),  // floor A : ��ο� ������Ʈ
		XMFLOAT4(0.14f, 0.14f, 0.20f, 1.0f),  // floor B : �ణ ���� ������Ʈ
		XMFLOAT4(0.42f, 0.46f, 0.56f, 1.0f),  // wall A  : ȸû�� ����
		XMFLOAT4(0.32f, 0.36f, 0.46f, 1.0f),  // wall B  : ��ο� ���� (üũ ����)
		XMFLOAT4(0.68f, 0.55f, 0.32f, 1.0f),  // stair   : ��� ��
		XMFLOAT4(0.92f, 0.82f, 0.42f, 1.0f)); // platform: �ݺ� ����
}

void BuildMap2Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects)
{
	// ������ ���� ���� ����. ȶ�ҿ� ��ģ ���� ����.
	const auto& grid = Map2Grid();
	const auto wallH = ComputeWallHeights(grid, 2u);
	BuildMazeFromGrid(pd3dDevice, pd3dCommandList, vObjects, grid, wallH,
		XMFLOAT4(0.18f, 0.12f, 0.07f, 1.0f),  // floor A : £�� ���
		XMFLOAT4(0.26f, 0.18f, 0.10f, 1.0f),  // floor B : �ణ ���� ���
		XMFLOAT4(0.55f, 0.32f, 0.18f, 1.0f),  // wall A  : ������
		XMFLOAT4(0.42f, 0.24f, 0.14f, 1.0f),  // wall B  : ��ο� ����
		XMFLOAT4(0.78f, 0.50f, 0.22f, 1.0f),  // stair   : ������ ������
		XMFLOAT4(1.00f, 0.78f, 0.32f, 1.0f)); // platform: ȶ�� ���
}

MapInfo GetMap1Info()
{
	// 30x30 �̷��� (��1, ��1) ���� ��ġ���� ����(+Z) ������ �ٶ󺻴�.
	// halfX = halfZ = (30-1) * TILE * 0.5 = 58
	MapInfo info;
	info.cameraPosition = XMFLOAT3(1.0f * TILE - 58.0f, MAP_EYE_HEIGHT, 1.0f * TILE - 58.0f);
	info.cameraLookAt   = XMFLOAT3(1.0f * TILE - 58.0f, MAP_EYE_HEIGHT - 0.2f, 3.0f * TILE - 58.0f);
	return info;
}

MapInfo GetMap2Info()
{
	// 30x30 �̷��� ���� ��ġ���� ����(+X)�� ����. �ٸ� �õ�� ������� �̷����� ������ ��ǥ ü��.
	MapInfo info;
	info.cameraPosition = XMFLOAT3(1.0f * TILE - 58.0f, MAP_EYE_HEIGHT, 1.0f * TILE - 58.0f);
	info.cameraLookAt   = XMFLOAT3(3.0f * TILE - 58.0f, MAP_EYE_HEIGHT - 0.2f, 1.0f * TILE - 58.0f);
	return info;
}
// ===================== 충돌 처리 =====================
// 플레이어 발 높이(fFeetY) 를 기준으로 (x,z) 방향 이동이 가능한지 판정한다.
// W       : 항상 막힘.
// '1'~'3' : 단차 윗면이 fFeetY + STEP_UP_TOLERANCE 보다 높으면 막힘.
// '.'     : 항상 통과.
// 지도 바깥은 막힘 취급.
namespace {
	constexpr float STEP_UP_TOLERANCE = STEP_H + 0.05f;

	bool IsBlockedInGrid(const std::vector<std::string>& grid, float x, float z, float fFeetY)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());
		const float halfX = (cols - 1) * TILE * 0.5f;
		const float halfZ = (rows - 1) * TILE * 0.5f;

		// (x,z) -> (c,r) ��ȯ. �� �߾��� �������� 0.5f ������ ����.
		const int c = static_cast<int>(floorf((x + halfX) / TILE + 0.5f));
		const int r = static_cast<int>(floorf((z + halfZ) / TILE + 0.5f));

		// ���� �ٱ��� �������� ó��.
		if (c < 0 || c >= cols || r < 0 || r >= rows) return true;

		char ch = grid[r][c];
		if (ch == 'W') return true;
		if (ch >= '1' && ch <= '5') {
			const float topY = STEP_H * (ch - '0');
			return (topY > fFeetY + STEP_UP_TOLERANCE);
		}
		return false; // '.' �� ���
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

// (x,z) ���� ���� ���� Y ��ǥ�� ��ȯ. ProcessInput ���� �߷� ó���� ī�޶� Y ������ ����Ѵ�.
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

// 두 지점 사이를 수평 마칭으로 훑어 벽이 가로막는지 검사한다.
// ClampDistanceAgainstWalls 와 같은 TILE * 0.25 간격을 사용하지만,
// 시작 / 끝 셀 부근은 검사에서 제외하여 적/플레이어 본인이 서 있는 셀이
// 자기 자신 때문에 시야가 막히는 일이 없게 한다.
bool HasLineOfSight(SceneState state, XMFLOAT3 from, XMFLOAT3 to, float eyeY)
{
	if (state != SceneState::MAP1 && state != SceneState::MAP2) return true;

	const XMFLOAT3 delta{ to.x - from.x, 0.0f, to.z - from.z };
	const float dist = sqrtf(delta.x * delta.x + delta.z * delta.z);
	if (dist < 1e-5f) return true;
	const float invLen = 1.0f / dist;
	const XMFLOAT3 dn{ delta.x * invLen, 0.0f, delta.z * invLen };

	const float kStep = TILE * 0.25f;
	// 시작/끝 셀의 본인 위치는 건너뛴다 (kSkip ≈ 본인 AABB 반경).
	const float kSkip = TILE * 0.5f;
	const float dStart = kSkip;
	const float dEnd = dist - kSkip;
	if (dEnd <= dStart) return true; // 매우 가까우면 시야가 통한다고 본다

	for (float d = dStart; d <= dEnd; d += kStep) {
		const float sx = from.x + dn.x * d;
		const float sz = from.z + dn.z * d;
		if (IsBlockedInMap(state, sx, sz, eyeY)) return false;
	}
	return true;
}

// 적 스폰 위치를 무작위로 nMax 개 골라 반환한다. 플레이어 시작 위치 기준
// Chebyshev 거리 ≥ 5 (= 반경 5타일 바깥) 인 보행 가능 셀만 후보로 삼는다.
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

	// 플레이어 시작 위치의 (행, 열) 환산.
	const int playerC = static_cast<int>(floorf((xmf3PlayerStart.x + halfX) / TILE + 0.5f));
	const int playerR = static_cast<int>(floorf((xmf3PlayerStart.z + halfZ) / TILE + 0.5f));

	std::vector<XMFLOAT3> candidates;
	candidates.reserve(rows * cols);
	for (int r = 0; r < rows; ++r) {
		for (int c = 0; c < cols; ++c) {
			const char ch = grid[r][c];
			// 평면/단차 바닥만 스폰 후보.
			const bool walkable = (ch == '.') || (ch >= '1' && ch <= '3');
			if (!walkable) continue;

			// Chebyshev 거리 (행/열 max) ≥ 5 (= 반경 5타일 바깥).
			// windows.h 의 max 매크로 충돌을 피하기 위해 직접 비교.
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

	// 무작위 셔플 후 앞에서 nMax 개 추출.
	std::mt19937 rng{ std::random_device{}() };
	std::shuffle(candidates.begin(), candidates.end(), rng);
	// windows.h 의 min 매크로 충돌을 피하기 위해 삼항 연산자 사용.
	const size_t cap = (nMax < 0) ? 0u : static_cast<size_t>(nMax);
	const size_t take = (candidates.size() < cap) ? candidates.size() : cap;
	result.assign(candidates.begin(), candidates.begin() + take);
	return result;
}

// =====================================================================================
// A* 길찾기 — 그리드 셀 기반 최단 경로 탐색
// =====================================================================================
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

    // 월드 좌표 → 그리드 (열, 행) 변환
    auto WorldToGrid = [&](const XMFLOAT3& pos, int& outC, int& outR) {
        outC = static_cast<int>(floorf((pos.x + halfX) / TILE + 0.5f));
        outR = static_cast<int>(floorf((pos.z + halfZ) / TILE + 0.5f));
    };

    // 그리드 (열, 행) → 월드 좌표 중심점 변환
    auto GridToWorld = [&](int c, int r) -> XMFLOAT3 {
        const float worldX = c * TILE - halfX;
        const float worldZ = r * TILE - halfZ;
        const float floorY = GetFloorHeightAt(state, worldX, worldZ);
        return XMFLOAT3(worldX, floorY, worldZ);
    };

    int startC, startR, goalC, goalR;
    WorldToGrid(start, startC, startR);
    WorldToGrid(goal,  goalC,  goalR);

    // 범위 체크
    if (startC < 0 || startC >= cols || startR < 0 || startR >= rows) return {};
    if (goalC  < 0 || goalC  >= cols || goalR  < 0 || goalR  >= rows) return {};
    if (startC == goalC && startR == goalR) return {};

    // 셀 통과 가능 여부: 'W' 만 불통 (단차는 TryMoveXZ 점프가 처리)
    auto isWalkable = [&](int c, int r) -> bool {
        if (c < 0 || c >= cols || r < 0 || r >= rows) return false;
        return grid[r][c] != 'W';
    };

    // 평탄화 인덱스 헬퍼
    auto idx2 = [&](int c, int r) -> int { return r * cols + c; };
    const int N = rows * cols;

    // A* 상태 배열
    std::vector<float> gCost(N, 1e30f);
    std::vector<bool>  closed(N, false);
    std::vector<int>   parent(N, -1);  // 부모 셀 평탄화 인덱스

    // 오픈셋: {f, 평탄화 인덱스}  (min-heap)
    using PQItem = std::pair<float, int>;
    std::priority_queue<PQItem, std::vector<PQItem>, std::greater<PQItem>> openSet;

    auto heuristic = [&](int c, int r) -> float {
        // 맨해튼 거리 × 셀 크기
        return static_cast<float>(abs(c - goalC) + abs(r - goalR)) * TILE;
    };

    const int startIdx = idx2(startC, startR);
    gCost[startIdx] = 0.0f;
    openSet.emplace(heuristic(startC, startR), startIdx);

    // 4방향 이웃 (대각선 제외 — 벽 모서리 통과 방지)
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
            // 경로 역추적 (start 제외, goal 포함)
            std::vector<XMFLOAT3> path;
            int pIdx = goalIdx;
            while (pIdx != startIdx) {
                const int pc = pIdx % cols;
                const int pr = pIdx / cols;
                path.push_back(GridToWorld(pc, pr));
                pIdx = parent[pIdx];
                if (pIdx < 0) break;  // 안전망
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

    return {};  // 도달 불가
}
