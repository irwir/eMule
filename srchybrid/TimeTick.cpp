// TimeTick.cpp : implementation of the CTimeTick class
//
/////////////////////////////////////////////////////////////////////////////
//
// Copyright © 2001, Stefan Belopotocan, http://welcome.to/BeloSoft
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TimeTick.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//////////////////////////////////////////////////////////////////////
// CTimeTick

__int64 CTimeTick::m_nPerformanceFrequency = CTimeTick::GetPerformanceFrequency();

CTimeTick::CTimeTick()
	: m_nTimeElapsed()
	, m_nTime()
{
}

void CTimeTick::Start()
{
	if (m_nPerformanceFrequency)
		QueryPerformanceCounter(&m_nTimeElapsed);
	m_nTime.QuadPart = 0;
}

float CTimeTick::Tick()
{
	LARGE_INTEGER nTime;

	if (m_nPerformanceFrequency) {
		QueryPerformanceCounter(&nTime);
		m_nTimeElapsed.QuadPart = nTime.QuadPart;
		return GetTimeInMilliSeconds(nTime.QuadPart - m_nTimeElapsed.QuadPart);
	}
	return 0.0f;
}

__int64 CTimeTick::GetPerformanceFrequency()
{
	LARGE_INTEGER nPerformanceFrequency;
	return QueryPerformanceFrequency(&nPerformanceFrequency) ? nPerformanceFrequency.QuadPart : 0;
}

float CTimeTick::GetTimeInMilliSeconds(__int64 nTime)
{
	return (float)nTime * 1000 / m_nPerformanceFrequency;
}