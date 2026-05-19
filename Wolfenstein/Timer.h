#pragma once

const ULONG MAX_SAMPLE_COUNT = 50; // 최근 50 프레임의 처리 시간을 누적하여 평균낸다.

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
	float GetTimeElapsed();	// 프레임의 평균 경과 시간 반환

private:
	bool	m_bHardwareHasPerformanceCounter;	// 하드웨어가 Performance Counter 를 지원하는가
	float	m_fTimeScale;						// 카운터 스케일 값
	float	m_fTimeElapsed;						// 마지막 프레임 이후 경과 시간
	__int64 m_nCurrentTime;						// 현재 시간
	__int64 m_nLastTime;						// 마지막 프레임의 시간
	__int64 m_nPerformanceFrequency;			// Performance Frequency

	float	m_fFrameTime[MAX_SAMPLE_COUNT];		// 프레임 시간을 누적하기 위한 배열
	ULONG	m_nSampleCount;						// 누적된 프레임 샘플 수

	unsigned long	m_nCurrentFrameRate;		// 현재 프레임 레이트
	unsigned long	m_nFramesPerSecond;			// 초당 프레임 수
	float			m_fFPSTimeElapsed;			// 프레임 레이트 계산 누적 시간

	bool m_bStopped;
};
