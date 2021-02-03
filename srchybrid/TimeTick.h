/////////////////////////////////////////////////////////////////////////////
//
// Copyright © 2001, Stefan Belopotocan, http://welcome.to/BeloSoft
//
/////////////////////////////////////////////////////////////////////////////
#pragma once

/////////////////////////////////////////////////////////////////////////////
// CTimeTick

class CTimeTick
{
	CTimeTick(const CTimeTick &d);
	CTimeTick& operator=(const CTimeTick &d) = delete;

public:
	CTimeTick();
	~CTimeTick() = default;

	// Operations
	void Start();
	float Tick();
	static bool  isPerformanceCounter()		{ return m_nPerformanceFrequency != 0; }

	// Implementation
protected:
	static __int64 GetPerformanceFrequency();
	static float GetTimeInMilliSeconds(__int64 nTime);

	// Data
private:
	static __int64 m_nPerformanceFrequency;

	LARGE_INTEGER m_nTimeElapsed;
	LARGE_INTEGER m_nTime;
};