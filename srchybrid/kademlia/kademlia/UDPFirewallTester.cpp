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
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/prefs.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/routing/RoutingZone.h"
#include "Log.h"
#include "emule.h"
#include "emuledlg.h"
#include "opcodes.h"
#include "clientlist.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

#define UDP_FIREWALLTEST_CLIENTSTOASK	2	// more clients increase the chance of a false positive, while less the chance of a false negative

bool CUDPFirewallTester::m_bFirewalledUDP = false;
bool CUDPFirewallTester::m_bFirewalledLastStateUDP = false;
bool CUDPFirewallTester::m_bIsFWVerifiedUDP = false;
bool CUDPFirewallTester::m_bNodeSearchStarted = false;
bool CUDPFirewallTester::m_bTimedOut = false;
uint8 CUDPFirewallTester::m_byFWChecksRunningUDP = 0;
uint8 CUDPFirewallTester::m_byFWChecksFinishedUDP = 0;
uint32 CUDPFirewallTester::m_dwTestStart = 0;
uint32 CUDPFirewallTester::m_dwLastSucceededTime = 0;
CList<CContact> CUDPFirewallTester::m_liPossibleTestClients;
CList<UsedClient_Struct> CUDPFirewallTester::m_liUsedTestClients;

bool CUDPFirewallTester::IsFirewalledUDP(bool bLastStateIfTesting)
{
	if (CKademlia::IsRunningInLANMode())
		return false;
	bool bCheckingFW = IsFWCheckUDPRunning();
	if (m_bTimedOut) {
		if (bCheckingFW)
			return true; // firewalled state by timeout
		ASSERT(0);
	} else if (bCheckingFW) {
		//For now we don't allow to get firewalled by timeouts if we have succeeded a test before; might be changed later
		if (!m_bFirewalledUDP && CKademlia::IsFirewalled() && m_dwTestStart != 0
			&& !m_bIsFWVerifiedUDP && ::GetTickCount() >= m_dwTestStart + MIN2MS(6))
		{
			DebugLogWarning(_T("Firewall UDP Tester: Timeout: Setting UDP status to firewalled after being unable to get results for 6 minutes"));
			m_bTimedOut = true;
			theApp.emuledlg->ShowConnectionState();
		}
	}
	return (bLastStateIfTesting && bCheckingFW) ? m_bFirewalledLastStateUDP : m_bFirewalledUDP;
}

bool CUDPFirewallTester::GetUDPCheckClientsNeeded()
{ // are we in search for testclients
	return (m_byFWChecksRunningUDP + m_byFWChecksFinishedUDP) < UDP_FIREWALLTEST_CLIENTSTOASK;
}

void CUDPFirewallTester::SetUDPFWCheckResult(bool bSucceeded, bool bTestCancelled, uint32 uFromIP, uint16 nIncomingPort)
{
	if (!CKademlia::IsRunning())
		return;
	// see if we actually requested a firewall check from this client
	bool bRequested = false;
	for (POSITION pos = m_liUsedTestClients.GetHeadPosition(); pos != NULL;) {
		UsedClient_Struct &ucs = m_liUsedTestClients.GetNext(pos);
		if (ucs.contact.GetIPAddress() == uFromIP) {

			if (!IsFWCheckUDPRunning() && !m_bFirewalledUDP && m_bIsFWVerifiedUDP && ::GetTickCount() < m_dwLastSucceededTime + SEC2MS(10)
				&& nIncomingPort == CKademlia::GetPrefs()->GetInternKadPort() && CKademlia::GetPrefs()->GetUseExternKadPort())
			{
				// our test finished already in the last 10 seconds with being open because we received a proper result packet before
				// however we now receive another answer packet on our incoming port (which is not unusual as both result packets are sent
				// nearly at the same time and UDP doesn't cares if the order stays), while the one before was received on our extern port
				// Because a proper forwarded intern port is more reliable to stay open, than an extern port set by the NAT, we prefer
				// intern ports and change the setting.
				CKademlia::GetPrefs()->SetUseExternKadPort(false);
				DebugLog(_T("Corrected UDP firewall result: Using open internal (%u) instead open external port"), nIncomingPort);
				theApp.emuledlg->ShowConnectionState();
				return;
			}
			if (ucs.bAnswered) {
				// we already received an answer. This may happen since all tests contain of two answer packets,
				// but the answer could also be too late and we already counted it as failure.
				return;
			}
			ucs.bAnswered = true;
			bRequested = true;
			break;
		}
	}
	if (!bRequested) {
		DebugLogWarning(_T("Unrequested UDPFWCheckResult from %s"), (LPCTSTR)ipstr(htonl(uFromIP)));
		return;
	}

	if (!IsFWCheckUDPRunning()) // it's over already
		return;

	if (m_byFWChecksRunningUDP == 0)
		ASSERT(0);
	else
		--m_byFWChecksRunningUDP;

	if (!bTestCancelled) {
		++m_byFWChecksFinishedUDP;
		if (bSucceeded) {	//one positive result is enough
			m_dwTestStart = 0;
			m_bFirewalledUDP = false;
			m_bIsFWVerifiedUDP = true;
			m_bTimedOut = false;
			m_byFWChecksFinishedUDP = UDP_FIREWALLTEST_CLIENTSTOASK; // don't do any more tests
			m_byFWChecksRunningUDP = 0; // all other tests are cancelled
			m_liPossibleTestClients.RemoveAll(); // clear list, keep used clients list through
			m_dwLastSucceededTime = ::GetTickCount(); // for delayed results, see above
			CSearchManager::CancelNodeFWCheckUDPSearch(); // cancel firewall node searches if any are still active
			// if this packet came to our internal port, explicitly set the internal port as used port from now on
			if (nIncomingPort == CKademlia::GetPrefs()->GetInternKadPort()) {
				CKademlia::GetPrefs()->SetUseExternKadPort(false);
				DebugLog(_T("New KAD Firewall state (UDP): Open, using intern port"));
			} else if (nIncomingPort == CKademlia::GetPrefs()->GetExternalKadPort() && nIncomingPort != 0) {
				CKademlia::GetPrefs()->SetUseExternKadPort(true);
				DebugLog(_T("New KAD Firewall state (UDP): Open, using extern port"));
			}
			theApp.emuledlg->ShowConnectionState();

			return;
		}
		if (m_byFWChecksFinishedUDP >= UDP_FIREWALLTEST_CLIENTSTOASK) {
			// seems we are firewalled
			m_dwTestStart = 0;
			DebugLog(_T("New KAD Firewall state (UDP): Firewalled"));
			m_bFirewalledUDP = true;
			m_bIsFWVerifiedUDP = true;
			m_bTimedOut = false;
			theApp.emuledlg->ShowConnectionState();
			m_liPossibleTestClients.RemoveAll(); // clear list, keep used clients list through
			CSearchManager::CancelNodeFWCheckUDPSearch(); // cancel firewall node searches if any are still active
			return;
		}
		DebugLogWarning(_T("Kad UDP firewall check from %s result: Firewalled, continue testing"), (LPCTSTR)ipstr(htonl(uFromIP)));
	} else
		DebugLogWarning(_T("Kad UDP firewall check from %s cancelled"), (LPCTSTR)ipstr(htonl(uFromIP)));
	QueryNextClient();
}

void CUDPFirewallTester::ReCheckFirewallUDP(bool bSetUnverified)
{
	ASSERT(m_byFWChecksRunningUDP == 0);
	m_byFWChecksRunningUDP = 0;
	m_byFWChecksFinishedUDP = 0;
	m_dwLastSucceededTime = 0;
	m_dwTestStart = ::GetTickCount();
	m_bTimedOut = false;
	m_bFirewalledLastStateUDP = m_bFirewalledUDP;
	m_bIsFWVerifiedUDP = (m_bIsFWVerifiedUDP && !bSetUnverified);
	CSearchManager::FindNodeFWCheckUDP(); // start a lookup for a random node to find suitable IPs
	m_bNodeSearchStarted = true;
	CKademlia::GetPrefs()->FindExternKadPort(true);
}

void CUDPFirewallTester::Connected()
{
	if (!m_bNodeSearchStarted && IsFWCheckUDPRunning()) {
		CSearchManager::FindNodeFWCheckUDP(); // start a lookup for a random node to find suitable IPs
		m_bNodeSearchStarted = true;
		m_dwTestStart = ::GetTickCount();
		m_bTimedOut = false;
	}
}

bool CUDPFirewallTester::IsFWCheckUDPRunning()
{
	return m_byFWChecksFinishedUDP < UDP_FIREWALLTEST_CLIENTSTOASK && !CKademlia::IsRunningInLANMode();
}

void CUDPFirewallTester::Reset()
{
	m_bFirewalledUDP = false;
	m_bFirewalledLastStateUDP = false;
	m_bIsFWVerifiedUDP = false;
	m_bNodeSearchStarted = false;
	m_bTimedOut = false;
	m_byFWChecksRunningUDP = 0;
	m_byFWChecksFinishedUDP = 0;
	m_dwTestStart = 0;
	m_dwLastSucceededTime = 0;
	CSearchManager::CancelNodeFWCheckUDPSearch(); // cancel firewall node searches if any are still active
	m_liPossibleTestClients.RemoveAll();
	CKademlia::GetPrefs()->SetUseExternKadPort(true);
	// keep the list of used clients
}

void CUDPFirewallTester::AddPossibleTestContact(const CUInt128 &uClientID, uint32 uIp, uint16 uUdpPort, uint16 uTcpPort, const CUInt128 &uTarget, uint8 uVersion, const CKadUDPKey &cUDPKey, bool bIPVerified)
{
	if (!IsFWCheckUDPRunning())
		return;
	// add the possible contact to our list - no checks in advance
	m_liPossibleTestClients.AddHead(CContact(uClientID, uIp, uUdpPort, uTcpPort, uTarget, uVersion, cUDPKey, bIPVerified));
	QueryNextClient();
}

void CUDPFirewallTester::QueryNextClient() // try the next available client for the firewall check
{
	if (!IsFWCheckUDPRunning() || !GetUDPCheckClientsNeeded() || CKademlia::GetPrefs()->FindExternKadPort(false))
		return; // check if more tests are needed and wait till we know our extern port

	if (!CKademlia::IsRunning() || CKademlia::GetRoutingZone() == NULL) {
		ASSERT(0);
		return;
	}

	while (!m_liPossibleTestClients.IsEmpty()) {
		CContact curContact = m_liPossibleTestClients.RemoveHead();
		// udp firewall checks are not supported by clients with kad version < 6
		if (curContact.GetVersion() <= KADEMLIA_VERSION5_48a)
			continue;

		// sanitize - do not test ourself
		if (htonl(curContact.GetIPAddress()) == theApp.GetPublicIP() || curContact.GetClientID().CompareTo(CKademlia::GetPrefs()->GetKadID()) == 0)
			continue;

		// check if we actually requested a firewall check from this client at some point
		bool bAlreadyRequested = false;
		for (POSITION pos = m_liUsedTestClients.GetHeadPosition(); pos != NULL;)
			if (m_liUsedTestClients.GetNext(pos).contact.GetIPAddress() == curContact.GetIPAddress()) {
				bAlreadyRequested = true;
				break;
			}

		// check if we know itsIP already from kademlia - we need an IP which was never used for UDP yet
		if (!bAlreadyRequested && CKademlia::GetRoutingZone()->GetContact(curContact.GetIPAddress(), 0, false) == NULL)
			// OK, tell the client list to do the same search and start the check if OK
			if (theApp.clientlist->DoRequestFirewallCheckUDP(curContact)) {
				UsedClient_Struct sAdd = {curContact, false};
				m_liUsedTestClients.AddHead(sAdd);
				++m_byFWChecksRunningUDP;
				break;
			}
	}
}

bool CUDPFirewallTester::IsVerified()
{
	return m_bIsFWVerifiedUDP || CKademlia::IsRunningInLANMode();
}