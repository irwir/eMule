#pragma once

class CPerfLog
{
public:
	CPerfLog();

	void Startup();
	void Shutdown();
	void LogSamples();

protected:
	DWORD	m_dwInterval;
	DWORD	m_dwLastSampled;
	CString	m_strFilePath;
	CString	m_strMRTGDataFilePath;
	CString	m_strMRTGOverheadFilePath;
	uint64	m_nLastSessionSentBytes;
	uint64	m_nLastSessionRecvBytes;
	uint64	m_nLastDnOH;
	uint64	m_nLastUpOH;
	// those values have to be specified in 'preferences.ini' -> hardcode them
	enum ELogMode : uint8
	{
		None = 0,
		OneSample = 1,
		AllSamples = 2
	} m_eMode;
	// those values have to be specified in 'preferences.ini' -> hardcode them
	enum ELogFileFormat : uint8
	{
		CSV = 0,
		MRTG = 1
	} m_eFileFormat;

	bool m_bInitialized;

	void WriteSamples(UINT nCurDn, UINT nCurUp, UINT nCurDnOH, UINT nCurUpOH);
};

extern CPerfLog thePerfLog;
