#pragma once

#include "GameObject.h"
#include <functional>
#include <memory>
#include <vector>

// Generalized collision query. Pass the object container and two tags; the
// helper visits every (a, b) pair where a has tagA and b has tagB and their
// axis-aligned bounding boxes overlap, invoking `cb` for each overlap.
//
// Adding a new participant -- e.g. an enemy actor next sprint -- only
// requires that the actor sets its tag to EObjectTag::Enemy and lives in the
// same per-scene object list. Bullets vs. enemies, enemies vs. player, etc.
// are all expressed as a single call here with the appropriate tag pair.
//
// tagA == tagB is allowed; in that case each unordered pair is reported
// once. Dead objects (IsAlive() == false) are skipped on both sides.
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

} // namespace Collision
