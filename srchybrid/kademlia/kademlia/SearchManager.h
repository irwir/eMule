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

#define INV_KAD_KEYWORD_CHARS	" ()[]{}<>,._-!?:;\\/\""
extern LPCSTR g_aszInvKadKeywordCharsA;
extern LPCTSTR g_aszInvKadKeywordChars;
extern LPCWSTR g_awszInvKadKeywordChars;

namespace Kademlia
{
	void deleteTagListEntries(TagList *plistTag);

	class CRoutingZone;
	class CKadClientSearcher;
	class CSearchManager
	{
		friend class CRoutingZone;
		friend class CKademlia;
	public:
		static bool IsSearching(uint32 uSearchID);
		static void StopSearch(uint32 uSearchID, bool bDelayDelete);
		static void StopAllSearches();
		// Search for a particular file
		// Will return unique search id, returns zero if already searching for this file.
		static CSearch* PrepareLookup(uint32 uType, bool bStart, const CUInt128 &uID);
		// Will return unique search id, returns zero if already searching for this keyword.
		static CSearch* PrepareFindKeywords(LPCWSTR szKeyword, UINT uSearchTermsSize, LPBYTE pucSearchTermsData);
		static bool StartSearch(CSearch *pSearch);
		static void ProcessResponse(const CUInt128 &uTarget, uint32 uFromIP, uint16 uFromPort, ContactList *plistResults);
		static uint8 GetExpectedResponseContactCount(const CUInt128 &uTarget);
		static void ProcessResult(const CUInt128 &uTarget, const CUInt128 &uAnswer, TagList *plistInfo, uint32 uFromIP, uint16 uFromPort);
		static void ProcessPublishResult(const CUInt128 &uTarget, const uint8 uLoad, const bool bLoadResponse);
		static void GetWords(LPCWSTR sz, WordList *plistWords);
		static void UpdateStats();
		static bool AlreadySearchingFor(const CUInt128 &uTarget);

		static void CancelNodeFWCheckUDPSearch();
		static bool FindNodeFWCheckUDP();
		static bool IsFWCheckUDPSearch(const CUInt128 &uTarget);
		static void SetNextSearchID(uint32 uNextID)		{ m_uNextID = uNextID; }
	private:
		static void FindNode(const CUInt128 &uID, bool bComplete);
		static bool FindNodeSpecial(const CUInt128 &uID, CKadClientSearcher *pRequester);
		static void CancelNodeSpecial(CKadClientSearcher *pRequester);
		static void JumpStart();
		static uint32 m_uNextID;
		static SearchMap m_mapSearches;
	};
}