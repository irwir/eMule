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
#include "Kademlia/Utils/KadClientSearcher.h"

class CUpDownClient;
class CFileDataIO;
class CFriendConnectionListener;

enum EFriendConnectState
{
	FCS_NONE = 0,
	FCS_CONNECTING,
	FCS_AUTH,
	FCS_KADSEARCHING,
};

enum EFriendConnectReport
{
	FCR_ESTABLISHED = 0,
	FCR_DISCONNECTED,
	FCR_USERHASHVERIFIED,
	FCR_USERHASHFAILED,
	FCR_SECUREIDENTFAILED,
	FCR_DELETED
};

#define	FF_NAME		0x01
#define	FF_KADID	0x02

///////////////////////////////////////////////////////////////////////////////
// CFriendConnectionListener

class CFriendConnectionListener
{
public:
	virtual void	ReportConnectionProgress(CUpDownClient *pClient, const CString &strProgressDesc, bool bNoTimeStamp) = 0;
	virtual void	ConnectingResult(CUpDownClient *pClient, bool bSuccess) = 0;
	virtual void	ClientObjectChanged(CUpDownClient *pOldClient, CUpDownClient *pNewClient) = 0;
protected:
	virtual	~CFriendConnectionListener() = default; //just in case...
};

///////////////////////////////////////////////////////////////////////////////
// CFriend
class CFriend : public Kademlia::CKadClientSearcher
{
public:
	CFriend();
	explicit CFriend(CUpDownClient *client);
	CFriend(const uchar *abyUserhash, time_t dwLastSeen, uint32 dwLastUsedIP, uint16 nLastUsedPort
			, uint32 dwLastChatted, LPCTSTR pszName, uint32 dwHasHash);
	~CFriend();

	uchar	m_abyUserhash[16];

	time_t	m_dwLastSeen;
	time_t	m_dwLastChatted;
	CString m_strName;
	uint32	m_dwLastUsedIP;
	uint16	m_nLastUsedPort;

	CUpDownClient*	GetClientForChatSession();
	CUpDownClient*	GetLinkedClient(bool bValidCheck = false) const;
	void			SetLinkedClient(CUpDownClient *linkedClient);

	void	LoadFromFile(CFileDataIO *file);
	void	WriteToFile(CFileDataIO *file);

	bool	TryToConnect(CFriendConnectionListener *pConnectionReport);
	void	UpdateFriendConnectionState(EFriendConnectReport eEvent);
	bool	IsTryingToConnect() const	{ return m_FriendConnectState != FCS_NONE; }
	//bool	CancelTryToConnect(CFriendConnectionListener *pConnectionReport);
	void	FindKadID();
	void	KadSearchNodeIDByIPResult(Kademlia::EKadClientSearchRes eStatus, const uchar *pachNodeID);
	void	KadSearchIPByNodeIDResult(Kademlia::EKadClientSearchRes eStatus, uint32 dwIP, uint16 nPort);

	//void	SendMsg(const CString &strMessage);

	void	SetFriendSlot(bool newValue);
	bool	GetFriendSlot() const;

	bool	HasUserhash() const;
	bool	HasKadID() const;

private:
	uchar	m_abyKadID[16];
	CTypedPtrList<CPtrList, CFriendConnectionListener*> m_liConnectionReport;
	CUpDownClient *m_LinkedClient;
	EFriendConnectState	m_FriendConnectState;
	uint32	m_dwLastKadSearch;
	bool	m_friendSlot;

	void	init();
};