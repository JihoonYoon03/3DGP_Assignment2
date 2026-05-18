#pragma once

#include "GameObject.h"

// 두 개의 서로 다른 인게임 맵을 구축하는 헬퍼들이다 (요구사항 2).
// 각 맵은 색이 입혀진 큐브 메시들로만 구성되어 제한 파이프라인을 준수하고,
// 다양한 높이의 계단/단상으로 비-평탄 지형(요구사항 3)을 충족한다.

// 맵 1: 붉은 성채 - 사각형 바닥, 외곽 벽, 중앙에 5단 계단과 꼭대기 단상.
void BuildMap1Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);

// 맵 2: 푸른 정찰소 - ㄱ자 모양 바닥, 단계형 피라미드와 별도 경사 계단.
void BuildMap2Objects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
	std::vector<std::shared_ptr<CGameObject>>& vObjects);
