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
#include "deadsourcelist.h"
#include "opcodes.h"
#include "updownclient.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define	CLEANUPTIME			MIN2MS(60)

#define BLOCKTIME		(::GetTickCount() + MIN2MS(m_bGlobalList ? 15 : 45))
#define BLOCKTIMEFW		(::GetTickCount() + MIN2MS(m_bGlobalList ? 30 : 45))

///////////////////////////////////////////////////////////////////////////////////////
//// CDeadSource

CDeadSource::CDeadSource(uint32 dwID, uint16 nPort, uint32 dwServerIP, uint16 nKadPort)
	: m_dwID(dwID)
	, m_dwServerIP(dwServerIP)
	, m_nPort(nPort)
	, m_nKadPort(nKadPort)
	, m_aucHash()
{
}

CDeadSource::CDeadSource(const uchar *paucHash)
	: m_dwID()
	, m_dwServerIP()
	, m_nPort()
	, m_nKadPort()
{
	md4cpy(m_aucHash, paucHash);
}

bool operator==(const CDeadSource &ds1, const CDeadSource &ds2){
	//ASSERT( ((ds1.m_dwID + ds1.m_dwServerIP) ^ isnulmd4(ds1.m_aucHash)) != 0 );
	//ASSERT( ((ds2.m_dwID + ds2.m_dwServerIP) ^ isnulmd4(ds2.m_aucHash)) != 0 );
	return (
		// lowid ed2k and highid kad + ed2k check
		((ds1.m_dwID != 0 && ds1.m_dwID == ds2.m_dwID) && ((ds1.m_nPort != 0 && ds1.m_nPort == ds2.m_nPort) || (ds1.m_nKadPort != 0 && ds1.m_nKadPort == ds2.m_nKadPort)) && (ds1.m_dwServerIP == ds2.m_dwServerIP || !::IsLowID(ds1.m_dwID)) )
		// lowid kad check
		|| (::IsLowID(ds1.m_dwID) && !isnulmd4(ds1.m_aucHash) && md4equ(ds1.m_aucHash, ds2.m_aucHash)) );
}

CDeadSource& CDeadSource::operator=(const CDeadSource &ds)
{
	m_dwID = ds.m_dwID;
	m_dwServerIP = ds.m_dwServerIP;
	m_nPort = ds.m_nPort;
	m_nKadPort = ds.m_nKadPort;
	md4cpy(m_aucHash, ds.m_aucHash);
	return *this;
}

///////////////////////////////////////////////////////////////////////////////////////
//// CDeadSourceList

CDeadSourceList::CDeadSourceList()
	: m_dwLastCleanUp()
	, m_bGlobalList()
{
}

void CDeadSourceList::Init(bool bGlobalList)
{
	m_dwLastCleanUp = ::GetTickCount();
	m_mapDeadSources.InitHashTable(bGlobalList ? 3001 : 503);
	m_bGlobalList = bGlobalList;
}

bool CDeadSourceList::IsDeadSource(const CUpDownClient *pToCheck) const
{
	uint32 dwExpTime;
//	bool bDbgCheck = false;
	if (!pToCheck->HasLowID() || pToCheck->GetServerIP() != 0) {
		if (m_mapDeadSources.Lookup(CDeadSource(pToCheck->GetUserIDHybrid(), pToCheck->GetUserPort(), pToCheck->GetServerIP(), pToCheck->GetKadPort()), dwExpTime))
			if (::GetTickCount() < dwExpTime)
				return true;
//		bDbgCheck = true;
	}
	if (((pToCheck->HasValidBuddyID() || pToCheck->SupportsDirectUDPCallback()) && !isnulmd4(pToCheck->GetUserHash())) || (pToCheck->HasLowID() && pToCheck->GetServerIP() == 0)) {
		if (m_mapDeadSources.Lookup(CDeadSource(pToCheck->GetUserHash()), dwExpTime)) {
			if (::GetTickCount() < dwExpTime)
				return true;
		}
//		bDbgCheck = true;
	}
//	ASSERT ( bDbgCheck );
	return false;
}

void CDeadSourceList::AddDeadSource(const CUpDownClient *pToAdd)
{
	//if (thePrefs.GetLogFilteredIPs())
	//	AddDebugLogLine(DLP_VERYLOW, false, _T("Added source to bad source list (%s) - file %s : %s")
	//		, m_bGlobalList? _T("Global") : _T("Local")
	//		, (pToAdd->GetRequestFile() != NULL) ? (LPCTSTR)pToAdd->GetRequestFile()->GetFileName() : _T("???")
	//		, (LPCTSTR)pToAdd->DbgGetClientInfo());

	if (!pToAdd->HasLowID())
		m_mapDeadSources.SetAt(CDeadSource(pToAdd->GetUserIDHybrid(), pToAdd->GetUserPort(), pToAdd->GetServerIP(), pToAdd->GetKadPort()), BLOCKTIME);
	else {
//		bool bDbgCheck = false;
		if (pToAdd->GetServerIP() != 0) {
//			bDbgCheck = true;
			m_mapDeadSources.SetAt(CDeadSource(pToAdd->GetUserIDHybrid(), pToAdd->GetUserPort(), pToAdd->GetServerIP(), 0), BLOCKTIMEFW);
		}
		if (pToAdd->HasValidBuddyID() || pToAdd->SupportsDirectUDPCallback()) {
//			bDbgCheck = true;
			m_mapDeadSources.SetAt(CDeadSource(pToAdd->GetUserHash()), BLOCKTIMEFW);
		}
//		ASSERT( bDbgCheck );
	}
	if (::GetTickCount() >= m_dwLastCleanUp + CLEANUPTIME)
		CleanUp();
}

void CDeadSourceList::RemoveDeadSource(const CUpDownClient *client)
{
	if (!client->HasLowID())
		m_mapDeadSources.RemoveKey(CDeadSource(client->GetUserIDHybrid(), client->GetUserPort(), client->GetServerIP(), client->GetKadPort()));
	else {
		if (client->GetServerIP() != 0)
			m_mapDeadSources.RemoveKey(CDeadSource(client->GetUserIDHybrid(), client->GetUserPort(), client->GetServerIP(), 0));
		if (client->HasValidBuddyID() || client->SupportsDirectUDPCallback())
			m_mapDeadSources.RemoveKey(CDeadSource(client->GetUserHash()));
	}
}

void CDeadSourceList::CleanUp()
{
	const uint32 dwTick = ::GetTickCount();
	m_dwLastCleanUp = dwTick;
	//if (thePrefs.GetLogFilteredIPs())
	//	AddDebugLogLine(DLP_VERYLOW, false, _T("Cleaning up DeadSourceList (%s), %i clients on List..."),  m_bGlobalList ? _T("Global") : _T("Local"), m_mapDeadSources.GetCount());

	CDeadSource dsKey;
	for (POSITION pos = m_mapDeadSources.GetStartPosition(); pos != NULL;) {
		uint32 dwExpTime;
		m_mapDeadSources.GetNextAssoc(pos, dsKey, dwExpTime);
		if (dwTick >= dwExpTime)
			m_mapDeadSources.RemoveKey(dsKey);
	}
	//if (thePrefs.GetLogFilteredIPs())
	//	AddDebugLogLine(DLP_VERYLOW, false, _T("...done, %i clients left on list"), m_mapDeadSources.GetCount());
}