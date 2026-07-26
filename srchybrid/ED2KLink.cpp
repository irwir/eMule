//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
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
#include <wininet.h>
#include "resource.h"
#include "opcodes.h"
#include "ED2KLink.h"
#include "SafeFile.h"
#include "StringConversion.h"
#include "preferences.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////
// CED2KServerListLink implementation
/////////////////////////////////////////////
CED2KServerListLink::CED2KServerListLink(LPCTSTR address)
	: m_address(address)
{
}

void CED2KServerListLink::GetLink(CString &lnk) const
{
	lnk.Format(_T("ed2k://|serverlist|%s|/"), (LPCTSTR)m_address);
}

/////////////////////////////////////////////
// CED2KNodesListLink implementation
/////////////////////////////////////////////
CED2KNodesListLink::CED2KNodesListLink(LPCTSTR address)
	: m_address(address)
{
}

void CED2KNodesListLink::GetLink(CString &lnk) const
{
	lnk.Format(_T("ed2k://|nodeslist|%s|/"), (LPCTSTR)m_address);
}

/////////////////////////////////////////////
// CED2KServerLink implementation
/////////////////////////////////////////////
CED2KServerLink::CED2KServerLink(LPCTSTR ip, LPCTSTR port)
	: m_strAddress(ip)
{
	unsigned long uPort = _tcstoul(port, 0, 10);
	if (uPort > _UI16_MAX)
		throw GetResString(IDS_ERR_BADPORT);
	m_port = static_cast<uint16>(uPort);
	m_defaultName.Format(_T("Server %s:%s"), ip, port);
}

void CED2KServerLink::GetLink(CString &lnk) const
{
	lnk.Format(_T("ed2k://|server|%s|%u|/"), (LPCTSTR)GetAddress(), (unsigned)GetPort());
}


/////////////////////////////////////////////
// CED2KSearchLink implementation
/////////////////////////////////////////////
CED2KSearchLink::CED2KSearchLink(LPCTSTR pszSearchTerm)
	: m_strSearchTerm(OptUtf8ToStr(URLDecode(pszSearchTerm)))
{
}

void CED2KSearchLink::GetLink(CString &lnk) const
{
	lnk.Format(_T("ed2k://|search|%s|/"), (LPCTSTR)EncodeUrlUtf8(m_strSearchTerm));
}


/////////////////////////////////////////////
// CED2KFileLink implementation
/////////////////////////////////////////////
CED2KFileLink::CED2KFileLink(LPCTSTR pszName, LPCTSTR pszSize, LPCTSTR pszHash
		, const CStringArray &astrParams, LPCTSTR pszSources)
	: SourcesList()
	, m_hashset()
	, m_name(OptUtf8ToStr(URLDecode(pszName)).Trim())
	, m_size(pszSize)
	, m_bAICHHashValid()
{
	// Here we have a little problem. Actually the proper solution would be to decode from UTF-8,
	// only if the string does contain escape sequences. But if user pastes a raw UTF-8 encoded
	// string (for whatever reason), we would miss to decode that string. On the other side,
	// always decoding UTF-8 can give flaws in case the string is valid for Unicode and UTF-8
	// at the same time. However, to avoid the pasting of raw UTF-8 strings (which would lead
	// to a greater mess in the network) we always try to decode from UTF-8, even if the string
	// did not contain escape sequences.
	UINT uid = 0;
	if (m_name.IsEmpty())
		uid = IDS_ERR_NOTAFILELINK;
	else if (_tcslen(pszHash) != 32)
		uid = IDS_ERR_ILLFORMEDHASH;
	else {
		const sint64 iSize = _tstoi64(pszSize);
		if (iSize <= 0)
			uid = IDS_ERR_NOTAFILELINK;
		else if ((uint64)iSize > MAX_EMULE_FILE_SIZE)
			uid = IDS_ERR_TOOLARGEFILE;
		else if ((uint64)iSize > OLD_MAX_EMULE_FILE_SIZE && !thePrefs.CanFSHandleLargeFiles(0))
			uid = IDS_ERR_FSCANTHANDLEFILE;
		else if (!strmd4(pszHash, m_hash))
			uid = IDS_ERR_ILLFORMEDHASH;
	}
	if (uid)
		throw GetResString(uid);

	bool bError = false;
	for (INT_PTR i = 0; !bError && i < astrParams.GetCount(); ++i) {
		const CString &strParam(astrParams[i]);
		ASSERT(!strParam.IsEmpty());

		CString strTok;
		int iPos = strParam.Find(_T('='));
		if (iPos >= 0)
			strTok = strParam.Left(iPos);
		if (strTok.IsEmpty())
			continue;
		switch (strTok[0]) {
		case _T('s'):
			{
				const CString &strURL(strParam.Mid(iPos + 1));
				if (!strURL.IsEmpty()) {
					TCHAR szScheme[INTERNET_MAX_SCHEME_LENGTH];
					TCHAR szHostName[INTERNET_MAX_HOST_NAME_LENGTH];
					TCHAR szUrlPath[INTERNET_MAX_PATH_LENGTH];
					TCHAR szUserName[INTERNET_MAX_USER_NAME_LENGTH];
					TCHAR szPassword[INTERNET_MAX_PASSWORD_LENGTH];
					TCHAR szExtraInfo[INTERNET_MAX_URL_LENGTH];
					URL_COMPONENTS Url = {};
					Url.dwStructSize = (DWORD)sizeof Url;
					Url.lpszScheme = szScheme;
					Url.dwSchemeLength = _countof(szScheme);
					Url.lpszHostName = szHostName;
					Url.dwHostNameLength = _countof(szHostName);
					Url.lpszUserName = szUserName;
					Url.dwUserNameLength = _countof(szUserName);
					Url.lpszPassword = szPassword;
					Url.dwPasswordLength = _countof(szPassword);
					Url.lpszUrlPath = szUrlPath;
					Url.dwUrlPathLength = _countof(szUrlPath);
					Url.lpszExtraInfo = szExtraInfo;
					Url.dwExtraInfoLength = _countof(szExtraInfo);
					if (::InternetCrackUrl(strURL, 0, 0, &Url) && Url.dwHostNameLength > 0 && Url.dwHostNameLength < INTERNET_MAX_HOST_NAME_LENGTH) {
						SUnresolvedHostname *hostname = new SUnresolvedHostname;
						hostname->strURL = strURL;
						hostname->strHostname = szHostName;
						m_HostnameSourcesList.AddTail(hostname);
					}
				} else
					ASSERT(0);
			}
			break;
		case _T('p'):
			{
				const CString &strPartHashes(strParam.Tokenize(_T("="), iPos));

				if (m_hashset != NULL) {
					ASSERT(0);
					bError = true;
					break;
				}

				m_hashset = new CSafeMemFile(256);
				m_hashset->WriteHash16(m_hash);
				m_hashset->WriteUInt16(0);

				uint16 iHashCount = 0;
				for (int jPos = 0;;) {
					const CString &strHash(strPartHashes.Tokenize(_T(":"), jPos));
					if (strHash.IsEmpty())
						break;
					uchar aucPartHash[MDX_DIGEST_SIZE];
					if (!strmd4(strHash, aucPartHash)) {
						bError = true;
						break;
					}
					m_hashset->WriteHash16(aucPartHash);
					++iHashCount;
				}
				if (!bError) {
					m_hashset->Seek(16, CFile::begin);
					m_hashset->WriteUInt16(iHashCount);
					m_hashset->Seek(0, CFile::begin);
				}
			}
			break;
		case _T('h'):
			if (strParam[iPos + 1]) { //not empty
				if (DecodeBase32(CPTR(strParam, iPos + 1), m_AICHHash.GetRawHash(), CAICHHash::GetHashSize()) == CAICHHash::GetHashSize()) {
					m_bAICHHashValid = true;
					ASSERT(m_AICHHash.GetString().CompareNoCase(CPTR(strParam, iPos + 1)) == 0);
					break;
				}
			}
		default:
			ASSERT(0);
		}
	}

	if (bError) {
		delete m_hashset;
		m_hashset = NULL;
	}

	if (!pszSources || !*pszSources)
		return;
	LPCTSTR pCh = pszSources;
	pCh = _tcsstr(pCh, _T("sources"));
	if (pCh == NULL)
		return;
	pCh += 7; // point to char after "sources"
	LPCTSTR pEnd = pCh; // make a pointer to the terminating NUL
	while (*pEnd)
		++pEnd;

	// if there's an expiration date...
	if (*pCh == _T('@')) {
		if (pEnd - pCh <= 7)
			return;
		TCHAR date[3];
		date[2] = 0; // terminate the string

		struct tm tmexp = {};
		date[0] = *++pCh;
		date[1] = *++pCh;
		tmexp.tm_year = (int)_tcstol(date, NULL, 10) + (2000 - 1900); //since 1900
		date[0] = *++pCh;
		date[1] = *++pCh;
		tmexp.tm_mon = (int)_tcstol(date, NULL, 10) - 1;
		date[0] = *++pCh;
		date[1] = *++pCh;
		tmexp.tm_mday = (int)_tcstol(date, NULL, 10);
		time_t tExpire = mktime(&tmexp);
		//no time zone information, assume UTC
		if (tExpire == (time_t)-1 || tExpire <= time(NULL))
			return;
		++pCh;
	}

	if (++pCh >= pEnd) //make pCh to point to the first "ip:port" and check for sources
		return;
	//uint32 dwServerIP = 0; - unknown here
	//uint16 nServerPort = 0;
	int nInvalid = 0;
	SourcesList = new CSafeMemFile(256);
	uint16 nCount = 0;
	SourcesList->WriteUInt16(nCount); // init to 0, we'll fix this at the end.
	// for each "ip:port" source string until the end
	// limit to prevent overflow (uint16 due to CPartFile::AddClientSources)
	while (*pCh != 0 && nCount < MAXSHORT) {
		LPCTSTR pIP = pCh;
		LPCTSTR pNext;
		// find the end of this ip:port string & start of next ip:port string.
		if ((pCh = _tcschr(pCh, _T(','))) != NULL)
			pNext = pCh++; // ends the current "ip:port" and point to next "ip:port"
		else
			pNext = pCh = pEnd;

		LPCTSTR pPort = _tcschr(pIP, _T(':'));
		// if port is not present for this ip, skip to the next ip
		if (pPort == NULL || pPort >= pNext) {
			++nInvalid;
			continue;
		}
		CStringA sIPa(pIP, static_cast<int>(pPort - pIP));
		++pPort;	// move to port string
		unsigned long uPort = _tcstoul(pPort, NULL, 10);
		// skip bad ips and ports
		if (!uPort || uPort > _UI16_MAX) {
			++nInvalid;
			continue;
		}
		unsigned long dwID = inet_addr(sIPa);
		if (dwID == INADDR_NONE) {	// host name?
			if (_tcslen(pIP) > 512) {
				++nInvalid;
				continue;
			}
			SUnresolvedHostname *hostname = new SUnresolvedHostname;
			hostname->strHostname = sIPa;
			hostname->nPort = static_cast<uint16>(uPort);
			m_HostnameSourcesList.AddTail(hostname);
			continue;
		}
		//TODO: This will filter out *.*.*.0 clients. Is there a nice way to fix?
		if (::IsLowID(dwID)) { // ip
			++nInvalid;
			continue;
		}

		SourcesList->WriteUInt32(dwID);
		SourcesList->WriteUInt16(static_cast<uint16>(uPort));
		SourcesList->WriteUInt32(0); // dwServerIP
		SourcesList->WriteUInt16(0); // nServerPort
		++nCount;
	}

	if (nCount) {
		SourcesList->SeekToBegin();
		SourcesList->WriteUInt16(nCount);
		SourcesList->SeekToBegin();
	} else {
		delete SourcesList;
		SourcesList = NULL;
	}
}

CED2KFileLink::~CED2KFileLink()
{
	delete SourcesList;
	while (!m_HostnameSourcesList.IsEmpty())
		delete m_HostnameSourcesList.RemoveHead();
	delete m_hashset;
}

void CED2KFileLink::GetLink(CString &lnk) const
{
	lnk.Format(_T("ed2k://|file|%s|%s|%s|/")
		, (LPCTSTR)EncodeUrlUtf8(m_name)
		, (LPCTSTR)m_size
		, (LPCTSTR)EncodeBase16(m_hash, 16));
}

EMFileSize CED2KFileLink::GetSize() const
{
	return (EMFileSize)(uint64)_tstoi64(m_size);
}

CED2KLink* CED2KLink::CreateLinkFromUrl(LPCTSTR uri)
{
	CString strURI(uri);
	strURI.Trim(); // This function is used for various sources, trim the string again.
	int iPos = 0;
	CString strTok(GetNextString(strURI, _T('|'), iPos));
	if (strTok.CompareNoCase(_T("ed2k://")) == 0) {
		strTok = GetNextString(strURI, _T('|'), iPos);
		if (strTok == _T("file")) {
			const CString &strName(GetNextString(strURI, _T('|'), iPos));
			if (!strName.IsEmpty()) {
				const CString &strSize(GetNextString(strURI, _T('|'), iPos));
				if (!strSize.IsEmpty()) {
					const CString &strHash(GetNextString(strURI, _T('|'), iPos));
					if (!strHash.IsEmpty()) {
						CStringArray astrEd2kParams;
						bool bEmuleExt = false;
						CString strEmuleExt;

						CString strLastTok;
						while (!(strTok = GetNextString(strURI, _T('|'), iPos)).IsEmpty()) {
							strLastTok = strTok;
							if (strTok == _T("/")) {
								if (bEmuleExt)
									break;
								bEmuleExt = true;
							} else {
								if (bEmuleExt) {
									if (!strEmuleExt.IsEmpty())
										strEmuleExt += _T('|');
									strEmuleExt += strTok;
								} else
									astrEd2kParams.Add(strTok);
							}
						}

						if (strLastTok == _T("/"))
							return new CED2KFileLink(strName, strSize, strHash, astrEd2kParams, strEmuleExt);
					}
				}
			}
		} else if (strTok == _T("serverlist")) {
			const CString &strURL(GetNextString(strURI, _T('|'), iPos));
			if (!strURL.IsEmpty() && GetNextString(strURI, _T('|'), iPos) == _T("/"))
				return new CED2KServerListLink(strURL);
		} else if (strTok == _T("server")) {
			const CString &strServer(GetNextString(strURI, _T('|'), iPos));
			if (!strServer.IsEmpty()) {
				const CString &strPort(GetNextString(strURI, _T('|'), iPos));
				if (!strPort.IsEmpty() && GetNextString(strURI, _T('|'), iPos) == _T("/"))
					return new CED2KServerLink(strServer, strPort);
			}
		} else if (strTok == _T("nodeslist")) {
			const CString &strURL(GetNextString(strURI, _T('|'), iPos));
			if (!strURL.IsEmpty() && GetNextString(strURI, _T('|'), iPos) == _T("/"))
				return new CED2KNodesListLink(strURL);
		} else if (strTok == _T("search")) {
			const CString &strSearchTerm(GetNextString(strURI, _T('|'), iPos));
			// might be extended with more parameters in future versions
			if (!strSearchTerm.IsEmpty())
				return new CED2KSearchLink(strSearchTerm);
		}
	} else {
		iPos = 0;
		if (GetNextString(strURI, _T('?'), iPos).Compare(_T("magnet:")) == 0) {
			CString strName, strSize, strHash, strEmuleExt;
			CStringArray astrEd2kParams;
			for (;;) {
				strTok = GetNextString(strURI, _T('&'), iPos);
				if (iPos < 0)
					return new CED2KFileLink(strName, strSize, strHash, astrEd2kParams, strEmuleExt);
				if (strTok.GetLength() < 3 || strTok[2] != _T('='))
					continue;
				const CString &strT(strTok.Left(2));
				strTok.Delete(0, 3);
				if (strT == _T("as")) { //acceptable source
					if (strTok.Left(7).CompareNoCase(_T("http://")) == 0)
						astrEd2kParams.Add(_T("s=") + strTok); //http source
				} else if (strT == _T("dn")) { //display name
					strName = strTok; //file name
				} else if (strT == _T("xl")) { //eXact length
					strSize = strTok; //file size
				} else if (strT == _T("xs") && strTok.Left(10) == _T("ed2kftp://")) {//eXact source
					strTok.Delete(0, 10);
					int i = strTok.Find(_T('/'));
					if (i > 0)
						astrEd2kParams.Add(_T("sources,") + strTok.Left(i)); //source IP:port
				} else if (strT == _T("xt")) {//eXact topic
					if (strTok.Left(9) == _T("urn:ed2k:"))
						strHash = strTok.Mid(9); //file ID
					else if (strTok.Left(13) == _T("urn:ed2khash:"))
						strHash = strTok.Mid(13); //file ID
					else if (strTok.Left(9) == _T("urn:aich:"))
						astrEd2kParams.Add(_T("h=") + strTok.Mid(9)); //AICH root hash
				}
			}
		}
	}

	throw GetResString(IDS_ERR_NOSLLINK);
}