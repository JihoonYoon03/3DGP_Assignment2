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
MapInfo GetMap1Info();
MapInfo GetMap2Info();