#pragma once

const ULONG MAX_SAMPLE_COUNT = 50; // 최근 50 프레임의 경과 시간을 평균하여 사용한다.

class CGameTimer
{
public:
	CGameTimer();
	virtual ~CGameTimer();

	void Start() {}
	void Stop() {}
	void Reset();
	void Tick(float fLockFPS = 0.0f);	// 프레임 시간 갱신
	unsigned long GetFrameRate(LPTSTR lpszString = NULL, int nCharacters = 0);	// 프레임 레이트 반환
	float GetTimeElapsed();	// 이전 프레임 이후 경과 시간 반환

private:
	bool	m_bHardwareHasPerformanceCounter;	// 하드웨어 Performance Counter 가 있는지 여부
	float	m_fTimeScale;						// 타이머 스케일 값
	float	m_fTimeElapsed;						// 이전 프레임 이후의 경과 시간
	__int64 m_nCurrentTime;						// 현재 시간
	__int64 m_nLastTime;						// 이전 프레임의 시간
	__int64 m_nPerformanceFrequency;			// Performance Frequency

	float	m_fFrameTime[MAX_SAMPLE_COUNT];		// 프레임 시간을 평균하기 위한 배열
	ULONG	m_nSampleCount;						// 프레임 샘플의 현재 수

	unsigned long	m_nCurrentFrameRate;		// 현재 프레임 레이트
	unsigned long	m_nFramesPerSecond;			// 초당 프레임 수
	float			m_fFPSTimeElapsed;			// 프레임 레이트 계산을 위한 경과 시간

	bool m_bStopped;
};
