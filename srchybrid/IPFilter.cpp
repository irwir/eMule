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
#include <share.h>
#include <fcntl.h>
#include <io.h>
#include "emule.h"
#include "IPFilter.h"
#include "OtherFunctions.h"
#include "StringConversion.h"
#include "Preferences.h"
#include "emuledlg.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define	DFLT_FILTER_LEVEL	100 // default filter level if non specified

CIPFilter::CIPFilter()
	: m_pLastHit()
	, m_bModified()
{
	LoadFromDefaultFile(false);
}

CIPFilter::~CIPFilter()
{
	if (m_bModified) {
		try {
			SaveToDefaultFile();
		} catch (const CString&) {
		}
	}
	RemoveAllIPFilters();
}

static int __cdecl CmpSIPFilterByStartAddr(const void *p1, const void *p2) noexcept
{
	const SIPFilter *rng1 = *(SIPFilter**)p1;
	const SIPFilter *rng2 = *(SIPFilter**)p2;
	return CompareUnsigned(rng1->start, rng2->start);
}

CString CIPFilter::GetDefaultFilePath()
{
	return thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + DFLT_IPFILTER_FILENAME;
}

INT_PTR	CIPFilter::LoadFromDefaultFile(bool bShowResponse)
{
	RemoveAllIPFilters();
	return AddFromFile(GetDefaultFilePath(), bShowResponse);
}

INT_PTR CIPFilter::AddFromFile(LPCTSTR pszFilePath, bool bShowResponse)
{
	DWORD dwStart = ::GetTickCount();
	FILE *readFile = _tfsopen(pszFilePath, _T("r"), _SH_DENYWR);
	if (readFile != NULL) {
		int iFoundRanges = 0;
		int iLine = 0;
		try {
			enum EIPFilterFileType
			{
				Unknown = 0,
				FilterDat = 1,		// ipfilter.dat/ip.prefix format
				PeerGuardian = 2,	// PeerGuardian text format
				PeerGuardian2 = 3	// PeerGuardian binary format
			} eFileType = Unknown;
			setvbuf(readFile, NULL, _IOFBF, 32768);

			TCHAR szNam[_MAX_FNAME];
			TCHAR szExt[_MAX_EXT];
			_tsplitpath(pszFilePath, NULL, NULL, szNam, szExt);
			if (_tcsicmp(szExt, _T(".p2p")) == 0 || (_tcsicmp(szNam, _T("guarding.p2p")) == 0 && _tcsicmp(szExt, _T(".txt")) == 0))
				eFileType = PeerGuardian;
			else if (_tcsicmp(szExt, _T(".prefix")) == 0)
				eFileType = FilterDat;
			else {
				VERIFY(_setmode(_fileno(readFile), _O_BINARY) != -1);
				static const BYTE _aucP2Bheader[] = "\xFF\xFF\xFF\xFFP2B";
				BYTE aucHeader[sizeof _aucP2Bheader - 1];
				if (fread(aucHeader, sizeof aucHeader, 1, readFile) == 1) {
					if (memcmp(aucHeader, _aucP2Bheader, sizeof _aucP2Bheader - 1) == 0)
						eFileType = PeerGuardian2;
					else {
						(void)fseek(readFile, 0, SEEK_SET);
						VERIFY(_setmode(_fileno(readFile), _O_TEXT) != -1); // ugly!
					}
				}
			}

			if (eFileType == PeerGuardian2) {
				// Version 1: strings are ISO-8859-1 encoded
				// Version 2: strings are UTF-8 encoded
				uint8 nVersion;
				if (fread(&nVersion, sizeof nVersion, 1, readFile) == 1 && (nVersion == 1 || nVersion == 2)) {
					while (!feof(readFile)) {
						CHAR szName[256];
						unsigned iLen = 0;
						for (;;) { // read until NUL or EOF
							int iChar = getc(readFile);
							if (iChar == EOF)
								break;
							if (iLen < sizeof szName - 1)
								szName[iLen++] = (CHAR)iChar;
							if (iChar == '\0')
								break;
						}
						szName[iLen] = '\0';

						uint32 uStart;
						if (fread(&uStart, sizeof uStart, 1, readFile) != 1)
							break;
						uStart = ntohl(uStart);

						uint32 uEnd;
						if (fread(&uEnd, sizeof uEnd, 1, readFile) != 1)
							break;
						uEnd = ntohl(uEnd);

						++iLine;
						// (nVersion == 2) ? OptUtf8ToStr(szName, iLen) :
						AddIPRange(uStart, uEnd, DFLT_FILTER_LEVEL, CStringA(szName, iLen));
						++iFoundRanges;
					}
				}
			} else {
				CStringA sbuffer;
				CHAR szBuffer[1024];
				while (fgets(szBuffer, _countof(szBuffer), readFile) != NULL) {
					++iLine;
					sbuffer = szBuffer;
					sbuffer.Trim(" \t\r\n");

					// ignore comments & too short lines
					if (sbuffer[0] == '#' || sbuffer[0] == '/' || sbuffer.GetLength() < 15) {
						DEBUG_ONLY((!sbuffer.IsEmpty()) ? TRACE("IP filter: ignored line %u\n", iLine) : (void)0);
						continue;
					}

					if (eFileType == Unknown) {
						// looks like html
						if (sbuffer.Find('>') >= 0 && sbuffer.Find('<') >= 0)
							sbuffer.Delete(0, sbuffer.ReverseFind('>') + 1);

						// check for <IP> - <IP> at start of line
						UINT u1, u2, u3, u4, u5, u6, u7, u8;
						if (sscanf(sbuffer, "%3u.%3u.%3u.%3u - %3u.%3u.%3u.%3u", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8) == 8)
							eFileType = FilterDat;
						else {
							// check for <description> ':' <IP> '-' <IP>
							int iColon = sbuffer.Find(':');
							if (iColon > -1) {
								const CStringA &strIPRange(sbuffer.Mid(iColon + 1));
								if (sscanf(strIPRange, "%3u.%3u.%3u.%3u - %3u.%3u.%3u.%3u", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8) == 8)
									eFileType = PeerGuardian;
							}
						}
					}

					bool bValid = false;
					uint32 start = 0;
					uint32 end = 0;
					UINT level = 0;
					CStringA desc;
					if (eFileType == FilterDat)
						bValid = ParseFilterLine1(sbuffer, start, end, level, desc);
					else if (eFileType == PeerGuardian)
						bValid = ParseFilterLine2(sbuffer, start, end, level, desc);

					// add a filter
					if (bValid) {
						AddIPRange(start, end, level, desc);
						++iFoundRanges;
					} else
						DEBUG_ONLY((!sbuffer.IsEmpty()) ? TRACE("IP filter: ignored line %u\n", iLine) : (void)0);
				}
			}
		} catch (...) {
			AddDebugLogLine(false, _T("Exception when loading IP filters - %s"), _tcserror(errno));
			fclose(readFile);
			throw;
		}
		fclose(readFile);

		// sort the IP filter list by IP range start addresses
		qsort(m_iplist.GetData(), m_iplist.GetCount(), sizeof(m_iplist[0]), CmpSIPFilterByStartAddr);

		// merge overlapping and adjacent filter ranges
		int iDuplicate = 0;
		int iMerged = 0;
		if (m_iplist.GetCount() >= 2) {
			// On large IP-filter lists there is a noticeable performance problem when merging the list.
			// The 'CIPFilterArray::RemoveAt' call is way too expensive to get called during the merging,
			// thus we use temporary helper arrays to copy only the entries into the final list which
			// are not get deleted.

			// Reserve a byte array (its used as a boolean array actually) as large as the current
			// IP-filter list, so we can set a 'to delete' flag for each entry in the current IP-filter list.
			char *pcToDelete = new char[m_iplist.GetCount()]();
			int iNumToDelete = 0;

			SIPFilter *pPrv = m_iplist[0];
			for (int i = 1; i < m_iplist.GetCount(); ++i) {
				SIPFilter *pCur = m_iplist[i];
				if (pCur->start >= pPrv->start && pCur->start <= pPrv->end	 // overlapping
					|| pCur->start == pPrv->end + 1 && pCur->level == pPrv->level) // adjacent
				{
					if (pCur->start != pPrv->start || pCur->end != pPrv->end) { // don't merge identical entries
						//TODO: not yet handled, overlapping entries with different 'level'
						if (pCur->end > pPrv->end)
							pPrv->end = pCur->end;
						//pPrv->desc.AppendFormat("; %s", (LPCSTR)pCur->desc); // this may create a very very long description string...
						++iMerged;
					} else {
						// if we have identical entries, use the lowest 'level'
						if (pCur->level < pPrv->level)
							pPrv->level = pCur->level;
						++iDuplicate;
					}
					delete pCur;
					//m_iplist.RemoveAt(i);	// way too expensive (read above)
					pcToDelete[i] = 1;		// mark this entry as 'to delete'
					++iNumToDelete;
				} else
					pPrv = pCur;
			}

			// Create new IP-filter list which contains only the entries from
			// the original IP-filter list which are not to be deleted.
			if (iNumToDelete > 0) {
				CIPFilterArray newList;
				int iNewListIndex = (int)(m_iplist.GetCount() - iNumToDelete);
				newList.SetSize(iNewListIndex);
				for (INT_PTR i = m_iplist.GetCount(); --i >= 0;)
					if (!pcToDelete[i])
						newList[--iNewListIndex] = m_iplist[i];

				ASSERT(!iNewListIndex); //everything has been copied

				// Replace current list with new list. Dump, but still fast enough (only 1 memcpy)
				m_iplist.RemoveAll();
				m_iplist.Append(newList);
				newList.RemoveAll();
				m_bModified = true;
			}
			delete[] pcToDelete;
		}

		if (thePrefs.GetVerbose()) {
			DWORD dwEnd = ::GetTickCount();
			AddDebugLogLine(false, _T("Loaded IP filters from \"%s\""), pszFilePath);
			AddDebugLogLine(false, _T("Parsed lines/entries:%u  Found IP ranges:%u  Duplicate:%u  Merged:%u  Time:%s"), iLine, iFoundRanges, iDuplicate, iMerged, (LPCTSTR)CastSecondsToHM((dwEnd - dwStart + 500) / 1000));
		}
		AddLogLine(bShowResponse, GetResString(IDS_IPFILTERLOADED), m_iplist.GetCount());
	}
	return m_iplist.GetCount();
}

void CIPFilter::SaveToDefaultFile()
{
	const CString strFilePath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + DFLT_IPFILTER_FILENAME);
	FILE *fp = _tfsopen(strFilePath, _T("wt"), _SH_DENYWR);
	if (fp != NULL) {
		for (int i = 0; i < m_iplist.GetCount(); ++i) {
			const SIPFilter *flt = m_iplist[i];

			CHAR szStart[16];
			ipstrA(szStart, _countof(szStart), htonl(flt->start));

			CHAR szEnd[16];
			ipstrA(szEnd, _countof(szEnd), htonl(flt->end));

			if (fprintf(fp, "%-15s - %-15s , %03u , %s\n", szStart, szEnd, flt->level, (LPCSTR)flt->desc) == 0 || ferror(fp)) {
				CString strError;
				strError.Format(_T("Failed to save IP filter to file \"%s\" - %s"), (LPCTSTR)strFilePath, _tcserror(errno));
				fclose(fp);
				throw strError;
			}
		}
		fclose(fp);
		m_bModified = false;
	} else {
		CString strError;
		strError.Format(_T("Failed to save IP filter to file \"%s\" - %s"), (LPCTSTR)strFilePath, _tcserror(errno));
		throw strError;
	}
}

bool CIPFilter::ParseFilterLine1(const CStringA &sbuffer, uint32 &ip1, uint32 &ip2, UINT &level, CStringA &desc)
{
	UINT u1, u2, u3, u4, u5, u6, u7, u8, uLevel = DFLT_FILTER_LEVEL;
	int iDescStart = 0;
	int iItems = sscanf(sbuffer, "%3u.%3u.%3u.%3u - %3u.%3u.%3u.%3u , %3u , %n", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8, &uLevel, &iDescStart);
	if (iItems < 8)
		return false;

	((BYTE*)&ip1)[0] = (BYTE)u4;
	((BYTE*)&ip1)[1] = (BYTE)u3;
	((BYTE*)&ip1)[2] = (BYTE)u2;
	((BYTE*)&ip1)[3] = (BYTE)u1;

	((BYTE*)&ip2)[0] = (BYTE)u8;
	((BYTE*)&ip2)[1] = (BYTE)u7;
	((BYTE*)&ip2)[2] = (BYTE)u6;
	((BYTE*)&ip2)[3] = (BYTE)u5;

	if (iItems == 8) {
		level = DFLT_FILTER_LEVEL;	// set default level
		return true;
	}

	level = uLevel;

	if (iDescStart > 0) {
		LPCSTR pszDescStart = (LPCSTR)sbuffer + iDescStart;
		int iDescLen = sbuffer.GetLength() - iDescStart;
		if (iDescLen > 0 && pszDescStart[iDescLen - 1] == '\n')
			--iDescLen;

		memcpy(desc.GetBuffer(iDescLen), pszDescStart, iDescLen * (sizeof pszDescStart[0]));
		desc.ReleaseBuffer(iDescLen);
	}

	return true;
}

bool CIPFilter::ParseFilterLine2(const CStringA &sbuffer, uint32 &ip1, uint32 &ip2, UINT &level, CStringA &desc)
{
	int iPos = sbuffer.ReverseFind(':');
	if (iPos < 0)
		return false;

	desc = sbuffer.Left(iPos);
	desc.Replace("PGIPDB", "");
	desc.Trim();

	CStringA strIPRange = sbuffer.Mid(iPos + 1, sbuffer.GetLength() - iPos);
	unsigned u1, u2, u3, u4, u5, u6, u7, u8;
	if (sscanf(strIPRange, "%3u.%3u.%3u.%3u - %3u.%3u.%3u.%3u", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8) != 8)
		return false;

	((BYTE*)&ip1)[0] = (BYTE)u4;
	((BYTE*)&ip1)[1] = (BYTE)u3;
	((BYTE*)&ip1)[2] = (BYTE)u2;
	((BYTE*)&ip1)[3] = (BYTE)u1;

	((BYTE*)&ip2)[0] = (BYTE)u8;
	((BYTE*)&ip2)[1] = (BYTE)u7;
	((BYTE*)&ip2)[2] = (BYTE)u6;
	((BYTE*)&ip2)[3] = (BYTE)u5;

	level = DFLT_FILTER_LEVEL;

	return true;
}

void CIPFilter::RemoveAllIPFilters()
{
	for (INT_PTR i = m_iplist.GetCount(); --i >= 0;)
		delete m_iplist[i];
	m_iplist.RemoveAll();
	m_pLastHit = NULL;
}

bool CIPFilter::IsFiltered(uint32 ip) /*const*/
{
	return IsFiltered(ip, thePrefs.GetIPFilterLevel());
}

static int __cdecl CmpSIPFilterByAddr(const void *pvKey, const void *pvElement) noexcept
{
	uint32 ip = *(uint32*)pvKey;
	const SIPFilter *pIPFilter = *(SIPFilter**)pvElement;

	if (ip < pIPFilter->start)
		return -1;
	if (ip > pIPFilter->end)
		return 1;
	return 0;
}

bool CIPFilter::IsFiltered(uint32 ip, UINT level) /*const*/
{
	if (m_iplist.IsEmpty() || ip == 0)
		return false;

	ip = ntohl(ip);

	// to speed things up we use a binary search
	//	*)	the IP filter list must be sorted by IP range start addresses
	//	*)	the IP filter list is not allowed to contain overlapping IP ranges (see also the IP range merging code when
	//		loading the list)
	//	*)	the filter 'level' is ignored during the binary search and is evaluated only for the found element
	//
	// TODO: this can still be improved even more:
	//	*)	use a pre assembled list of IP ranges which contains only the IP ranges for the currently used filter level
	//	*)	use a dumb plain array for storing the IP range structures. this will give more cache hits when processing
	//		the list. but(!) this would require to also use a dumb SIPFilter structure (don't use data items with ctors).
	//		otherwise the creation of the array would be rather slow.
	SIPFilter **ppFound = (SIPFilter**)bsearch(&ip, m_iplist.GetData(), m_iplist.GetCount(), sizeof m_iplist[0], CmpSIPFilterByAddr);
	if (ppFound && (*ppFound)->level < level) {
		(*ppFound)->hits++;
		m_pLastHit = *ppFound;
		return true;
	}

	return false;
}

CString CIPFilter::GetLastHit() const
{
	return CString(m_pLastHit ? m_pLastHit->desc : "Not available");
}

const CIPFilterArray& CIPFilter::GetIPFilter() const
{
	return m_iplist;
}

bool CIPFilter::RemoveIPFilter(const SIPFilter *pFilter)
{
	for (INT_PTR i = m_iplist.GetCount(); --i >= 0;)
		if (m_iplist[i] == pFilter) {
			delete m_iplist[i];
			m_iplist.RemoveAt(i);
			return true;
		}

	return false;
}