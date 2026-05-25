#include "stdafx.h"
#include "Player.h"
#include "Maps.h"

CPlayer::CPlayer()
{
	m_eTag = EObjectTag::Player;
	m_nHP = 10;                              // 플레이어 기본 라이프 10
	// 플레이어 AABB 는 충돌 판정 단계에서 직접 사용하지 않으나 (이동은 반경
	// 기반 프로브 방식), 일관성을 위해 적과 비슷한 크기로 잡는다.
	m_xmf3AABBHalf = XMFLOAT3(1.0f, MAP_EYE_HEIGHT * 0.5f, 1.0f);
}

CPlayer::~CPlayer()
{
}

void CPlayer::TickRecoil(float fTimeElapsed, float fRecoilDuration)
{
	if (m_fRecoilTimer < 0.0f) return;
	m_fRecoilTimer += fTimeElapsed;
	if (m_fRecoilTimer >= fRecoilDuration) m_fRecoilTimer = -1.0f;
}
