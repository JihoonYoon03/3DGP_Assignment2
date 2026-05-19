#pragma once

const ULONG MAX_SAMPLE_COUNT = 50; // �ֱ� 50 �������� ó�� �ð��� �����Ͽ� ��ճ���.

class CGameTimer
{
public:
	CGameTimer();
	virtual ~CGameTimer();

	void Start() {}
	void Stop() {}
	void Reset();
	void Tick(float fLockFPS = 0.0f);	// ������ �ð� ����
	unsigned long GetFrameRate(LPTSTR lpszString = NULL, int nCharacters = 0);	// ������ ����Ʈ ��ȯ
	float GetTimeElapsed();	// �������� ��� ��� �ð� ��ȯ

private:
	bool	m_bHardwareHasPerformanceCounter;	// �ϵ��� Performance Counter �� �����ϴ°�
	float	m_fTimeScale;						// ī���� ������ ��
	float	m_fTimeElapsed;						// ������ ������ ���� ��� �ð�
	__int64 m_nCurrentTime;						// ���� �ð�
	__int64 m_nLastTime;						// ������ �������� �ð�
	__int64 m_nPerformanceFrequency;			// Performance Frequency

	float	m_fFrameTime[MAX_SAMPLE_COUNT];		// ������ �ð��� �����ϱ� ���� �迭
	ULONG	m_nSampleCount;						// ������ ������ ���� ��

	unsigned long	m_nCurrentFrameRate;		// ���� ������ ����Ʈ
	unsigned long	m_nFramesPerSecond;			// �ʴ� ������ ��
	float			m_fFPSTimeElapsed;			// ������ ����Ʈ ��� ���� �ð�

	bool m_bStopped;
};
