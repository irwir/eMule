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
#include "StdAfx.h"
#include "corruptionblackbox.h"
#include "knownfile.h"
#include "updownclient.h"
#include "log.h"
#include "emule.h"
#include "clientlist.h"
#include "opcodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define	 CBB_BANTHRESHOLD	32 //% max corrupted data

CCBBRecord::CCBBRecord(uint64 nStartPos, uint64 nEndPos, uint32 dwIP, EBBRStatus BBRStatus)
{
	if (nStartPos > nEndPos) {
		ASSERT(0);
		return;
	}
	m_nStartPos = nStartPos;
	m_nEndPos = nEndPos;
	m_dwIP = dwIP;
	m_BBRStatus = BBRStatus;
}

CCBBRecord& CCBBRecord::operator=(const CCBBRecord &cv)
{
	m_nStartPos = cv.m_nStartPos;
	m_nEndPos = cv.m_nEndPos;
	m_dwIP = cv.m_dwIP;
	m_BBRStatus = cv.m_BBRStatus;
	return *this;
}

bool CCBBRecord::Merge(uint64 nStartPos, uint64 nEndPos, uint32 dwIP, EBBRStatus BBRStatus)
{
	if (m_dwIP == dwIP && m_BBRStatus == BBRStatus && (nStartPos == m_nEndPos + 1 || nEndPos + 1 == m_nStartPos)) {
		if (nStartPos == m_nEndPos + 1)
			m_nEndPos = nEndPos;
		else if (nEndPos + 1 == m_nStartPos)
			m_nStartPos = nStartPos;
		else
			ASSERT(0);

		return true;
	}
	return false;
}

bool CCBBRecord::CanMerge(uint64 nStartPos, uint64 nEndPos, uint32 dwIP, EBBRStatus BBRStatus) const
{
	return m_dwIP == dwIP && m_BBRStatus == BBRStatus && (nStartPos == m_nEndPos + 1 || nEndPos + 1 == m_nStartPos);
}

void CCorruptionBlackBox::Init(EMFileSize nFileSize)
{
	m_aaRecords.SetSize((INT_PTR)(((uint64)nFileSize + (PARTSIZE - 1)) / PARTSIZE));
}

void CCorruptionBlackBox::Free()
{
	m_aaRecords.RemoveAll();
	m_aaRecords.FreeExtra();
}

void CCorruptionBlackBox::TransferredData(uint64 nStartPos, uint64 nEndPos, const CUpDownClient *pSender)
{
	if (nEndPos - nStartPos >= PARTSIZE) {
		ASSERT(0);
		return;
	}
	if (nStartPos > nEndPos) {
		ASSERT(0);
		return;
	}
	if (!pSender) //importing parts
		return;
	uint32 dwSenderIP = pSender->GetIP();
	// we store records separated for each part, so we don't have to search all entries every time

	// convert pos to relative block pos
	UINT nPart = (UINT)(nStartPos / PARTSIZE);
	uint64 nRelStartPos = nStartPos - nPart * PARTSIZE;
	uint64 nRelEndPos = nEndPos - nPart * PARTSIZE;
	if (nRelEndPos >= PARTSIZE) {
		// data crosses the part boundary, split it
		nRelEndPos = PARTSIZE - 1;
		uint64 nTmpStartPos = nPart * PARTSIZE + nRelEndPos + 1;
		ASSERT(nTmpStartPos % PARTSIZE == 0); // remove later
		TransferredData(nTmpStartPos, nEndPos, pSender);
	}
	if ((INT_PTR)nPart >= m_aaRecords.GetCount()) {
		//ASSERT( false );
		m_aaRecords.SetSize(nPart + 1);
	}
	INT_PTR posMerge = -1;
	uint64 ndbgRewritten = 0;
	for (INT_PTR i = 0; i < m_aaRecords[nPart].GetCount(); ++i) {
		CCBBRecord &cbb = m_aaRecords[nPart][i];
		if (cbb.CanMerge(nRelStartPos, nRelEndPos, dwSenderIP, BBR_NONE))
			posMerge = i;
		// check if there is already a pending entry and overwrite it
		else if (cbb.m_BBRStatus == BBR_NONE) {
			if (cbb.m_nStartPos >= nRelStartPos && cbb.m_nEndPos <= nRelEndPos) {
				// old one is included in new one -> delete
				ndbgRewritten += (cbb.m_nEndPos - cbb.m_nStartPos) + 1;
				m_aaRecords[nPart].RemoveAt(i);
				--i;
			} else if (cbb.m_nStartPos < nRelStartPos && cbb.m_nEndPos > nRelEndPos) {
				// old one includes new one
				// check if the old one and new one have the same ip
				if (dwSenderIP != cbb.m_dwIP) {
					// different IP, means we have to split it 2 times
					uint64 nTmpStartPos1 = nRelEndPos + 1;
					uint64 nTmpEndPos1 = cbb.m_nEndPos;
					uint64 nTmpStartPos2 = cbb.m_nStartPos;
					uint64 nTmpEndPos2 = nRelStartPos - 1;
					cbb.m_nStartPos = nRelStartPos;
					cbb.m_nEndPos = nRelEndPos;
					uint32 dwOldIP = cbb.m_dwIP;
					cbb.m_dwIP = dwSenderIP;
					m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos1, nTmpEndPos1, dwOldIP));
					m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos2, nTmpEndPos2, dwOldIP));
					// and are done then
				}
				DEBUG_ONLY(AddDebugLogLine(DLP_DEFAULT, false, _T("CorruptionBlackBox: Debug: %i bytes were rewritten and records replaced with new stats (1)"), (nRelEndPos - nRelStartPos) + 1));
				return;
			} else if (cbb.m_nStartPos >= nRelStartPos && cbb.m_nStartPos <= nRelEndPos) {
				// old one overlaps new one on the right side
				ASSERT(nRelEndPos > cbb.m_nStartPos);
				ndbgRewritten += nRelEndPos - cbb.m_nStartPos;
				cbb.m_nStartPos = nRelEndPos + 1;
			} else if (cbb.m_nEndPos >= nRelStartPos && cbb.m_nEndPos <= nRelEndPos) {
				// old one overlaps new one on the left side
				ASSERT(cbb.m_nEndPos > nRelStartPos);
				ndbgRewritten += cbb.m_nEndPos - nRelStartPos;
				cbb.m_nEndPos = nRelStartPos - 1;
			}
		}
	}
	if (posMerge >= 0)
		VERIFY(m_aaRecords[nPart][posMerge].Merge(nRelStartPos, nRelEndPos, dwSenderIP, BBR_NONE));
	else
		m_aaRecords[nPart].Add(CCBBRecord(nRelStartPos, nRelEndPos, dwSenderIP, BBR_NONE));

	if (ndbgRewritten > 0)
		DEBUG_ONLY(AddDebugLogLine(DLP_DEFAULT, false, _T("CorruptionBlackBox: Debug: %i bytes were rewritten and records replaced with new stats (2)"), ndbgRewritten));
}

void CCorruptionBlackBox::VerifiedData(uint64 nStartPos, uint64 nEndPos)
{
	if (nEndPos - nStartPos >= PARTSIZE) {
		ASSERT(0);
		return;
	}
	// convert pos to relative block pos
	UINT nPart = (UINT)(nStartPos / PARTSIZE);
	uint64 nRelStartPos = nStartPos - nPart * PARTSIZE;
	uint64 nRelEndPos = nEndPos - nPart * PARTSIZE;
	if (nRelEndPos >= PARTSIZE) {
		ASSERT(0);
		return;
	}
	if ((INT_PTR)nPart >= m_aaRecords.GetCount()) {
		//ASSERT( false );
		m_aaRecords.SetSize(nPart + 1);
	}
#ifdef _DEBUG
	uint64 nDbgVerifiedBytes = 0;
	//uint32 nDbgOldEntries = m_aaRecords[nPart].GetCount();
	CMap<int, int, int, int> mapDebug;
#endif
	for (INT_PTR i = 0; i < m_aaRecords[nPart].GetCount(); ++i) {
		CCBBRecord &cbb = m_aaRecords[nPart][i];
		if (cbb.m_BBRStatus == BBR_NONE || cbb.m_BBRStatus == BBR_VERIFIED) {
			if (cbb.m_nStartPos >= nRelStartPos && cbb.m_nEndPos <= nRelEndPos)
				;
			else if (cbb.m_nStartPos < nRelStartPos && cbb.m_nEndPos > nRelEndPos) {
				// need to split it 2*
				uint64 nTmpStartPos1 = nRelEndPos + 1;
				uint64 nTmpEndPos1 = cbb.m_nEndPos;
				uint64 nTmpStartPos2 = cbb.m_nStartPos;
				uint64 nTmpEndPos2 = nRelStartPos - 1;
				cbb.m_nStartPos = nRelStartPos;
				cbb.m_nEndPos = nRelEndPos;
				m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos1, nTmpEndPos1, cbb.m_dwIP, cbb.m_BBRStatus));
				m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos2, nTmpEndPos2, cbb.m_dwIP, cbb.m_BBRStatus));
			} else if (cbb.m_nStartPos >= nRelStartPos && cbb.m_nStartPos <= nRelEndPos) {
				// need to split it
				uint64 nTmpStartPos = nRelEndPos + 1;
				uint64 nTmpEndPos = cbb.m_nEndPos;
				cbb.m_nEndPos = nRelEndPos;
				m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos, nTmpEndPos, cbb.m_dwIP, cbb.m_BBRStatus));
			} else if (cbb.m_nEndPos >= nRelStartPos && cbb.m_nEndPos <= nRelEndPos) {
				// need to split it
				uint64 nTmpStartPos = cbb.m_nStartPos;
				uint64 nTmpEndPos = nRelStartPos - 1;
				cbb.m_nStartPos = nRelStartPos;
				m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos, nTmpEndPos, cbb.m_dwIP, cbb.m_BBRStatus));
			} else
				continue;
			cbb.m_BBRStatus = BBR_VERIFIED;
#ifdef _DEBUG
			nDbgVerifiedBytes += cbb.m_nEndPos - cbb.m_nStartPos + 1;
			mapDebug.SetAt(cbb.m_dwIP, 1);
#endif
			}
	}
/*#ifdef _DEBUG
	uint32 nClients = mapDebug.GetCount();
#else
	uint32 nClients = 0;
#endif
	AddDebugLogLine(DLP_DEFAULT, false, _T("Found and marked %u recorded bytes of %u as verified in the CorruptionBlackBox records, %u(%u) records found, %u different clients"), nDbgVerifiedBytes, (nEndPos-nStartPos)+1, m_aaRecords[nPart].GetCount(), nDbgOldEntries, nClients);*/
}

void CCorruptionBlackBox::CorruptedData(uint64 nStartPos, uint64 nEndPos)
{
	if (nEndPos - nStartPos >= EMBLOCKSIZE) {
		ASSERT(0);
		return;
	}
	// convert pos to relative block pos
	UINT nPart = (UINT)(nStartPos / PARTSIZE);
	uint64 nRelStartPos = nStartPos - nPart * PARTSIZE;
	uint64 nRelEndPos = nEndPos - nPart * PARTSIZE;
	if (nRelEndPos >= PARTSIZE) {
		ASSERT(0);
		return;
	}
	if ((INT_PTR)nPart >= m_aaRecords.GetCount()) {
		//ASSERT( false );
		m_aaRecords.SetSize(nPart + 1);
	}
	uint64 nDbgVerifiedBytes = 0;
	for (INT_PTR i = 0; i < m_aaRecords[nPart].GetCount(); ++i) {
		CCBBRecord &cbb = m_aaRecords[nPart][i];
		if (cbb.m_BBRStatus == BBR_NONE) {
			if (cbb.m_nStartPos >= nRelStartPos && cbb.m_nEndPos <= nRelEndPos)
				;
			else if (cbb.m_nStartPos < nRelStartPos && cbb.m_nEndPos > nRelEndPos) {
				// need to split it 2*
				uint64 nTmpStartPos1 = nRelEndPos + 1;
				uint64 nTmpEndPos1 = cbb.m_nEndPos;
				uint64 nTmpStartPos2 = cbb.m_nStartPos;
				uint64 nTmpEndPos2 = nRelStartPos - 1;
				cbb.m_nStartPos = nRelStartPos;
				cbb.m_nEndPos = nRelEndPos;
				m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos1, nTmpEndPos1, cbb.m_dwIP, cbb.m_BBRStatus));
				m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos2, nTmpEndPos2, cbb.m_dwIP, cbb.m_BBRStatus));
			} else if (cbb.m_nStartPos >= nRelStartPos && cbb.m_nStartPos <= nRelEndPos) {
				// need to split it
				uint64 nTmpStartPos = nRelEndPos + 1;
				uint64 nTmpEndPos = cbb.m_nEndPos;
				cbb.m_nEndPos = nRelEndPos;
				m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos, nTmpEndPos, cbb.m_dwIP, cbb.m_BBRStatus));
			} else if (cbb.m_nEndPos >= nRelStartPos && cbb.m_nEndPos <= nRelEndPos) {
				// need to split it
				uint64 nTmpStartPos = cbb.m_nStartPos;
				uint64 nTmpEndPos = nRelStartPos - 1;
				cbb.m_nStartPos = nRelStartPos;
				m_aaRecords[nPart].Add(CCBBRecord(nTmpStartPos, nTmpEndPos, cbb.m_dwIP, cbb.m_BBRStatus));
			} else
				continue;
			cbb.m_BBRStatus = BBR_CORRUPTED;
			nDbgVerifiedBytes += cbb.m_nEndPos - cbb.m_nStartPos + 1;
		}
	}
	AddDebugLogLine(DLP_HIGH, false, _T("Found and marked %I64u recorded bytes of %I64u as corrupted in the CorruptionBlackBox records"), nDbgVerifiedBytes, (nEndPos - nStartPos) + 1);
}

void CCorruptionBlackBox::EvaluateData(uint16 nPart)
{
	CArray<uint32, uint32> aGuiltyClients;
	for (INT_PTR i = 0; i < m_aaRecords[nPart].GetCount(); ++i)
		if (m_aaRecords[nPart][i].m_BBRStatus == BBR_CORRUPTED)
			aGuiltyClients.Add(m_aaRecords[nPart][i].m_dwIP);

	// check if any IPs are already banned, so we can skip the test for those
	for (INT_PTR k = 0; k < aGuiltyClients.GetCount();) {
		// remove doubles
		for (INT_PTR y = aGuiltyClients.GetCount(); --y > k;)
			if (aGuiltyClients[k] == aGuiltyClients[y])
				aGuiltyClients.RemoveAt(y);

		if (theApp.clientlist->IsBannedClient(aGuiltyClients[k])) {
			AddDebugLogLine(DLP_DEFAULT, false, _T("CorruptionBlackBox: Suspicious IP (%s) is already banned, skipping recheck"), (LPCTSTR)ipstr(aGuiltyClients[k]));
			aGuiltyClients.RemoveAt(k);
		} else
			++k;
	}
	if (aGuiltyClients.IsEmpty())
		return;

	// parse all recorded data for this file to produce statistics for the involved clients
	// first init arrays for the statistics
	CArray<uint64> aDataCorrupt, aDataVerified;
	aDataCorrupt.SetSize(aGuiltyClients.GetCount());
	memset(&aDataCorrupt[0], 0, aGuiltyClients.GetCount() * sizeof aDataCorrupt[0]);
	aDataVerified.SetSize(aGuiltyClients.GetCount());
	memset(&aDataVerified[0], 0, aGuiltyClients.GetCount() * sizeof aDataVerified[0]);

	// now the parsing
	for (INT_PTR iPart = 0; iPart < m_aaRecords.GetCount(); ++iPart)
		for (INT_PTR i = 0; i < m_aaRecords[iPart].GetCount(); ++i)
			for (INT_PTR k = 0; k < aGuiltyClients.GetCount(); ++k) {
				const CCBBRecord &cbb = m_aaRecords[iPart][i];
				if (cbb.m_dwIP == aGuiltyClients[k])
					if (cbb.m_BBRStatus == BBR_CORRUPTED)
						// corrupted data records are always counted as at least blocksize or bigger
						aDataCorrupt[k] += max(cbb.m_nEndPos - cbb.m_nStartPos + 1, (uint64)EMBLOCKSIZE);
					else if (cbb.m_BBRStatus == BBR_VERIFIED)
						aDataVerified[k] += cbb.m_nEndPos - cbb.m_nStartPos + 1;
			}

	for (INT_PTR k = 0; k < aGuiltyClients.GetCount(); ++k) {
		// calculate the percentage of corrupted data for each client and ban
		// him if the limit is reached
		int nCorruptPercentage;
		if ((aDataVerified[k] + aDataCorrupt[k]) > 0)
			nCorruptPercentage = (int)((aDataCorrupt[k] * 100) / (aDataVerified[k] + aDataCorrupt[k]));
		else {
			AddDebugLogLine(DLP_HIGH, false, _T("CorruptionBlackBox: Program Error: No records for guilty client found!"));
			ASSERT(0);
			nCorruptPercentage = 0;
		}
		CUpDownClient *pClient = theApp.clientlist->FindClientByIP(aGuiltyClients[k]);
		if (nCorruptPercentage > CBB_BANTHRESHOLD) { //evil client
			if (pClient != NULL) {
				AddDebugLogLine(DLP_HIGH, false, _T("CorruptionBlackBox: Banning: Found client which sent %s of %s corrupted data, %s"), (LPCTSTR)CastItoXBytes(aDataCorrupt[k]), (LPCTSTR)CastItoXBytes((aDataVerified[k] + aDataCorrupt[k])), (LPCTSTR)pClient->DbgGetClientInfo());
				theApp.clientlist->AddTrackClient(pClient);
				pClient->Ban(_T("Identified as a sender of corrupt data"));
			} else {
				AddDebugLogLine(DLP_HIGH, false, _T("CorruptionBlackBox: Banning: Found client which sent %s of %s corrupted data, %s"), (LPCTSTR)CastItoXBytes(aDataCorrupt[k]), (LPCTSTR)CastItoXBytes((aDataVerified[k] + aDataCorrupt[k])), (LPCTSTR)ipstr(aGuiltyClients[k]));
				theApp.clientlist->AddBannedClient(aGuiltyClients[k]);
			}
		} else { //suspected client
			if (pClient != NULL) {
				AddDebugLogLine(DLP_DEFAULT, false, _T("CorruptionBlackBox: Reporting: Found client which probably sent %s of %s corrupted data, but it is within the acceptable limit, %s"), (LPCTSTR)CastItoXBytes(aDataCorrupt[k]), (LPCTSTR)CastItoXBytes((aDataVerified[k] + aDataCorrupt[k])), (LPCTSTR)pClient->DbgGetClientInfo());
				theApp.clientlist->AddTrackClient(pClient);
			} else
				AddDebugLogLine(DLP_DEFAULT, false, _T("CorruptionBlackBox: Reporting: Found client which probably sent %s of %s corrupted data, but it is within the acceptable limit, %s"), (LPCTSTR)CastItoXBytes(aDataCorrupt[k]), (LPCTSTR)CastItoXBytes((aDataVerified[k] + aDataCorrupt[k])), (LPCTSTR)ipstr(aGuiltyClients[k]));
		}
	}
}