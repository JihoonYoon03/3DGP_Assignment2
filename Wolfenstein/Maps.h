#pragma once

#include "GameObject.h"

// 각 맵의 정보를 나타내는 구조체이다.
// - cameraPosition / cameraLookAt: 게임의 1인칭 시야의 시작 위치와 바라보는 방향.
struct MapInfo {
	XMFLOAT3 cameraPosition;
	XMFLOAT3 cameraLookAt;
};

// 서로 다른 색감의 Wolfenstein 스타일 미로 두 개를 생성한다 (요구사항 2).
// 각 맵은 절차적으로 생성된 큐브 메시들로 구성되어 격자 좌표계에서 동작하고,
// 맵 내부에 다양한 높이의 단차/계단이 포함되어 비-평면 지형(요구사항 3)을 만족한다.

// 맵 1: 어두운 회청색 톤 미로. 시작 점 근처에 4단 계단과 단차 존재.
void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);

// 맵 2: 따뜻한 갈색 톤 모래성 같은 미로. 중앙에 6층 단상으로 이어지는 계단 배치.
void BuildMap2Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);

// 각 맵의 1인칭 카메라 시작 위치/방향을 반환한다.
// 플레이어의 1인칭 눈 높이. MapInfo 와 ProcessInput 양쪽에서 모두 참조한다.
constexpr float MAP_EYE_HEIGHT = 3.5f;

MapInfo GetMap1Info();
MapInfo GetMap2Info();

// Scene.h 간접 순환 의존성을 피하기 위한 전방 선언.
enum class SceneState : int;

// 그리드 한 셀의 월드 크기 (Maps.cpp 내 TILE 상수와 동일).
// A* 경유 거리 판정 등에 외부에서 사용한다.
// (헤더에 정의되지만 constexpr float 는 내부 링키지를 가져 ODR 위반 없음)
static constexpr float MAP_TILE_SIZE = 4.0f;

// 격자 좌표 (x,z) 에서 플레이어의 발 높이(fFeetY)를 기준으로 이동이 가능한지 판정한다.
// 맵 바깥이나 W(벽), 너무 높은 단차 위는 막힌 것으로 간주한다.
bool IsBlockedInMap(SceneState state, float x, float z, float fFeetY);

// (x,z) 위치에서 현재 맵의 바닥(또는 단차) 윗면 Y 좌표를 반환한다.
// '.' 은 0, '1'~'3' 은 STEP_H * digit, 'W'/지도 바깥은 0 으로 처리.
float GetFloorHeightAt(SceneState state, float x, float z);

// Walk a horizontal ray from `fromXZ` in `dirXZ` direction up to `maxDist`
// world units and return the distance just before the ray runs into a wall
// (or a step that the camera at height `eyeY` cannot clear). Used by the
// TPS spring-arm so the camera does not penetrate level geometry.
float ClampDistanceAgainstWalls(SceneState state,
	XMFLOAT3 fromXZ, XMFLOAT3 dirXZ, float maxDist, float eyeY);

// 두 점(from, to) 사이의 수평 시야가 벽/높은 단차에 막히지 않는지 확인한다.
// 양 끝 셀은 검사에서 제외하여 시야 송신자/수신자 본인의 셀이 단차여도
// 자기 자신 때문에 막힘 처리되지 않게 한다.
// 적 AI의 LOS(시야) 판정에 사용된다.
bool HasLineOfSight(SceneState state, XMFLOAT3 from, XMFLOAT3 to, float eyeY);

// 현재 맵에서 적이 스폰 가능한 위치(평면/단차 바닥) 중 플레이어의 시작
// 타일과 Chebyshev 거리 ≥ 5 인 것 중 무작위로 최대 nMax 개를 선택해 반환한다.
// 반환되는 XMFLOAT3 의 Y 는 해당 타일의 바닥 + halfBodyY 로 설정되어 있어
// 그대로 적의 위치로 사용 가능하다.
std::vector<XMFLOAT3> PickEnemySpawnPositions(SceneState state,
	XMFLOAT3 xmf3PlayerStart, int nMax, float fHalfBodyY = 1.3f);

// A* 알고리즘으로 start 에서 goal 까지 그리드 셀 중심 경유 경로를 반환한다.
// 반환 벡터는 start 셀 제외 + goal 셀 포함 순서로 정렬된 월드 좌표 목록이다.
// 도달 불가 시 빈 벡터를 반환한다. nMaxNodes 로 탐색 상한을 제한해 비용 폭주 방지.
// 'W' 셀만 불통으로 처리하고 단차('1'~'3')는 통과 가능 취급한다
// (실제 이동 시 TryMoveXZ 의 점프 메커니즘이 단차를 처리함).
std::vector<XMFLOAT3> FindPathAStar(
	SceneState state,
	const XMFLOAT3& start,
	const XMFLOAT3& goal,
	int nMaxNodes = 1024);
