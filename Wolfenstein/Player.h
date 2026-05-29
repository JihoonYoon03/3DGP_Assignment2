#pragma once

#include "GameObject.h"

// 플레이어 캐릭터. CCharacter 의 HP/점프/소총 위에 반동 타이머만 추가한다.
class CPlayer : public CCharacter
{
public:
	CPlayer();
	virtual ~CPlayer();

	// 반동 타이머 (FireBullet 이 0 으로 리셋 → UpdateRifleTransform 가 키프레임 보간)
	float GetRecoilTimer() const { return m_fRecoilTimer; }
	void  SetRecoilTimer(float t) { m_fRecoilTimer = t; }
	void  TickRecoil(float fTimeElapsed, float fRecoilDuration);

	// 점프 물리 접근자 (CGameFramework 가 입력으로 갱신)
	float GetVerticalVelocity() const { return m_fVerticalVelocity; }
	void  SetVerticalVelocity(float v) { m_fVerticalVelocity = v; }
	bool  IsGrounded() const { return !m_bJumping; }
	void  SetGrounded(bool b) { m_bJumping = !b; }

	// 소총 객체 접근
	const std::shared_ptr<CGameObject>& GetRifleShared() const { return m_pRifle; }
	void SetRifleObject(std::shared_ptr<CGameObject> p) { m_pRifle = std::move(p); }

private:
	float m_fRecoilTimer = -1.0f;
};
