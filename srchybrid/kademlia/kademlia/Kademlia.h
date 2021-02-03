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

class CSharedFileList;
struct Status;

namespace Kademlia
{
	class CPrefs;
	class CKademliaUDPListener;
	class CIndexed;
	class CKadUDPKey;
	class CKadClientSearcher;
	class CKademlia
	{
#ifdef _BOOTSTRAPNODESDAT
		friend CRoutingZone;
#endif
	public:
		CKademlia();

		static void Start();
		static void	Start(CPrefs *pPrefs);
		static void	Stop();
		static CPrefs* GetPrefs();
		static CRoutingZone* GetRoutingZone();
		static CKademliaUDPListener* GetUDPListener();
		static CIndexed* GetIndexed();
		static bool	IsRunning();
		static bool	IsConnected();
		static bool	IsFirewalled();
		static void	RecheckFirewalled();
		static uint32 GetKademliaUsers(bool bNewMethod = false);
		static uint32 GetKademliaFiles();
		static uint32 GetTotalStoreKey();
		static uint32 GetTotalStoreSrc();
		static uint32 GetTotalStoreNotes();
		static uint32 GetTotalFile();
		static bool	GetPublish();
		static uint32 GetIPAddress();
		static void	Bootstrap(uint32 uIP, uint16 uPort);
		static void	Bootstrap(LPCTSTR szHost, uint16 uPort);
		static void	ProcessPacket(const byte *pbyData, uint32 uLenData, uint32 uIP, uint16 uPort, bool bValidReceiverKey, const CKadUDPKey &senderUDPKey);
		static void	AddEvent(CRoutingZone *pZone);
		static void	RemoveEvent(CRoutingZone *pZone);
		static void	Process();
		static bool	InitUnicode(HMODULE hInst);
		static void StatsAddClosestDistance(const CUInt128 &uDist);
		static bool IsRunningInLANMode();

		static bool	FindNodeIDByIP(CKadClientSearcher &rRequester, uint32 dwIP, uint16 nTCPPort, uint16 nUDPPort);
		static bool FindIPByNodeID(CKadClientSearcher &rRequester, const uchar *pachNodeID);
		static void	CancelClientSearch(CKadClientSearcher &rFromRequester);

		static _ContactList	s_liBootstrapList;

	private:
		static uint32 CalculateKadUsersNew();

		static CKademlia *m_pInstance;
		static EventMap	m_mapEvents;
		static time_t m_tNextSearchJumpStart;
		static time_t m_tNextSelfLookup;
		static time_t m_tNextFirewallCheck;
		static time_t m_tNextUPnPCheck;
		static time_t m_tNextFindBuddy;
		static time_t m_tStatusUpdate;
		static time_t m_tBigTimer;
		static time_t m_tBootstrap;
		static time_t m_tConsolidate;
		static time_t m_tExternPortLookup;
		static time_t m_tLANModeCheck;
		static volatile bool m_bRunning;
		static CList<uint32, uint32> m_liStatsEstUsersProbes;
		static bool m_bLANMode;
		static bool	m_bootstrapping; // used for GUI

		CPrefs *m_pPrefs;
		CRoutingZone *m_pRoutingZone;
		CKademliaUDPListener *m_pUDPListener;
		CIndexed *m_pIndexed;
	};
}