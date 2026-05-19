#include "stdafx.h"
#include "Timer.h"

CGameTimer::CGameTimer()
{
	if (::QueryPerformanceFrequency((LARGE_INTEGER*)&m_nPerformanceFrequency))
	{
		m_bHardwareHasPerformanceCounter = TRUE;
		::QueryPerformanceCounter((LARGE_INTEGER*)&m_nLastTime);
		m_fTimeScale = 1.0f / m_nPerformanceFrequency;
	}
	else
	{
		m_bHardwareHasPerformanceCounter = FALSE;
		//m_nLastTime = ::timeGetTime();
		m_fTimeScale = 0.001f;
	}
	m_nSampleCount = 0;
	m_nCurrentFrameRate = 0;
	m_nFramesPerSecond = 0;
	m_fFPSTimeElapsed = 0.0f;
}

CGameTimer::~CGameTimer()
{
}

void CGameTimer::Tick(float fLockFPS)
{
	if (m_bHardwareHasPerformanceCounter) {
		::QueryPerformanceCounter((LARGE_INTEGER*)&m_nCurrentTime);
	}
	else {
		//m_nCurrentTime = ::timeGetTime();
	}

	// ���������� �� �Լ��� ȣ���� ���� ����� �ð��� ����Ѵ�.
	float fTimeElapsed = (m_nCurrentTime - m_nLastTime) * m_fTimeScale;

	if (fLockFPS > 0.0f) {

		// �Ķ���� fLockFPS �� 0���� ũ�� ���� �ð� ���ݱ��� �Լ��� ��ٸ��� �Ѵ�.
		while (fTimeElapsed < (1.0f / fLockFPS)) {
			if (m_bHardwareHasPerformanceCounter) {
				::QueryPerformanceCounter((LARGE_INTEGER*)&m_nCurrentTime);
			}
			else {
				//m_nCurrentTime = ::timeGetTime();
			}
			// �ٽ� �� �� ��� �ð��� ����Ѵ�.
			fTimeElapsed = (m_nCurrentTime - m_nLastTime) * m_fTimeScale;
		}
	}
	// ���� �ð��� m_nLastTime �� �����Ѵ�.
	m_nLastTime = m_nCurrentTime;

	/*
	������ ������ ó�� �ð��� ���� ������ ó�� �ð��� ���̰� 1�ʺ��� ������
	���� ������ ó�� �ð��� m_fFrameTime[0] �� �����Ѵ�.
	*/
	if (fabsf(fTimeElapsed - m_fTimeElapsed) < 1.0f)
	{
		::memmove(&m_fFrameTime[1], m_fFrameTime, (MAX_SAMPLE_COUNT - 1) *
			sizeof(float));
		m_fFrameTime[0] = fTimeElapsed;
		if (m_nSampleCount < MAX_SAMPLE_COUNT) m_nSampleCount++;
	}

	// �ʴ� ������ ���� 1 ������Ű�� ���� ������ ó�� �ð��� ������ �д�.
	m_nFramesPerSecond++;
	m_fFPSTimeElapsed += fTimeElapsed;
	if (m_fFPSTimeElapsed > 1.0f)
	{
		m_nCurrentFrameRate = m_nFramesPerSecond;
		m_nFramesPerSecond = 0;
		m_fFPSTimeElapsed = 0.0f;
	}

	// ������ ������ ó�� �ð����� ����� ���Ͽ� ��� ������ �ð����� ��´�.
	m_fTimeElapsed = 0.0f;
	for (ULONG i = 0; i < m_nSampleCount; i++) {
		m_fTimeElapsed += m_fFrameTime[i];
	}

	if (m_nSampleCount > 0) {
		m_fTimeElapsed /= m_nSampleCount;
	}
}

unsigned long CGameTimer::GetFrameRate(LPTSTR lpszString, int nCharacters)
{
	// ���� ������ ����Ʈ�� ���ڿ��� ��ȯ�� lpszString ���ۿ� ä��� " FPS)" �� �����δ�.
	if (lpszString)	{
		_itow_s(m_nCurrentFrameRate, lpszString, nCharacters, 10);
		wcscat_s(lpszString, nCharacters, _T(" FPS)"));
	}

	return m_nCurrentFrameRate;
}

float CGameTimer::GetTimeElapsed()
{
	return m_fTimeElapsed;
}

void CGameTimer::Reset()
{
	__int64 nPerformanceCounter;
	::QueryPerformanceCounter((LARGE_INTEGER*)&nPerformanceCounter);
	m_nLastTime = nPerformanceCounter;
	m_nCurrentTime = nPerformanceCounter;
	m_bStopped = false;
}
