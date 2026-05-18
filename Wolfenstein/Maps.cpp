#include "stdafx.h"
#include "Maps.h"
#include "Mesh.h"

namespace {
	// 맵 구성을 위한 기본 단위 크기 상수들.
	constexpr float TILE_WIDTH = 4.0f;
	constexpr float TILE_DEPTH = 4.0f;
	constexpr float FLOOR_HEIGHT = 0.5f;
	constexpr float WALL_HEIGHT = 4.0f;
	constexpr float STEP_HEIGHT = 1.0f;

	// 지정한 위치/크기/색으로 큐브 게임 오브젝트를 만들어 vObjects 에 추가한다.
	// 큐브 메시는 균일 색상 모드로 생성되어 각 맵의 테마 색을 반영한다.
	void AddCube(
		ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		std::vector<std::shared_ptr<CGameObject>>& vObjects,
		XMFLOAT3 xmf3Position, XMFLOAT3 xmf3Size, XMFLOAT4 xmf4Color)
	{
		auto pMesh = std::make_shared<CCubeMeshDiffused>(
			pd3dDevice, pd3dCommandList,
			xmf3Size.x, xmf3Size.y, xmf3Size.z,
			true, xmf4Color);

		auto pObj = std::make_shared<CGameObject>();
		pObj->SetMesh(pMesh);
		pObj->SetPosition(xmf3Position);
		vObjects.push_back(std::move(pObj));
	}
}

void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects)
{
	// 맵 1: 붉은 성채. 11x11 정사각형 바닥과 외곽 벽, 중앙에 계단/단상.
	const XMFLOAT4 colorFloorA{ 0.35f, 0.10f, 0.10f, 1.0f };
	const XMFLOAT4 colorFloorB{ 0.50f, 0.15f, 0.10f, 1.0f };
	const XMFLOAT4 colorWall  { 0.60f, 0.18f, 0.18f, 1.0f };
	const XMFLOAT4 colorStair { 1.00f, 0.55f, 0.10f, 1.0f };
	const XMFLOAT4 colorTop   { 1.00f, 0.85f, 0.20f, 1.0f };

	constexpr int GRID = 11;
	const float HALF = (GRID - 1) * TILE_WIDTH * 0.5f;

	// 바닥 타일 (체크무늬)
	for (int x = 0; x < GRID; ++x) {
		for (int z = 0; z < GRID; ++z) {
			XMFLOAT3 pos(x * TILE_WIDTH - HALF, -FLOOR_HEIGHT * 0.5f, z * TILE_DEPTH - HALF);
			XMFLOAT4 c = ((x + z) % 2 == 0) ? colorFloorA : colorFloorB;
			AddCube(pd3dDevice, pd3dCommandList, vObjects, pos,
				XMFLOAT3(TILE_WIDTH, FLOOR_HEIGHT, TILE_DEPTH), c);
		}
	}

	// 외곽 벽 (네 변 모두)
	for (int i = 0; i < GRID; ++i) {
		float fa = i * TILE_WIDTH - HALF;
		// 남쪽 / 북쪽 벽
		AddCube(pd3dDevice, pd3dCommandList, vObjects,
			XMFLOAT3(fa, WALL_HEIGHT * 0.5f, -HALF - TILE_DEPTH),
			XMFLOAT3(TILE_WIDTH, WALL_HEIGHT, TILE_DEPTH), colorWall);
		AddCube(pd3dDevice, pd3dCommandList, vObjects,
			XMFLOAT3(fa, WALL_HEIGHT * 0.5f, +HALF + TILE_DEPTH),
			XMFLOAT3(TILE_WIDTH, WALL_HEIGHT, TILE_DEPTH), colorWall);
		// 서쪽 / 동쪽 벽
		AddCube(pd3dDevice, pd3dCommandList, vObjects,
			XMFLOAT3(-HALF - TILE_WIDTH, WALL_HEIGHT * 0.5f, fa),
			XMFLOAT3(TILE_WIDTH, WALL_HEIGHT, TILE_DEPTH), colorWall);
		AddCube(pd3dDevice, pd3dCommandList, vObjects,
			XMFLOAT3(+HALF + TILE_WIDTH, WALL_HEIGHT * 0.5f, fa),
			XMFLOAT3(TILE_WIDTH, WALL_HEIGHT, TILE_DEPTH), colorWall);
	}

	// 중앙에 5단 계단 (요구사항 3: 비-평탄 지형)
	constexpr int STEPS = 5;
	for (int s = 0; s < STEPS; ++s) {
		float y = FLOOR_HEIGHT + STEP_HEIGHT * (s + 0.5f);
		float zCenter = (s - STEPS * 0.5f) * TILE_DEPTH;
		AddCube(pd3dDevice, pd3dCommandList, vObjects,
			XMFLOAT3(0.0f, y, zCenter),
			XMFLOAT3(TILE_WIDTH * 1.5f, STEP_HEIGHT, TILE_DEPTH),
			colorStair);
	}

	// 계단 꼭대기에 단상 (계단 끝 너머로 살짝 이어진다)
	float topY = FLOOR_HEIGHT + STEP_HEIGHT * (STEPS + 1.0f);
	float topZ = (STEPS * 0.5f) * TILE_DEPTH + TILE_DEPTH * 0.5f;
	AddCube(pd3dDevice, pd3dCommandList, vObjects,
		XMFLOAT3(0.0f, topY, topZ),
		XMFLOAT3(TILE_WIDTH * 2.0f, STEP_HEIGHT, TILE_DEPTH * 2.0f),
		colorTop);
}

void BuildMap2Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects)
{
	// 맵 2: 푸른 정찰소. ㄱ자 모양 바닥, 한쪽에 단계형 피라미드, 반대쪽에 별도 경사 계단.
	const XMFLOAT4 colorFloorA { 0.10f, 0.20f, 0.40f, 1.0f };
	const XMFLOAT4 colorFloorB { 0.10f, 0.30f, 0.50f, 1.0f };
	const XMFLOAT4 colorWall   { 0.06f, 0.12f, 0.35f, 1.0f };
	const XMFLOAT4 colorPyrA   { 0.20f, 0.60f, 0.85f, 1.0f };
	const XMFLOAT4 colorPyrB   { 0.35f, 0.78f, 0.95f, 1.0f };
	const XMFLOAT4 colorPyrTop { 0.90f, 1.00f, 1.00f, 1.0f };
	const XMFLOAT4 colorRamp   { 0.10f, 0.55f, 0.70f, 1.0f };

	constexpr int GRID = 13;
	const float HALF = (GRID - 1) * TILE_WIDTH * 0.5f;

	// ㄱ자 바닥: 우상단 4x4 영역은 제외하여 맵 1과 명확히 구분되는 모양을 만든다.
	for (int x = 0; x < GRID; ++x) {
		for (int z = 0; z < GRID; ++z) {
			if (x >= GRID - 4 && z >= GRID - 4) continue;
			XMFLOAT3 pos(x * TILE_WIDTH - HALF, -FLOOR_HEIGHT * 0.5f, z * TILE_DEPTH - HALF);
			XMFLOAT4 c = ((x + z) % 2 == 0) ? colorFloorA : colorFloorB;
			AddCube(pd3dDevice, pd3dCommandList, vObjects, pos,
				XMFLOAT3(TILE_WIDTH, FLOOR_HEIGHT, TILE_DEPTH), c);
		}
	}

	// 외곽 벽 - 남쪽과 서쪽 두 면만 둘러쳐 ㄱ자 형상을 강조한다.
	for (int i = 0; i < GRID; ++i) {
		float fa = i * TILE_WIDTH - HALF;
		AddCube(pd3dDevice, pd3dCommandList, vObjects,
			XMFLOAT3(fa, WALL_HEIGHT * 0.5f, -HALF - TILE_DEPTH),
			XMFLOAT3(TILE_WIDTH, WALL_HEIGHT, TILE_DEPTH), colorWall);
		AddCube(pd3dDevice, pd3dCommandList, vObjects,
			XMFLOAT3(-HALF - TILE_WIDTH, WALL_HEIGHT * 0.5f, fa),
			XMFLOAT3(TILE_WIDTH, WALL_HEIGHT, TILE_DEPTH), colorWall);
	}

	// 단계형 피라미드 (좌측-앞쪽). 5단으로 점차 좁아지며 꼭대기에 밝은 단상.
	const float pyrX = -TILE_WIDTH * 2.0f;
	const float pyrZ = -TILE_DEPTH * 2.0f;
	constexpr int PYR_LEVELS = 5;
	for (int level = 0; level < PYR_LEVELS; ++level) {
		float side = (PYR_LEVELS - level) * TILE_WIDTH;
		float h = STEP_HEIGHT * 2.0f;
		float y = FLOOR_HEIGHT + h * (level + 0.5f);
		XMFLOAT4 c = (level == PYR_LEVELS - 1) ? colorPyrTop
			: (level % 2 == 0) ? colorPyrA : colorPyrB;
		AddCube(pd3dDevice, pd3dCommandList, vObjects,
			XMFLOAT3(pyrX, y, pyrZ),
			XMFLOAT3(side, h, side),
			c);
	}

	// 별도 경사 계단 (4단). 피라미드 반대편에 위치한다.
	constexpr int RAMP_STEPS = 4;
	const float rampX0 = +TILE_WIDTH * 2.0f;
	const float rampZ0 = +TILE_DEPTH * 1.5f;
	for (int s = 0; s < RAMP_STEPS; ++s) {
		float y = FLOOR_HEIGHT + STEP_HEIGHT * (s + 0.5f);
		float x = rampX0 + s * TILE_WIDTH;
		AddCube(pd3dDevice, pd3dCommandList, vObjects,
			XMFLOAT3(x, y, rampZ0),
			XMFLOAT3(TILE_WIDTH, STEP_HEIGHT, TILE_DEPTH * 1.5f),
			colorRamp);
	}
}
