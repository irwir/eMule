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
#pragma once
#include "DeadSourceList.h"

class CClientReqSocket;
class CUpDownClient;
namespace Kademlia
{
	class CContact;
	class CUInt128;
};
typedef CTypedPtrList<CPtrList, CUpDownClient*> CUpDownClientPtrList;

#define	NUM_CLIENTLIST_STATS	19
#define BAN_CLEANUP_TIME		MIN2MS(20)

//------------CDeletedClient Class----------------------
// this class / list is a bit overkill, but currently needed to avoid any exploit possibility
// it will keep track of certain clients attributes for 2 hours, while the CUpDownClient object might be deleted already
// currently: IP, Port, UserHash
struct PORTANDHASH
{
	uint16 nPort;
	void *pHash;
};

struct IPANDTICS
{
	uint32 dwIP;
	uint32 dwInserted;
};
struct CONNECTINGCLIENT
{
	CUpDownClient *pClient;
	uint32 dwInserted;
};


class CDeletedClient
{
public:
	explicit CDeletedClient(const CUpDownClient *pClient);
	CArray<PORTANDHASH> m_ItemsList;
	uint32				m_dwInserted;
	uint32				m_cBadRequest;
};

enum buddyState
{
	Disconnected,
	Connecting,
	Connected
};

// ----------------------CClientList Class---------------
class CClientList
{
	friend class CClientListCtrl;

public:
	CClientList();
	~CClientList();

	// Clients
	void	AddClient(CUpDownClient *toadd, bool bSkipDupTest = false);
	void	RemoveClient(CUpDownClient *toremove, LPCTSTR pszReason = NULL);
	void	GetStatistics(uint32 &ruTotalClients, int stats[NUM_CLIENTLIST_STATS],
						  CMap<uint32, uint32, uint32, uint32> &clientVersionEDonkey,
						  CMap<uint32, uint32, uint32, uint32> &clientVersionEDonkeyHybrid,
						  CMap<uint32, uint32, uint32, uint32> &clientVersionEMule,
						  CMap<uint32, uint32, uint32, uint32> &clientVersionAMule);
	INT_PTR	GetClientCount()							{ return list.GetCount(); }
	void	DeleteAll();
	bool	AttachToAlreadyKnown(CUpDownClient **client, CClientReqSocket *sender);
	CUpDownClient* FindClientByConnIP(uint32 clientip, UINT port) const;
	CUpDownClient* FindClientByIP(uint32 clientip, UINT port) const;
	CUpDownClient* FindClientByUserHash(const uchar *clienthash, uint32 dwIP = 0, uint16 nTCPPort = 0) const;
	CUpDownClient* FindClientByIP(uint32 clientip) const;
	CUpDownClient* FindClientByIP_UDP(uint32 clientip, UINT nUDPport) const;
	CUpDownClient* FindClientByServerID(uint32 uServerIP, uint32 uED2KUserID) const;
	CUpDownClient* FindClientByUserID_KadPort(uint32 clientID, uint16 kadPort) const;
	CUpDownClient* FindClientByIP_KadPort(uint32 ip, uint16 port) const;

	// Banned clients
	void	AddBannedClient(uint32 dwIP);
	bool	IsBannedClient(uint32 dwIP) const;
	void	RemoveBannedClient(uint32 dwIP);
	INT_PTR	GetBannedCount() const						{ return m_bannedList.GetCount(); }
	void	RemoveAllBannedClients();

	// Tracked clients
	void	AddTrackClient(CUpDownClient *toadd);
	bool	ComparePriorUserhash(uint32 dwIP, uint16 nPort, void *pNewHash);
	INT_PTR	GetClientsFromIP(uint32 dwIP) const;
	void	TrackBadRequest(const CUpDownClient *upcClient, int nIncreaseCounter);
	uint32	GetBadRequests(const CUpDownClient *upcClient) const;
	INT_PTR	GetTrackedCount() const						{ return m_trackedClientsList.GetCount(); }
	void	RemoveAllTrackedClients();

	// Kad client list, buddy handling
	bool	RequestTCP(Kademlia::CContact *contact, uint8 byConnectOptions);
	void	RequestBuddy(Kademlia::CContact *contact, uint8 byConnectOptions);
	bool	IncomingBuddy(Kademlia::CContact *contact, Kademlia::CUInt128 *buddyID);
	void	RemoveFromKadList(CUpDownClient *torem);
	void	AddToKadList(CUpDownClient *toadd);
	bool	DoRequestFirewallCheckUDP(const Kademlia::CContact &contact);
	//bool	DebugDoRequestFirewallCheckUDP(uint32 ip, uint16 port);
	uint8	GetBuddyStatus() const						{ return m_nBuddyStatus; }
	CUpDownClient* GetBuddy() const						{ return m_pBuddy; }

	void	AddKadFirewallRequest(uint32 dwIP);
	bool	IsKadFirewallCheckIP(uint32 dwIP) const;

	// Direct Callback List
	void	AddTrackCallbackRequests(uint32 dwIP);
	bool	AllowCalbackRequest(uint32 dwIP) const;

	// Connecting Clients
	void	AddConnectingClient(CUpDownClient *pToAdd);
	void	RemoveConnectingClient(CUpDownClient *pToRemove);

	void	Process();
	bool	IsValidClient(CUpDownClient *tocheck) const;
	void	Debug_SocketDeleted(CClientReqSocket *deleted) const;

	// ZZ:UploadSpeedSense -->
	bool GiveClientsForTraceRoute();
	// ZZ:UploadSpeedSense <--

	void	ProcessA4AFClients() const; // ZZ:DownloadManager
	CDeadSourceList	m_globDeadSourceList;

protected:
	void	CleanUpClientList();
	void	ProcessConnectingClientsList();

private:
	CUpDownClientPtrList list;
	CUpDownClientPtrList m_KadList;
	CMap<uint32, uint32, DWORD, DWORD> m_bannedList;
	CMap<uint32, uint32, CDeletedClient*, CDeletedClient*> m_trackedClientsList;
	CUpDownClient *m_pBuddy;
	CList<IPANDTICS> listFirewallCheckRequests;
	CList<IPANDTICS> listDirectCallbackRequests;
	CList<CONNECTINGCLIENT> m_liConnectingClients;
	DWORD	m_dwLastBanCleanUp;
	DWORD	m_dwLastTrackedCleanUp;
	DWORD	m_dwLastClientCleanUp;
	uint8	m_nBuddyStatus;
};