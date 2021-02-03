/*
Copyright (C)2003 Barry Dunne (http://www.emule-project.net)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

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
#include "kademlia/routing/Maps.h"
#include "kademlia/kademlia/Tag.h"

class CKnownFile;
class CSafeMemFile;
struct SSearchTerm;

namespace Kademlia
{
	void deleteTagListEntries(TagList *plistTag);
	class CByteIO;
	class CKadClientSearcher;
	class CLookupHistory;
	class CSearch
	{
		friend class CSearchManager;
	public:
		uint32		GetSearchID() const;
		uint32		GetSearchType() const;
		void		SetSearchType(uint32 uVal);
		void		SetTargetID(const CUInt128 &uVal);
		CUInt128	GetTarget() const;
		uint32		GetAnswers() const;
		uint32		GetKadPacketSent() const;
		uint32		GetRequestAnswer() const;
		uint32		GetNodeLoad() const;
		uint32		GetNodeLoadResponse() const;
		uint32		GetNodeLoadTotal() const;
		const		CStringW& GetGUIName() const;
		void		SetGUIName(LPCWSTR sGUIName);
		void		SetSearchTermData(uint32 uSearchTermDataSize, LPBYTE pucSearchTermsData);
		static CString	GetTypeName(uint32 uType);

		void		AddFileID(const CUInt128 &uID);
		static void	PreparePacketForTags(CByteIO *byIO, CKnownFile *pFile, uint8 byTargetKadVersion);
		bool		Stoping() const;
		void		UpdateNodeLoad(uint8 uLoad);

		CKadClientSearcher*	GetNodeSpecialSearchRequester() const	{ return pNodeSpecialSearchRequester; }
		void				SetNodeSpecialSearchRequester(CKadClientSearcher *pNew)	{ pNodeSpecialSearchRequester = pNew; }

		CLookupHistory* GetLookupHistory() const					{ return m_pLookupHistory; }
		enum
		{
			NODE,
			NODECOMPLETE,
			FILE,
			KEYWORD,
			NOTES,
			STOREFILE,
			STOREKEYWORD,
			STORENOTES,
			FINDBUDDY,
			FINDSOURCE,
			NODESPECIAL, // node search request from requester "outside" of kad to find the IP of a given nodeid
			NODEFWCHECKUDP // find new unknown IPs for a UDP firewall check
		};

		CSearch();
		~CSearch();
		CSearch(const CSearch&) = delete;
		CSearch& operator=(const CSearch&) = delete;

	private:
		void Go();
		void ProcessResponse(uint32 uFromIP, uint16 uFromPort, ContactList *plistResults);
		void ProcessResult(const CUInt128 &uAnswer, TagList *plistInfo, uint32 uFromIP, uint16 uFromPort);
		void ProcessResultFile(const CUInt128 &uAnswer, TagList *plistInfo);
		void ProcessResultKeyword(const CUInt128 &uAnswer, TagList *plistInfo, uint32 uFromIP, uint16 uFromPort);
		void ProcessResultNotes(const CUInt128 &uAnswer, TagList *plistInfo);
		void JumpStart();
		void SendFindValue(CContact *pContact, bool bReAskMore = false);
		void PrepareToStop();
		void StorePacket();
		uint8 GetRequestContactCount() const;

		bool m_bStoping;
		time_t m_tCreated;
		uint32 m_uType;
		uint32 m_uAnswers;
		uint32 m_uTotalRequestAnswers;
		uint32 m_uKadPacketSent; //Used for gui reasons. May not be needed later.
		uint32 m_uTotalLoad;
		uint32 m_uTotalLoadResponses;
		time_t m_uLastResponse;
		uint32 m_uSearchID;
		uint32 m_uSearchTermsDataSize;
		LPBYTE m_pucSearchTermsData;
		SSearchTerm *m_pSearchTerm; // cached from m_pucSearchTermsData, used for verifying results later on
		CKadClientSearcher *pNodeSpecialSearchRequester; // used to callback on result for NODESPECIAL searches
		CUInt128 m_uTarget;
		WordList m_listWords;
		UIntList m_listFileIDs;
		ContactMap m_mapPossible;
		ContactMap m_mapTried;
		std::map<Kademlia::CUInt128, bool> m_mapResponded;
		ContactMap m_mapBest;
		ContactList m_listDelete;
		ContactMap m_mapInUse;
		CUInt128 m_uClosestDistantFound; // not used for the search itself, but for statistical data collecting
		CLookupHistory *m_pLookupHistory;
		CContact *pRequestedMoreNodesContact;
	};
}
void KadGetKeywordHash(const Kademlia::CKadTagValueString &rstrKeywordW, Kademlia::CUInt128 *puKadID);
void KadGetKeywordHash(const CStringA &rstrKeywordA, Kademlia::CUInt128 *puKadID);
CStringA KadGetKeywordBytes(const Kademlia::CKadTagValueString &rstrKeywordW);