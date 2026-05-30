#pragma once

#include "GameObject.h"
#include <functional>
#include <memory>
#include <vector>

// enum 기반 AABB 충돌 검사.
// tagA × tagB 의 모든 쌍에 대해 AABB 가 겹치면 cb 콜백을 호출한다.
// tagA == tagB 일 때는 unordered 쌍을 한 번씩만 보고한다.
namespace Collision {

	using HitCallback = std::function<void(CGameObject* a, CGameObject* b)>;

	inline bool AABBOverlap(const CGameObject& a, const CGameObject& b)
	{
		const XMFLOAT4X4& ma = a.GetWorldMatrixRef();
		const XMFLOAT4X4& mb = b.GetWorldMatrixRef();
		const XMFLOAT3& ha = a.GetAABBHalf();
		const XMFLOAT3& hb = b.GetAABBHalf();
		const float dx = ma._41 - mb._41;
		const float dy = ma._42 - mb._42;
		const float dz = ma._43 - mb._43;
		const float rx = ha.x + hb.x;
		const float ry = ha.y + hb.y;
		const float rz = ha.z + hb.z;
		return (fabsf(dx) <= rx) && (fabsf(dy) <= ry) && (fabsf(dz) <= rz);
	}

	inline void CheckCollisions(
		const std::vector<std::shared_ptr<CGameObject>>& objs,
		EObjectTag tagA, EObjectTag tagB,
		HitCallback cb)
	{
		if (!cb) return;
		std::vector<CGameObject*> as;
		std::vector<CGameObject*> bs;
		as.reserve(objs.size());
		bs.reserve(objs.size());
		for (const auto& sp : objs) {
			if (!sp || !sp->IsAlive()) continue;
			if (sp->GetTag() == tagA) as.push_back(sp.get());
			if (tagA != tagB && sp->GetTag() == tagB) bs.push_back(sp.get());
		}
		if (tagA == tagB) {
			for (size_t i = 0; i < as.size(); ++i) {
				for (size_t j = i + 1; j < as.size(); ++j) {
					if (!as[i]->IsAlive() || !as[j]->IsAlive()) continue;
					if (AABBOverlap(*as[i], *as[j])) cb(as[i], as[j]);
				}
			}
		}
		else {
			for (auto* pa : as) {
				if (!pa->IsAlive()) continue;
				for (auto* pb : bs) {
					if (!pb->IsAlive()) continue;
					if (AABBOverlap(*pa, *pb)) cb(pa, pb);
				}
			}
		}
	}

}
