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

// Note To Mods //
/*
Please do not change anything here and release it.
There is going to be a new forum created just for the Kademlia side of the client.
If you feel there is an error or a way to improve something, please
post it in the forum first and let us look at it. If it is a real improvement,
it will be added to the official client. Changing something without knowing
what all it does can cause great harm to the network if released in mass form.
Any mod that changes anything within the Kademlia side will not be allowed to advertise
their client on the eMule forum.
*/

#pragma once
#include "shahashset.h"
#include "kademlia/kademlia/Tag.h"

struct SSearchTerm;

namespace Kademlia
{
	class CDataIO;
	class CEntry
	{
	protected:
		struct structFileNameEntry
		{
			CKadTagValueString	m_fileName;
			uint32				m_uPopularityIndex;
		};
	public:
		CEntry();
		virtual	~CEntry();

		virtual	CEntry* Copy();
		virtual bool IsKeyEntry()					{ return false; }

		uint64	GetIntTagValue(const CKadTagNameString &strTagName, bool bIncludeVirtualTags = true) const;
		bool	GetIntTagValue(const CKadTagNameString &strTagName, uint64 &rValue, bool bIncludeVirtualTags = true) const;
		CKadTagValueString GetStrTagValue(const CKadTagNameString &strTagName) const;
		void	AddTag(CKadTag *pTag, uint32 uDbgSourceIP = 0);
		uint32	GetTagCount() const; // Adds filename and size to the count if not empty, even if they are not stored as tags
		void	WriteTagList(CDataIO *pData)	{ WriteTagListInc(pData, 0); }

		CKadTagValueString	GetCommonFileNameLowerCase() const;
		CKadTagValueString	GetCommonFileName() const;
		void		SetFileName(const CKadTagValueString &strName);

		uint32	m_uIP;
		uint16	m_uTCPPort;
		uint16	m_uUDPPort;
		CUInt128 m_uKeyID;
		CUInt128 m_uSourceID;
		uint64	m_uSize;
		time_t	m_tLifetime;
		bool	m_bSource;

	protected:
		void	WriteTagListInc(CDataIO *pData, uint32 nIncreaseTagNumber = 0);
		CList<structFileNameEntry>	m_listFileNames;
		TagList m_listTag;
	};

	class CKeyEntry : public CEntry
	{
	protected:
		struct structPublishingIP
		{
			time_t	m_tLastPublish;
			uint32	m_uIP;
			uint16	m_wAICHHashIdx;
		};
	public:
		CKeyEntry();
		virtual	~CKeyEntry();

		virtual	CEntry*	Copy();
		virtual bool IsKeyEntry()					{ return true; }

		bool	StartSearchTermsMatch(const SSearchTerm &rSearchTerm);
		void	MergeIPsAndFilenames(CKeyEntry *pFromEntry);
		void	CleanUpTrackedPublishers();
		float	GetTrustValue();
		void	WritePublishTrackingDataToFile(CDataIO *pData);
		void	ReadPublishTrackingDataFromFile(CDataIO *pData, bool bIncludesAICH);
		void	DirtyDeletePublishData();
		void	WriteTagListWithPublishInfo(CDataIO *pData);
		uint16	AddRemoveAICHHash(const CAICHHash &hash, bool bAdd);
		uint16	GetAICHHashCount() const			{ return (uint16)m_aAICHHashes.GetCount(); }
		static void	ResetGlobalTrackingMap()		{ s_mapGlobalPublishIPs.RemoveAll(); }

	protected:
		void	RecalcualteTrustValue();
		static void	AdjustGlobalPublishTracking(uint32 uIP, bool bIncrease, const CString &strDbgReason);

		float	m_fTrustValue;
		CArray<uint8>		m_anAICHHashPopularity;
		CArray<CAICHHash>	m_aAICHHashes;
		static CMap<uint32, uint32, uint32, uint32> s_mapGlobalPublishIPs; // tracks count of publishings for each 255.255.255.0/28 subnet
		CList<structPublishingIP> *m_pliPublishingIPs;
		uint32	dwLastTrustValueCalc;
	private:
		bool SearchTermsMatch(const SSearchTerm &rSearchTerm) const;
		CKadTagValueString m_strSearchTermCacheCommonFileNameLowerCase; // contains a valid value only while 'SearchTermsMatch' is running.
	};
}