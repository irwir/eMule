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
#include "kademlia/utils/KadUDPKey.h"
#include "kademlia/utils/UInt128.h"

namespace Kademlia
{
	class CContact
	{
		friend class CRoutingBin;
		friend class CRoutingZone;

	public:
		~CContact();
		CContact();
		CContact(const CUInt128 &uClientID, uint32 uIp, uint16 uUdpPort, uint16 uTcpPort, uint8 uVersion, const CKadUDPKey &cUDPKey, bool bIPVerified);
		CContact(const CUInt128 &uClientID, uint32 uIp, uint16 uUdpPort, uint16 uTcpPort, const CUInt128 &uTarget, uint8 uVersion, const CKadUDPKey &cUDPKey, bool bIPVerified);
		CContact(const CContact &k1)				{ Copy(k1); }
		CContact& operator=(const CContact &k1)		{ Copy(k1); return *this; }

		void	GetClientID(CUInt128 *puId) const;
		CUInt128 GetClientID() const;
		void	GetClientID(CString *psId) const;
		void	SetClientID(const CUInt128 &uClientID);
		void	GetDistance(CUInt128 *puDistance) const;
		void	GetDistance(CString *psDistance) const;
		CUInt128 GetDistance() const;
		uint32	GetIPAddress() const;
		uint32	GetNetIP() const;
		void	GetIPAddress(CString *psIp) const;
		void	SetIPAddress(uint32 uIp);
		uint16	GetTCPPort() const;
		void	GetTCPPort(CString *psPort) const;
		void	SetTCPPort(uint16 uPort);
		uint16	GetUDPPort() const;
		void	GetUDPPort(CString *psPort) const;
		void	SetUDPPort(uint16 uPort);
		byte	GetType() const;
		void	UpdateType();
		void	CheckingType();
		bool	GetGuiRefs() const;
		void	SetGuiRefs(bool bRefs);
		bool	InUse() const;
		void	IncUse();
		void	DecUse();
		uint8	GetVersion() const;
		void	SetVersion(uint8 uVersion);
		time_t	GetCreatedTime() const;
		time_t	GetExpireTime() const;
		time_t	GetLastTypeSet() const;
		time_t	GetLastSeen() const;
		void	Expire();

		bool	GetReceivedHelloPacket() const		{ return m_bReceivedHelloPacket; }
		void	SetReceivedHelloPacket()			{ m_bReceivedHelloPacket = true; }

		CKadUDPKey	GetUDPKey() const;
		void	SetUDPKey(const CKadUDPKey &cUDPKey);
		bool	IsIpVerified() const;
		void	SetIpVerified(bool bIPVerified);

		// bootstrapcontact functions only used for GUI
		bool	IsBootstrapContact() const			{ return m_bBootstrapContact; }
		void	SetBootstrapContact()				{ m_bBootstrapContact = true; }

	private:
		void	InitContact(); // Common var initialization goes here
		void	Copy(const CContact &fromContact);

		CUInt128	m_uClientID;
		CUInt128	m_uDistance;
		CKadUDPKey	m_cUDPKey;
		time_t	m_tLastTypeSet;
		time_t	m_tExpires;
		time_t	m_tCreated;
		uint32	m_uInUse;
		uint32	m_uIp; //host byte order
		uint32	m_uNetIp; //network byte order
		uint16	m_uTcpPort;
		uint16	m_uUdpPort;
		uint8	m_uVersion;
		byte	m_byType;
		bool	m_bGuiRefs;
		bool	m_bIPVerified;
		bool	m_bReceivedHelloPacket;
		bool	m_bBootstrapContact;
	};
}