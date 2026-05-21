#pragma once

#include "GameObject.h"

// �� �ʿ� ���� ��Ÿ������.
// - cameraPosition / cameraLookAt: �ΰ��� 1��Ī �þ��� ���� ��ġ�� �ٶ󺸴� ����.
struct MapInfo {
	XMFLOAT3 cameraPosition;
	XMFLOAT3 cameraLookAt;
};

// �� ���� ���� �ٸ� Wolfenstein ��Ÿ�� �̷� ���� �����Ѵ� (�䱸���� 2).
// �� ���� ���� ������ ť�� �޽õ�θ� �����Ǿ� ���� ������������ �ؼ��ϰ�,
// ���� ���ο� �پ��� ������ ���/�ܻ��� ���ԵǾ� ��-��ź ����(�䱸���� 3)�� �����Ѵ�.

// �� 1: ������ ȸû�� �� �̷�. ���� �� �ڳʿ� 4�� ��ܰ� �ܻ�.
void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);

// �� 2: ������ ���� ���� ���� �̷�. �߾ӿ� 6�� �ܻ����� �̾����� ��� ���.
void BuildMap2Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);

// �� ���� 1��Ī ī�޶� ���� ��ġ/������ ��ȯ�Ѵ�.
// �÷��̾��� 1��Ī �� ����. MapInfo �� ProcessInput ���ʿ��� ��� ����Ѵ�.
constexpr float MAP_EYE_HEIGHT = 3.5f;

MapInfo GetMap1Info();
MapInfo GetMap2Info();

// Scene.h 간접 순환 의존성을 피하기 위한 전방 선언.
enum class SceneState : int;

// 그리드 한 셀의 월드 크기 (Maps.cpp 내 TILE 상수와 동일).
// A* 경유 거리 판정 등에 외부에서 사용한다.
// (헤더에 정의되지만 constexpr float 는 내부 링키지를 가져 ODR 위반 없음)
static constexpr float MAP_TILE_SIZE = 4.0f;

// ���� ���� (x,z) ���� �÷��̾��� �� ����(fFeetY)�� �������� �̵��� �������� �����Ѵ�.
// ���� �ٱ��̳� W(��), �ʹ� ���� ���� ��� �������� ó���Ѵ�.
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
