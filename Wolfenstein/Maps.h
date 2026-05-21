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

// Scene.h ���� ��ȯ ������ ���ϱ� ���� ���� ����.
enum class SceneState : int;

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

// 30x30 그리드 기반 A* 4방향 최단 경로 계산. (cx,cz) → (tx,tz) world XZ.
// 통과 가능 여부는 IsBlockedInMap 과 동일 (fFeetY 기준). 경로가 없으면 false.
// 결과 outPathXZ 는 출발 셀 다음부터 도착 셀까지의 셀 중심 world (x,z) 시퀀스.
bool ComputeShortestPathXZ(SceneState state,
	float cx, float cz, float tx, float tz, float fFeetY,
	std::vector<XMFLOAT2>& outPathXZ);
