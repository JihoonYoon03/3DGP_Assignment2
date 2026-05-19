#include "stdafx.h"
#include "Maps.h"
#include "Mesh.h"
#include "Scene.h"
#include <string>
#include <random>
#include <queue>
#include <algorithm>

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

	// ���� �׸��带 �޾� �̷� ť�긦 ��ġ�Ѵ�.
	// �� ���� �ǹ�:
	//   'W' : �� (���̴� wallHeights ���� ���� ����, 0 �̸� �⺻ WALL_H)
	//   '.' : ��� (���� 0)
	//   '1' ~ '9' : �ش� ĭ ���� STEP_H * n ������ ���� ��
	//   'P' : �ܻ� (STEP_H * 6 ����)
	// �� �� ���ڴ� �ٴڸ� ���.
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

		// Floor cells '1'..'5' encode height steps 1..5. There is no longer a
		// separate yellow stair plate: the floor tile itself rises so its top
		// face sits at step * STEP_H. The colorStair param is repurposed as
		// the gradient endpoint, and colorPlatform is no longer used.
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

				// Floor or stepped floor. step == 0 is the legacy '.' level.
				int step = 0;
				if (ch >= '1' && ch <= '5') step = ch - '0';

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

	// Weighted sample for the region-to-region height delta. Most regions
	// stay close to their neighbors (delta 0 or +-1) so the floor reads as
	// gently varied, but a 25% chance of +-2 and 10% chance of +-3 gives
	// occasional jumps that the player must clear with the jump key.
	int SampleHeightDelta(std::mt19937& rng)
	{
		const unsigned r = rng() % 100u;
		if (r < 30u) return 0;
		if (r < 65u) return ((rng() & 1u) ? 1 : -1);
		if (r < 90u) return ((rng() & 1u) ? 2 : -2);
		return ((rng() & 1u) ? 3 : -3);
	}

	void PartitionFloorRegions(std::vector<std::string>& grid, std::mt19937& rng)
	{
		const int rows = static_cast<int>(grid.size());
		const int cols = static_cast<int>(grid[0].size());

		// 1. ���� �� ���� (���� ���� (1,1))
		const int startR = 1, startC = 1;
		std::vector<std::pair<int, int>> walkable;
		for (int r = 0; r < rows; ++r) {
			for (int c = 0; c < cols; ++c) {
				if (grid[r][c] == '.') walkable.emplace_back(r, c);
			}
		}
		if (walkable.empty()) return;

		// 2. K ���� �õ� ����. ���� ���� �׻� ù �õ�� ����.
		// Fewer seeds -> larger average region so the "same height runs >= 3 tiles"
		// requirement is satisfied for the vast majority of regions, and tiny
		// stragglers are merged below.
		const int K = (std::max)(3, static_cast<int>(walkable.size()) / 120);
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

		// 3. ���� �ҽ� BFS: ��� '.' ���� ���� ����� �õ��� regionId �� �ο�.
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

		// Merge tiny regions (< 3 cells) into their largest neighbor so every
		// remaining region runs for at least 3 contiguous tiles. We loop until
		// no small region remains.
		{
			const int K_REG_TMP = static_cast<int>(seeds.size());
			std::vector<int> sizes(K_REG_TMP, 0);
			auto recomputeSizes = [&]() {
				std::fill(sizes.begin(), sizes.end(), 0);
				for (int r = 0; r < rows; ++r)
					for (int c = 0; c < cols; ++c)
						if (regionId[r][c] >= 0) sizes[regionId[r][c]]++;
			};
			bool changed = true;
			int guard = 0;
			while (changed && guard++ < K_REG_TMP * 2) {
				changed = false;
				recomputeSizes();
				int target = -1;
				for (int rid = 0; rid < K_REG_TMP; ++rid) {
					if (sizes[rid] > 0 && sizes[rid] < 3) { target = rid; break; }
				}
				if (target < 0) break;
				int bestNb = -1, bestSize = -1;
				for (int r = 0; r < rows && bestSize < (int)sizes.size(); ++r) {
					for (int c = 0; c < cols; ++c) {
						if (regionId[r][c] != target) continue;
						for (int k = 0; k < 4; ++k) {
							int nr = r + d4[k][0], nc = c + d4[k][1];
							if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) continue;
							int oid = regionId[nr][nc];
							if (oid < 0 || oid == target) continue;
							if (sizes[oid] > bestSize) { bestSize = sizes[oid]; bestNb = oid; }
						}
					}
				}
				if (bestNb < 0) break;
				for (int r = 0; r < rows; ++r)
					for (int c = 0; c < cols; ++c)
						if (regionId[r][c] == target) regionId[r][c] = bestNb;
				changed = true;
			}
		}

		// 4. ���� ���� �׷��� ����
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

		// 5. ���� �������� BFS �� ���� ����. ���� ���� ���̸� -1, 0, +1 �� �ϳ��� ����.
		std::vector<int> heightStep(K_REG, -1);
		const int startRegion = regionId[startR][startC];
		heightStep[startRegion] = 0;
		std::queue<int> hq;
		hq.push(startRegion);
		while (!hq.empty()) {
			int cur = hq.front(); hq.pop();
			for (int nb : adj[cur]) {
				if (heightStep[nb] != -1) continue;
				// Weighted delta in [-3, +3]: mostly small differences, occasional
				// 2- or 3-step drops that the player must clear with a jump
				// (apex ~= 3 * STEP_H).
				int delta = SampleHeightDelta(rng);
				int cand = heightStep[cur] + delta;
				if (cand < 0) cand = 0;
				if (cand > 5) cand = 5;
				heightStep[nb] = cand;
				hq.push(nb);
			}
		}

		// 6. ����� �׸��忡 ���. ���� 0 �� '.', 1~3 �� '1'~'3' ���� ����.
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
// ===================== �浹 ó�� =====================
// �÷��̾� �� ����(fFeetY) �� ������� (x,z) ���� �̵��� �������� �����Ѵ�.
// W : �׻� ����.
// 1~9 / P : ���� ������ fFeetY + STEP_UP_TOLERANCE ���� ������ ����.
// '.' : �׻� ���.
// ���� �ٱ��� ��� ����.
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
