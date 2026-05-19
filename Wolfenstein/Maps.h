#pragma once

#include "GameObject.h"

// 한 맵에 대한 메타데이터.
// - cameraPosition / cameraLookAt: 인게임 1인칭 시야의 시작 위치와 바라보는 지점.
struct MapInfo {
	XMFLOAT3 cameraPosition;
	XMFLOAT3 cameraLookAt;
};

// 두 개의 서로 다른 Wolfenstein 스타일 미로 맵을 구축한다 (요구사항 2).
// 각 맵은 색이 입혀진 큐브 메시들로만 구성되어 제한 파이프라인을 준수하고,
// 격자 내부에 다양한 높이의 계단/단상이 포함되어 비-평탄 지형(요구사항 3)을 만족한다.

// 맵 1: 차가운 회청색 돌 미로. 동쪽 끝 코너에 4단 계단과 단상.
void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);

// 맵 2: 따뜻한 갈색 던전 십자 미로. 중앙에 6단 단상으로 이어지는 양면 계단.
void BuildMap2Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);

// 각 맵의 1인칭 카메라 시작 위치/시점을 반환한다.
// 플레이어의 1인칭 눈 높이. MapInfo 와 ProcessInput 양쪽에서 모두 사용한다.
constexpr float MAP_EYE_HEIGHT = 3.5f;

MapInfo GetMap1Info();
MapInfo GetMap2Info();

// Scene.h 와의 순환 참조를 피하기 위한 전방 선언.
enum class SceneState : int;

// 현재 맵의 (x,z) 에서 플레이어의 발 높이(fFeetY)를 기준으로 이동이 막히는지 판정한다.
// 격자 바깥이나 W(벽), 너무 높은 단은 모두 막힘으로 처리한다.
bool IsBlockedInMap(SceneState state, float x, float z, float fFeetY);

// (x,z) 위치에서 발이 닿을 바닥(혹은 단)의 Y 좌표를 반환한다.
// '.' 는 0, '1'~'9' 는 STEP_H * digit, 'P' 는 STEP_H * 6, 'W'/격자 바깥은 0.
float GetFloorHeightAt(SceneState state, float x, float z);
