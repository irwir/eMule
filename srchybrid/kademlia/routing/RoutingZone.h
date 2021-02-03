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


This work is based on the java implementation of the Kademlia protocol.
Kademlia: Peer-to-peer routing based on the XOR metric
Copyright (C) 2002  Petar Maymounkov [petar@post.harvard.edu]
http://kademlia.scs.cs.nyu.edu
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
#include "SafeFile.h"
#include "kademlia/routing/Maps.h"

namespace Kademlia
{
	class CRoutingBin;
	class CKadUDPKey;
	/**
	 * The *Zone* is just a node in a binary tree of *Zone*s.
	 * Each zone is either an internal node or a leaf node.
	 * Internal nodes have "bin == null" and "subZones[i] != null",
	 * leaf nodes have "subZones[i] == null" and "bin != null".
	 *
	 * All key pseudoaddresses are relative to the center (self), which
	 * is considered to be 000..000
	 */
	class CRoutingZone
	{
		public:
			CRoutingZone();
			explicit CRoutingZone(LPCSTR szFilename);
			~CRoutingZone();

			uint32		Consolidate();
			bool		OnBigTimer();
			void		OnSmallTimer();
			bool		Add(const CUInt128 &uID, uint32 uIP, uint16 uUDPPort, uint16 uTCPPort, uint8 uVersion, const CKadUDPKey &cUDPKey, bool &bIPVerified, bool bUpdate, bool bFromNodesDat, bool bFromHello);
			bool		AddUnfiltered(const CUInt128 &uID, uint32 uIP, uint16 uUDPPort, uint16 uTCPPort, uint8 uVersion, const CKadUDPKey &cUDPKey, bool &bIPVerified, bool bUpdate, bool bFromNodesDat, bool bFromHello);
			bool		Add(CContact *pContact, bool &bUpdate, bool &bOutIPVerified);
			void		ReadFile(const CString &strSpecialNodesdate = CString());
			bool		VerifyContact(const CUInt128 &uID, uint32 uIP) const;
			CContact*	GetContact(const CUInt128 &uID) const;
			CContact*	GetContact(uint32 uIP, uint16 nPort, bool bTCPPort) const;
			CContact*	GetRandomContact(uint32 nMaxType, uint32 nMinKadVersion) const;
			uint32		GetNumContacts() const;
			void		GetNumContacts(uint32 &nInOutContacts, uint32 &nInOutFilteredContacts, uint8 byMinVersion) const;
			// Check if we know a contact with the same ID or IP but non-matching IP/ID and other limitations, similar checks like when adding a node to the table except allowing duplicates
			bool		IsAcceptableContact(const CContact *pToCheck) const;
			// Returns a list of all contacts in all leafs of this zone.
			void		GetAllEntries(ContactList *plistResult, bool bEmptyFirst = true);
			// Returns the *maxRequired* tokens that are closest to the target within this zone's subtree.
			void		GetClosestTo(uint32 uMaxType, const CUInt128 &uTarget, const CUInt128 &uDistance, uint32 uMaxRequired, ContactMap *plistResult, bool bEmptyFirst = true, bool bSetInUse = false) const;
			// Ideally: Returns all contacts that are in buckets of common range between us and the asker.
			// In practice: returns the contacts from the top (2^{logBase+1}) buckets.
			uint32		GetBootstrapContacts(ContactList *plistResult, uint32 uMaxRequired);
			uint32		EstimateCount();
			bool		HasOnlyLANNodes() const;
			time_t		m_tNextBigTimer;
			time_t		m_tNextSmallTimer;
		private:
			CRoutingZone(CRoutingZone *pSuper_zone, int iLevel, const CUInt128 &uZone_index);
			void Init(CRoutingZone *pSuper_zone, int iLevel, const CUInt128 &uZone_index);
			static void ReadBootstrapNodesDat(CFileDataIO &file);
			void DbgWriteBootstrapFile();

			void WriteFile();
			bool IsLeaf() const;
			bool CanSplit() const;
			// Returns all contacts from this zone tree that are no deeper than *depth* from the current zone.
			void TopDepth(int iDepth, ContactList *plistResult, bool bEmptyFirst = true);
			// Returns the maximum depth of the tree as the number of edges of the longest path to a leaf.
			UINT GetMaxDepth() const;
			void RandomBin(ContactList *plistResult, bool bEmptyFirst = true);
			void Split();
			void StartTimer();
			void StopTimer();
			void RandomLookup();
			void SetAllContactsVerified();
			/**
			* Generates a new TokenBin for this zone. Used when the current zone is becoming a leaf zone.
			* Must be deleted by caller
			*/
			CRoutingZone* GenSubZone(int iSide);
			/**
			* Zone pair is an array of two. Either both entries are null, which
			* means that *this* is a leaf zone, or both are non-null which means
			* that *this* has been split into equally sized finer zones.
			* The zone with index 0 is the one closer to our *self* token.
			*/
			CRoutingZone		*m_pSubZones[2];
			CRoutingZone		*m_pSuperZone;
			static CString		m_sFilename;
			static CUInt128		uMe;
			/**
			* The level indicates what size chunk of the address space
			* this zone is representing. Level 0 is the whole space,
			* level 1 is 1/2 of the space, level 2 is 1/4, etc.
			*/
			UINT m_uLevel;
			/**
			* This is the distance in number of zones from the zone at this level
			* that contains the center of the system; distance is wrt the XOR metric.
			*/
			CUInt128 m_uZoneIndex;
			/** List of contacts, if this zone is a leaf zone. */
			CRoutingBin *m_pBin;
	};
}