#pragma once

#include "GameObject.h"

// ==========================================================================
// CPlayer
// ==========================================================================
// 플레이어 캐릭터. CCharacter 가 제공하는 HP / 점프 물리 / 소총 부착 위에
// 플레이어 고유의 반동 타이머를 더한다. 입력/카메라/HUD 등 외부 시스템과의
// 연결은 CGameFramework 가 담당하며, CPlayer 는 순수 상태 컨테이너 역할을
// 한다 (적의 AI 와 대비되는 "조작 주체" 차이).
//
// 위치는 CGameObject::Get/SetPosition 으로 접근하고, HP / 점프 / 소총은
// CCharacter 가 제공하는 메서드로 접근한다.
class CPlayer : public CCharacter
{
public:
	CPlayer();
	virtual ~CPlayer();

	// === 반동 애니메이션 ===
	// FireBullet() 이 0.0f 로 리셋해 시작하고, UpdateRifleTransform() 이 3 개
	// 키프레임 (뒤로 후진 → 절반 복귀 → 원위치) 으로 소총 위치에 forward
	// 방향 오프셋을 더한다. 음수면 비활성.
	float GetRecoilTimer() const { return m_fRecoilTimer; }
	void  SetRecoilTimer(float t) { m_fRecoilTimer = t; }
	void  TickRecoil(float fTimeElapsed, float fRecoilDuration);

	// === 점프 물리 (CGameFramework 가 입력 기반으로 직접 갱신) ===
	// CCharacter 의 protected 멤버에 외부에서 접근할 수 있도록 한정된 셋터/게터.
	float GetVerticalVelocity() const { return m_fVerticalVelocity; }
	void  SetVerticalVelocity(float v) { m_fVerticalVelocity = v; }
	bool  IsGrounded() const { return !m_bJumping; }
	void  SetGrounded(bool b) { m_bJumping = !b; }

	// === 소총 ===
	// m_pRifle (CCharacter) 의 shared_ptr 직접 참조. CGameFramework 가 소총
	// 메시를 부착할 때와 외부에서 소총 객체에 접근할 때 사용한다.
	const std::shared_ptr<CGameObject>& GetRifleShared() const { return m_pRifle; }
	void SetRifleObject(std::shared_ptr<CGameObject> p) { m_pRifle = std::move(p); }

private:
	float m_fRecoilTimer = -1.0f;
};
