#pragma once

#include "GameObject.h"

class CPlayer : public CCharacter
{
public:
	CPlayer();
	virtual ~CPlayer();

	float GetRecoilTimer() const { return m_fRecoilTimer; }
	void  SetRecoilTimer(float t) { m_fRecoilTimer = t; }
	void  TickRecoil(float fTimeElapsed, float fRecoilDuration);

	float GetVerticalVelocity() const { return m_fVerticalVelocity; }
	void  SetVerticalVelocity(float v) { m_fVerticalVelocity = v; }
	bool  IsGrounded() const { return !m_bJumping; }
	void  SetGrounded(bool b) { m_bJumping = !b; }

	const std::shared_ptr<CGameObject>& GetRifleShared() const { return m_pRifle; }
	void SetRifleObject(std::shared_ptr<CGameObject> p) { m_pRifle = std::move(p); }

private:
	float m_fRecoilTimer = -1.0f;
};
