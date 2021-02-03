//this file is part of eMule
//Copyright (C)2002-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#pragma once
#include "kademlia/routing/Maps.h"
#include "kademlia/kademlia/Tag.h"

namespace Kademlia
{
	class CLookupHistory
	{
	public:
		struct SLookupHistoryEntry
		{
			CUInt128	m_uContactID;
			CUInt128	m_uDistance;
			CArray<int>	m_liReceivedFromIdx;
			uint32		m_dwAskedContactsTime;
			uint32		m_uRespondedContact;
			uint32		m_dwAskedSearchItemTime;
			uint32		m_uRespondedSearchItem;
			uint32		m_uIP;
			uint16		m_uPort;
			uint8		m_byContactVersion;
			bool		m_bProvidedCloser;
			bool		m_bForcedInteresting;
			bool		IsInteresting() const	{ return m_dwAskedContactsTime != 0 || m_dwAskedSearchItemTime != 0 || m_bForcedInteresting; }
		};

		CLookupHistory();
		~CLookupHistory();

		void	ContactReceived(CContact *pRecContact, CContact *pFromContact, const CUInt128 &uDistance, bool bCloser, bool bForceInteresting = false);
		void	ContactAskedKad(const CContact *pContact);
		void	ContactAskedKeyword(const CContact *pContact);
		void	ContactRespondedKeyword(uint32 uContactIP, uint16 uContactUDPPort, uint32 uResultCount);

		void	SetSearchStopped()							{ m_bSearchStopped = true; }
		void	SetSearchDeleted();
		void	SetGUIDeleted();
		void	SetUsedByGUI()								{ ++m_uRefCount; }
		void	SetGUIName(LPCWSTR sName)					{ m_sGUIName = sName; }
		void	SetSearchType(uint32 uVal)					{ m_uType = uVal; }

		bool	IsSearchStopped() const						{ return m_bSearchStopped; }
		bool	IsSearchDeleted() const						{ return m_bSearchDeleted; }

		CArray<SLookupHistoryEntry*>& GetHistoryEntries()	{ return m_aIntrestingHistoryEntries; }
		const CStringW& GetGUIName() const					{ return m_sGUIName; }
		uint32	GetType() const								{ return m_uType; }

	protected:
		int		GetInterestingContactIdxByID(const CUInt128 &uContact) const;

	private:
		CArray<SLookupHistoryEntry*> m_aHistoryEntries;
		CArray<SLookupHistoryEntry*> m_aIntrestingHistoryEntries;
		CStringW m_sGUIName;
		uint32	m_uRefCount;
		uint32	m_uType;
		bool	m_bSearchStopped;
		bool	m_bSearchDeleted;
	};
}