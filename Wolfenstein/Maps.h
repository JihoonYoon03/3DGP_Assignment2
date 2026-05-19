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

// (x,z) ��ġ���� ���� ���� �ٴ�(Ȥ�� ��)�� Y ��ǥ�� ��ȯ�Ѵ�.
// '.' �� 0, '1'~'9' �� STEP_H * digit, 'P' �� STEP_H * 6, 'W'/���� �ٱ��� 0.
float GetFloorHeightAt(SceneState state, float x, float z);

// Walk a horizontal ray from `fromXZ` in `dirXZ` direction up to `maxDist`
// world units and return the distance just before the ray runs into a wall
// (or a step that the camera at height `eyeY` cannot clear). Used by the
// TPS spring-arm so the camera does not penetrate level geometry.
float ClampDistanceAgainstWalls(SceneState state,
	XMFLOAT3 fromXZ, XMFLOAT3 dirXZ, float maxDist, float eyeY);
