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
#include <map>
#include <list>
#include <set>
#include "MapKey.h"
#include "kademlia/utils/UInt128.h"

namespace Kademlia
{
	class CContact;
	class CSearch;
	class CKadTag;
	class CRoutingZone;
	class CEntry;
	class CKadTagValueString;
	typedef std::map<CUInt128, CContact*> ContactMap;
	typedef std::list<CContact*> ContactList;
	typedef CTypedPtrList<CPtrList, CContact*> _ContactList;
	typedef std::list<CUInt128> UIntList;
	typedef std::list<CKadTag*> TagList;
	typedef std::list<CKadTagValueString> WordList;
	typedef std::map<CUInt128, CSearch*> SearchMap;
	typedef std::map<CRoutingZone*, CRoutingZone*> EventMap;
	typedef CTypedPtrList<CPtrList, Kademlia::CEntry*> CKadEntryPtrList;
	struct Source
	{
		CUInt128 uSourceID;
		CKadEntryPtrList ptrlEntryList;
	};
	typedef CMap<CCKey, const CCKey&, Source*, Source*> CSourceKeyMap;
	struct KeyHash
	{
		CUInt128 uKeyID;
		CSourceKeyMap mapSource;
	};
	typedef CTypedPtrList<CPtrList, Source*> CKadSourcePtrList;
	struct SrcHash
	{
		CUInt128 uKeyID;
		CKadSourcePtrList ptrlistSource;
	};
	struct Load
	{
		CUInt128 uKeyID;
		time_t uTime;
	};
	typedef CMap<CCKey, const CCKey&, KeyHash*, KeyHash*> CKeyHashMap;
	typedef CMap<CCKey, const CCKey&, SrcHash*, SrcHash*> CSrcHashMap;
	typedef CMap<CCKey, const CCKey&, Load*, Load*> CLoadMap;
}
