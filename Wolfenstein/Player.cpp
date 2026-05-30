#include "stdafx.h"
#include "Player.h"
#include "Maps.h"

CPlayer::CPlayer()
{
	m_eTag = EObjectTag::Player;
	m_nHP = 10;
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
