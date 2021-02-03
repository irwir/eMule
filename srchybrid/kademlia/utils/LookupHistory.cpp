//this file is part of eMule
//Copyright (C)2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include "emule.h"
#include "kademlia/kademlia/search.h"
#include "kademlia/routing/contact.h"
#include "kademlia/utils/LookupHistory.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

CLookupHistory::CLookupHistory()
	: m_uRefCount(1)
	, m_uType()
	, m_bSearchStopped()
	, m_bSearchDeleted()
{
}

CLookupHistory::~CLookupHistory()
{
	for (INT_PTR i = m_aHistoryEntries.GetCount(); --i >= 0;)
		delete m_aHistoryEntries[i];
	m_aHistoryEntries.RemoveAll();
}

void CLookupHistory::SetSearchDeleted()
{
	m_bSearchDeleted = true;
	SetGUIDeleted();
}

void CLookupHistory::SetGUIDeleted()
{
	ASSERT(m_uRefCount);
	if (--m_uRefCount == 0)
		delete this;
}

void CLookupHistory::ContactReceived(CContact *pRecContact, CContact *pFromContact, const CUInt128 &uDistance, bool bCloser, bool bForceInteresting)
{
	// Do we know this contact already? If pRecContact is NULL we only set the responded flag to the pFromContact
	for (int i = (int)m_aHistoryEntries.GetCount(); --i >= 0;)
		if (uDistance == m_aHistoryEntries[i]->m_uDistance || pRecContact == NULL) {
			if (pFromContact != NULL) {
				int iIdx = GetInterestingContactIdxByID(pFromContact->GetClientID());
				if (iIdx >= 0) {
					if (pRecContact != NULL)
						m_aHistoryEntries[i]->m_liReceivedFromIdx.Add(iIdx);
					m_aIntrestingHistoryEntries[iIdx]->m_uRespondedContact++;
					if (bCloser)
						m_aIntrestingHistoryEntries[iIdx]->m_bProvidedCloser = true;
				}
			}
			return;
		}

	SLookupHistoryEntry *pstructNewEntry = new SLookupHistoryEntry;
	pstructNewEntry->m_uRespondedContact = 0;
	pstructNewEntry->m_uRespondedSearchItem = 0;
	pstructNewEntry->m_bProvidedCloser = false;
	pstructNewEntry->m_dwAskedContactsTime = 0;
	pstructNewEntry->m_dwAskedSearchItemTime = 0;
	if (pFromContact != NULL) {
		int iIdx = GetInterestingContactIdxByID(pFromContact->GetClientID());
		if (iIdx >= 0) {
			pstructNewEntry->m_liReceivedFromIdx.Add(iIdx);
			m_aIntrestingHistoryEntries[iIdx]->m_uRespondedContact++;
			if (bCloser)
				m_aIntrestingHistoryEntries[iIdx]->m_bProvidedCloser = true;
		}
	}
	pstructNewEntry->m_uContactID = pRecContact->GetClientID();
	pstructNewEntry->m_byContactVersion = pRecContact->GetVersion();
	pstructNewEntry->m_uDistance = uDistance;
	pstructNewEntry->m_uIP = pRecContact->GetIPAddress();
	pstructNewEntry->m_uPort = pRecContact->GetUDPPort();
	pstructNewEntry->m_bForcedInteresting = bForceInteresting;
	m_aHistoryEntries.Add(pstructNewEntry);
	if (bForceInteresting)
		m_aIntrestingHistoryEntries.Add(pstructNewEntry);
}

void CLookupHistory::ContactAskedKad(const CContact *pContact)
{
	// Find contact
	for (int i = (int)m_aHistoryEntries.GetCount(); --i >= 0;)
		if (pContact->GetClientID() == m_aHistoryEntries[i]->m_uContactID) {
			if (!m_aHistoryEntries[i]->IsInteresting())
				m_aIntrestingHistoryEntries.Add(m_aHistoryEntries[i]);
			m_aHistoryEntries[i]->m_dwAskedContactsTime = ::GetTickCount();
			return;
		}

	ASSERT(0);
}

int CLookupHistory::GetInterestingContactIdxByID(const CUInt128 &uContact) const
{
	for (int i = (int)m_aIntrestingHistoryEntries.GetCount(); --i >= 0;)
		if (uContact == m_aIntrestingHistoryEntries[i]->m_uContactID)
			return i;

	ASSERT(0);
	return -1;
}

void CLookupHistory::ContactAskedKeyword(const CContact *pContact)
{
	// Find contact
	for (int i = (int)m_aHistoryEntries.GetCount(); --i >= 0;)
		if (pContact->GetClientID() == m_aHistoryEntries[i]->m_uContactID) {
			if (!m_aHistoryEntries[i]->IsInteresting())
				m_aIntrestingHistoryEntries.Add(m_aHistoryEntries[i]);
			m_aHistoryEntries[i]->m_dwAskedSearchItemTime = ::GetTickCount();
			ASSERT(m_aHistoryEntries[i]->m_uRespondedSearchItem == 0);
			return;
		}

	ASSERT(0);
}

void CLookupHistory::ContactRespondedKeyword(uint32 uContactIP, uint16 uContactUDPPort, uint32 uResultCount)
{
	for (int i = (int)m_aIntrestingHistoryEntries.GetCount(); --i >= 0;)
		if ((m_aIntrestingHistoryEntries[i]->m_uIP == uContactIP) && (m_aIntrestingHistoryEntries[i]->m_uPort == uContactUDPPort)) {
			ASSERT(m_aIntrestingHistoryEntries[i]->m_dwAskedSearchItemTime > 0 || m_uType == CSearch::NODE || m_uType == CSearch::NODEFWCHECKUDP);
			m_aIntrestingHistoryEntries[i]->m_uRespondedSearchItem += uResultCount;
			return;
		}
	//ASSERT(0);
}