#pragma once

#include "GameObject.h"

// 각 맵의 1인칭 카메라 시작 정보
struct MapInfo {
	XMFLOAT3 cameraPosition;
	XMFLOAT3 cameraLookAt;
};

// 맵 1
void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);

// 맵 2
void BuildMap2Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);

// 플레이어 1인칭 눈 높이
constexpr float MAP_EYE_HEIGHT = 3.5f;

MapInfo GetMap1Info();
MapInfo GetMap2Info();

enum class SceneState : int;

// 그리드 한 셀의 월드 크기
static constexpr float MAP_TILE_SIZE = 4.0f;

// x, z 위치 이동 가능 여부 판정
bool IsBlockedInMap(SceneState state, float x, float z, float fFeetY);

// x, z 위치의 바닥 높이 반환
float GetFloorHeightAt(SceneState state, float x, float z);

// 스프링 암
float ClampDistanceAgainstWalls(SceneState state,
	XMFLOAT3 fromXZ, XMFLOAT3 dirXZ, float maxDist, float eyeY);

// 적 AI의 플레이어 포착 여부 판별 함수
bool HasLineOfSight(SceneState state, XMFLOAT3 from, XMFLOAT3 to, float eyeY);

// 적 스폰 가능 위치 중 플레이어로부터 떨어진 곳을 무작위로 선택
std::vector<XMFLOAT3> PickEnemySpawnPositions(SceneState state,
	XMFLOAT3 xmf3PlayerStart, int nMax, float fHalfBodyY = 1.3f);

// A* 알고리즘으로 start 에서 goal 까지의 경로 반환
std::vector<XMFLOAT3> FindPathAStar(
	SceneState state,
	const XMFLOAT3& start,
	const XMFLOAT3& goal,
	int nMaxNodes = 1024);
