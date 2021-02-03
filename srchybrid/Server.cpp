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
#include "stdafx.h"
#include "emule.h"
#include "Server.h"
#include "Opcodes.h"
#include "OtherFunctions.h"
#include "Packets.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

void CServer::init()
{
	_tcscpy(ipfull, ipstr(ip));
	lastpingedtime = 0;
	m_RealLastPingedTime = 0;
	challenge = 0;
	m_uDescReqChallenge = 0;
	lastpinged = 0;
	lastdescpingedcout = 0;
	files = 0;
	users = 0;
	maxusers = 0;
	softfiles = 0;
	hardfiles = 0;
	m_uPreference = 0;
	ping = 0;
	failedcount = 0;
	m_uTCPFlags = 0;
	m_uUDPFlags = 0;
	m_uLowIDUsers = 0;
	m_dwServerKeyUDP = 0;
	m_dwIPServerKeyUDP = 0;
	m_nObfuscationPortTCP = 0;
	m_nObfuscationPortUDP = 0;
	m_bstaticservermember = false;
	m_bCryptPingReplyPending = false;
	m_bTriedCryptOnce = false;
}

CServer::CServer(const ServerMet_Struct *in_data)
	: ip(in_data->ip)
	, port(in_data->port)
{
	init();
}

CServer::CServer(uint16 in_port, LPCTSTR pszAddr)
	: port(in_port)
{
	ip = inet_addr(CStringA(pszAddr));
	if (ip == INADDR_NONE && _tcscmp(pszAddr, _T("255.255.255.255")) != 0) {
		m_strDynIP = pszAddr;
		ip = 0;
	}
	init();
}

CServer::CServer(const CServer *pOld)
{
	_tcscpy(ipfull, pOld->ipfull);
	m_strDescription = pOld->m_strDescription;
	m_strName = pOld->m_strName;
	m_strDynIP = pOld->m_strDynIP;
	m_strVersion = pOld->m_strVersion;
	lastpingedtime = pOld->lastpingedtime;
	m_RealLastPingedTime = pOld->m_RealLastPingedTime;
	challenge = pOld->challenge;
	m_uDescReqChallenge = pOld->m_uDescReqChallenge;
	lastpinged = pOld->lastpinged;
	lastdescpingedcout = pOld->lastdescpingedcout;
	files = pOld->files;
	users = pOld->users;
	maxusers = pOld->maxusers;
	softfiles = pOld->softfiles;
	hardfiles = pOld->hardfiles;
	m_uPreference = pOld->m_uPreference;
	ping = pOld->ping;
	failedcount = pOld->failedcount;
	m_uTCPFlags = pOld->m_uTCPFlags;
	m_uUDPFlags = pOld->m_uUDPFlags;
	m_uLowIDUsers = pOld->m_uLowIDUsers;
	m_dwServerKeyUDP = pOld->m_dwServerKeyUDP;
	m_dwIPServerKeyUDP = pOld->m_dwIPServerKeyUDP;
	m_nObfuscationPortTCP = pOld->m_nObfuscationPortTCP;
	m_nObfuscationPortUDP = pOld->m_nObfuscationPortUDP;
	ip = pOld->ip;
	port = pOld->port;
	m_bstaticservermember = pOld->IsStaticMember();
	m_bCryptPingReplyPending = pOld->m_bCryptPingReplyPending;
	m_bTriedCryptOnce = pOld->m_bTriedCryptOnce;
}

bool CServer::AddTagFromFile(CFileDataIO *servermet)
{
	CTag *tag = new CTag(servermet, false);
	switch (tag->GetNameID()) {
	case ST_SERVERNAME:
		if (tag->IsStr()) {
			if (m_strName.IsEmpty())
				m_strName = tag->GetStr();
		} else
			ASSERT(0);
		break;
	case ST_DESCRIPTION:
		if (tag->IsStr()) {
			if (m_strDescription.IsEmpty())
				m_strDescription = tag->GetStr();
		} else
			ASSERT(0);
		break;
	case ST_PING:
		if (tag->IsInt())
			ping = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_FAIL:
		if (tag->IsInt())
			failedcount = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_PREFERENCE:
		if (tag->IsInt())
			m_uPreference = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_DYNIP:
		if (tag->IsStr()) {
			if (!tag->GetStr().IsEmpty() && m_strDynIP.IsEmpty()) {
				// set dynIP and reset available (outdated) IP
				SetDynIP(tag->GetStr());
				SetIP(0);
			}
		} else
			ASSERT(0);
		break;
	case ST_MAXUSERS:
		if (tag->IsInt())
			maxusers = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_SOFTFILES:
		if (tag->IsInt())
			softfiles = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_HARDFILES:
		if (tag->IsInt())
			hardfiles = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_LASTPING:
		if (tag->IsInt())
			lastpingedtime = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_VERSION:
		if (tag->IsStr()) {
			if (m_strVersion.IsEmpty())
				m_strVersion = tag->GetStr();
		} else if (tag->IsInt())
			m_strVersion.Format(_T("%u.%02u"), tag->GetInt() >> 16, tag->GetInt() & 0xffff);
		else
			ASSERT(0);
		break;
	case ST_UDPFLAGS:
		if (tag->IsInt())
			m_uUDPFlags = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_LOWIDUSERS:
		if (tag->IsInt())
			m_uLowIDUsers = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_PORT:
	case ST_IP:
		ASSERT(tag->IsInt());
		break;
	case ST_UDPKEY:
		if (tag->IsInt())
			m_dwServerKeyUDP = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_UDPKEYIP:
		if (tag->IsInt())
			m_dwIPServerKeyUDP = tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_TCPPORTOBFUSCATION:
		if (tag->IsInt())
			m_nObfuscationPortTCP = (uint16)tag->GetInt();
		else
			ASSERT(0);
		break;
	case ST_UDPPORTOBFUSCATION:
		if (tag->IsInt())
			m_nObfuscationPortUDP = (uint16)tag->GetInt();
		else
			ASSERT(0);
		break;
	default:
		if (tag->GetNameID() == 0 && CmpED2KTagName(tag->GetName(), "files") == 0) {
			if (tag->IsInt())
				files = tag->GetInt();
			else
				ASSERT(0);
		} else if (tag->GetNameID() == 0 && CmpED2KTagName(tag->GetName(), "users") == 0) {
			if (tag->IsInt())
				users = tag->GetInt();
			else
				ASSERT(0);
		} else
			TRACE(_T("***Unknown tag in server.met: %s\n"), (LPCTSTR)tag->GetFullInfo());
	}
	delete tag;
	return true;
}

LPCTSTR CServer::GetAddress() const
{
	return m_strDynIP.IsEmpty() ? ipfull : m_strDynIP;
}

void CServer::SetIP(uint32 newip)
{
	ip = newip;
	_tcscpy(ipfull, ipstr(ip));
}

void CServer::SetLastDescPingedCount(bool bReset)
{
	if (bReset)
		lastdescpingedcout = 0;
	else
		++lastdescpingedcout;
}
/* unused method
bool CServer::IsEqual(const CServer *pServer) const
{
	if (GetPort() != pServer->GetPort())
		return false;
	if (HasDynIP() && pServer->HasDynIP())
		return (GetDynIP().CompareNoCase(pServer->GetDynIP()) == 0);
	if (HasDynIP() || pServer->HasDynIP())
		return false;
	return (GetIP() == pServer->GetIP());
}*/

uint32 CServer::GetServerKeyUDP(bool bForce) const
{
	if (m_dwIPServerKeyUDP != 0 && m_dwIPServerKeyUDP == theApp.GetPublicIP() || bForce)
		return m_dwServerKeyUDP;
	return 0;
}

void CServer::SetServerKeyUDP(uint32 dwServerKeyUDP)
{
	ASSERT((theApp.GetPublicIP() != 0 || dwServerKeyUDP == 0) || theApp.IsFirewalled());
	m_dwServerKeyUDP = dwServerKeyUDP;
	m_dwIPServerKeyUDP = theApp.GetPublicIP();
}