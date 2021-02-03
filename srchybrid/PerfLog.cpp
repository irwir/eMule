//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"
#include <io.h>
#include <share.h>
#include "emule.h"
#include "PerfLog.h"
#include "ini2.h"
#include "Opcodes.h"
#include "Preferences.h"
#include "Statistics.h"
#include "emuledlg.h"
#include "Log.h"
#include "otherfunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CPerfLog thePerfLog;

CPerfLog::CPerfLog()
	: m_dwInterval(MIN2MS(5))
	, m_dwLastSampled()
	, m_nLastSessionSentBytes()
	, m_nLastSessionRecvBytes()
	, m_nLastDnOH()
	, m_nLastUpOH()
	, m_eMode(None)
	, m_eFileFormat(CSV)
	, m_bInitialized()
{
}

void CPerfLog::Startup()
{
	if (m_bInitialized)
		return;

	CIni ini(thePrefs.GetConfigFile(), _T("PerfLog"));

	m_eMode = (ELogMode)ini.GetInt(_T("Mode"), None);
	if (m_eMode != None && m_eMode != OneSample && m_eMode != AllSamples)
		m_eMode = None;
	if (m_eMode != None) {
		m_eFileFormat = (ELogFileFormat)ini.GetInt(_T("FileFormat"), CSV);

		// set default log file path
		CString strDefFilePath = thePrefs.GetMuleDirectory(EMULE_CONFIGBASEDIR);
		if (m_eFileFormat == CSV)
			strDefFilePath += _T("perflog.csv");
		else
			strDefFilePath += _T("perflog.mrtg");

		m_strFilePath = ini.GetString(_T("File"), strDefFilePath);
		if (m_strFilePath.IsEmpty())
			m_strFilePath = strDefFilePath;

		if (m_eFileFormat == MRTG) {
			TCHAR drv[_MAX_DRIVE];
			TCHAR dir[_MAX_DIR];
			TCHAR nam[_MAX_FNAME];
			_tsplitpath(m_strFilePath, drv, dir, nam, NULL);
			m_strFilePath.Empty();

			_tmakepathlimit(m_strMRTGDataFilePath.GetBuffer(MAX_PATH), drv, dir, CString(nam) + _T("_data"), _T("mrtg"));
			m_strMRTGDataFilePath.ReleaseBuffer();

			_tmakepathlimit(m_strMRTGOverheadFilePath.GetBuffer(MAX_PATH), drv, dir, CString(nam) + _T("_overhead"), _T("mrtg"));
			m_strMRTGOverheadFilePath.ReleaseBuffer();
		}

		m_dwInterval = MIN2MS(ini.GetInt(_T("Interval"), 5));
		if ((int)m_dwInterval <= 0)
			m_dwInterval = MIN2MS(5);
	}

	m_bInitialized = true;

	if (m_eMode == OneSample)
		LogSamples();
}

void CPerfLog::WriteSamples(UINT nCurDn, UINT nCurUp, UINT nCurDnOH, UINT nCurUpOH)
{
	ASSERT(m_bInitialized);

	if (m_eFileFormat == CSV) {
		time_t tNow = time(NULL);
		char szTime[40];
		// do not localize this date/time string!
		strftime(szTime, _countof(szTime), "%m/%d/%Y %H:%M:%S", localtime(&tNow));

		FILE *fp = _tfsopen(m_strFilePath, (m_eMode == OneSample) ? _T("wt") : _T("at"), _SH_DENYWR);
		if (fp == NULL) {
			LogError(LOG_DEFAULT, _T("Failed to open performance log file \"%s\" - %s"), (LPCTSTR)m_strFilePath, _tcserror(errno));
			return;
		}
		setvbuf(fp, NULL, _IOFBF, 16384); // ensure that all lines are written to file with one call
		if (m_eMode == OneSample || _filelength(_fileno(fp)) == 0)
			fprintf(fp, "\"(PDH-CSV 4.0)\",\"DatDown\",\"DatUp\",\"OvrDown\",\"OvrUp\"\n");
		fprintf(fp, "\"%s\",\"%u\",\"%u\",\"%u\",\"%u\"\n", szTime, nCurDn, nCurUp, nCurDnOH, nCurUpOH);
		fclose(fp);
	} else {
		ASSERT(m_eFileFormat == MRTG);

		FILE *fp = _tfsopen(m_strMRTGDataFilePath, (m_eMode == OneSample) ? _T("wt") : _T("at"), _SH_DENYWR);
		if (fp != NULL) {
			fprintf(fp, "%u\n%u\n\n\n", nCurDn, nCurUp);
			fclose(fp);
		} else
			LogError(LOG_DEFAULT, _T("Failed to open performance log file \"%s\" - %s"), (LPCTSTR)m_strMRTGDataFilePath, _tcserror(errno));

		fp = _tfsopen(m_strMRTGOverheadFilePath, (m_eMode == OneSample) ? _T("wt") : _T("at"), _SH_DENYWR);
		if (fp != NULL) {
			fprintf(fp, "%u\n%u\n\n\n", nCurDnOH, nCurUpOH);
			fclose(fp);
		} else
			LogError(LOG_DEFAULT, _T("Failed to open performance log file \"%s\" - %s"), (LPCTSTR)m_strMRTGOverheadFilePath, _tcserror(errno));
	}
}

void CPerfLog::LogSamples()
{
	if (m_eMode == None)
		return;

	DWORD dwNow = ::GetTickCount();
	if (dwNow < m_dwLastSampled + m_dwInterval)
		return;

	// 'data counters' amount of transferred file data
	UINT nCurDn = (UINT)(theStats.sessionReceivedBytes - m_nLastSessionRecvBytes);
	UINT nCurUp = (UINT)(theStats.sessionSentBytes - m_nLastSessionSentBytes);

	// 'overhead counters' amount of total overhead
	uint64 nDnOHTotal = theStats.GetDownDataOverheadFileRequest()
					+ theStats.GetDownDataOverheadSourceExchange()
					+ theStats.GetDownDataOverheadServer()
					+ theStats.GetDownDataOverheadKad()
					+ theStats.GetDownDataOverheadOther();
	uint64 nUpOHTotal = theStats.GetUpDataOverheadFileRequest()
					+ theStats.GetUpDataOverheadSourceExchange()
					+ theStats.GetUpDataOverheadServer()
					+ theStats.GetUpDataOverheadKad()
					+ theStats.GetUpDataOverheadOther();
	UINT nCurDnOH = (UINT)(nDnOHTotal - m_nLastDnOH);
	UINT nCurUpOH = (UINT)(nUpOHTotal - m_nLastUpOH);

	WriteSamples(nCurDn, nCurUp, nCurDnOH, nCurUpOH);

	m_nLastSessionRecvBytes = theStats.sessionReceivedBytes;
	m_nLastSessionSentBytes = theStats.sessionSentBytes;
	m_nLastDnOH = nDnOHTotal;
	m_nLastUpOH = nUpOHTotal;
	m_dwLastSampled = dwNow;
}

void CPerfLog::Shutdown()
{
	if (m_eMode == OneSample)
		WriteSamples(0, 0, 0, 0);
}