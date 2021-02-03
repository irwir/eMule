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

#include "stdafx.h"
#include "opcodes.h"
#include "preferences.h"
#include "emule.h"
#include "emuledlg.h"
#include "SafeFile.h"
#include "serverlist.h"
#include "Log.h"
#include "MD5Sum.h"
#include "OtherFunctions.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/kademlia/kademlia.h"
#include "kademlia/kademlia/indexed.h"
#include "kademlia/routing/RoutingZone.h"
#include "kademlia/utils/MiscUtils.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

CPrefs::CPrefs()
{
	Init(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("preferencesKad.dat"));
}

CPrefs::~CPrefs()
{
	if (!m_sFilename.IsEmpty())
		WriteFile();
}

void CPrefs::Init(LPCTSTR szFilename)
{
	m_uClientID.SetValueRandom();
	m_tLastContact = 0;
	m_uRecheckip = 0;
	m_uFirewalled = 0;
	m_uTotalFile = 0;
	m_uTotalStoreSrc = 0;
	m_uTotalStoreKey = 0;
	m_uTotalSource = 0;
	m_uTotalNotes = 0;
	m_uTotalStoreNotes = 0;
	m_bPublish = false;
	m_uClientHash.SetValue(CUInt128(thePrefs.GetUserHash()));
	m_uIP = 0;
	m_uIPLast = 0;
	m_bFindBuddy = false;
	m_uKademliaUsers = 0;
	m_uKademliaFiles = 0;
	m_sFilename = szFilename;
	m_bLastFirewallState = true;
	m_nExternKadPort = 0;
	m_bUseExternKadPort = true;
	m_nStatsUDPOpenNodes = 0;
	m_nStatsUDPFirewalledNodes = 0;
	m_nStatsTCPOpenNodes = 0;
	m_nStatsTCPFirewalledNodes = 0;
	m_nStatsKadV8LastChecked = 0;
	m_fKadV8Ratio = 0;
	ReadFile();
}

void CPrefs::ReadFile()
{
	try {
		CSafeBufferedFile file;
		CFileException fexp;
		if (file.Open(m_sFilename, CFile::modeRead | CFile::osSequentialScan | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
			setvbuf(file.m_pStream, NULL, _IOFBF, 16384);
			m_uIP = file.ReadUInt32();
			file.ReadUInt16();
			file.ReadUInt128(&m_uClientID);
			// get rid of invalid kad IDs which may have been stored by older versions
			if (m_uClientID == 0)
				m_uClientID.SetValueRandom();
			file.Close();
		}
	} catch (CException *ex) {
		ASSERT(0);
		ex->Delete();
	} catch (...) {
		TRACE("Exception in CPrefs::readFile\n");
	}
}

void CPrefs::WriteFile()
{
	try {
		CSafeBufferedFile file;
		CFileException fexp;
		if (file.Open(m_sFilename, CFile::modeWrite | CFile::modeCreate | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
			setvbuf(file.m_pStream, NULL, _IOFBF, 16384);
			file.WriteUInt32(m_uIP);
			file.WriteUInt16(0); //This is no longer used.
			file.WriteUInt128(&m_uClientID);
			file.WriteUInt8(0); //This is to tell older clients there are no tags.
			file.Close();
		}
	} catch (CException *ex) {
		ASSERT(0);
		ex->Delete();
	} catch (...) {
		TRACE("Exception in CPrefs::writeFile\n");
	}
}

void CPrefs::SetIPAddress(uint32 uVal)
{
	//This is our first check on connect, init our IP.
	if (!uVal || !m_uIPLast)
		m_uIPLast = uVal;
	//If the last check matches this one, reset our current IP.
	//If the last check does not match, wait for our next incoming IP.
	//This happens for two reasons. We just changed our IP, or a client responded with a bad IP.
	if (uVal == m_uIPLast)
		m_uIP = uVal;
	else
		m_uIPLast = uVal;
}


bool CPrefs::HasLostConnection() const
{
	return m_tLastContact && (time(NULL) >= m_tLastContact + KADEMLIADISCONNECTDELAY);
}

bool CPrefs::HasHadContact() const
{
	return m_tLastContact && (time(NULL) < m_tLastContact + KADEMLIADISCONNECTDELAY);
}

bool CPrefs::GetFirewalled() const
{
	if (m_uFirewalled < 2)
		//Not enough people have told us we are open but we may be doing a recheck
		//at the moment which will give a false lowID. Therefore we check to see
		//if we are still rechecking and will report our last known state.
		return !GetRecheckIP() || m_bLastFirewallState;

	//We had enough tell us we are not firewalled.
	return false;
}
void CPrefs::SetFirewalled()
{
	//We are checking our firewall state. Let keep a snapshot of our
	//current state to prevent false reports during the recheck.
	m_bLastFirewallState = (m_uFirewalled < 2);
	m_uFirewalled = 0;
	theApp.emuledlg->ShowConnectionState();
}

void CPrefs::IncFirewalled()
{
	++m_uFirewalled;
	theApp.emuledlg->ShowConnectionState();
}

bool CPrefs::GetFindBuddy()
{
	if (m_bFindBuddy) {
		m_bFindBuddy = false;
		return true;
	}
	return false;
}

void CPrefs::SetKademliaFiles()
{
	//There is no real way to know how many files are in the Kad network.
	//So we first try to see how many files per user are in the ED2K network.
	//If that fails, we use a set value based on previous tests.
	uint32 nServerAverage = 0;
	theApp.serverlist->GetAvgFile(nServerAverage);
	uint32 nKadAverage = Kademlia::CKademlia::GetIndexed()->GetFileKeyCount();

#ifdef _DEBUG
	CString method;
#endif
	if (nServerAverage > nKadAverage) {
#ifdef _DEBUG
		method.Format(_T("Kad file estimate used Server avg(%u)"), nServerAverage);
#endif
		nKadAverage = nServerAverage;
	}
#ifdef _DEBUG
	else
		method.Format(_T("Kad file estimate used Kad avg(%u)"), nKadAverage);
#endif
	if (nKadAverage < 108) {
#ifdef _DEBUG
		method.Format(_T("Kad file estimate used default avg(108)"));
#endif
		nKadAverage = 108;
	}
#ifdef _DEBUG
	AddDebugLogLine(DLP_VERYLOW, false, method);
#endif
	m_uKademliaFiles = nKadAverage * m_uKademliaUsers;
}

void CPrefs::GetKadID(CUInt128 *puID) const
{
	puID->SetValue(m_uClientID);
}

void CPrefs::GetKadID(CString *psID) const
{
	m_uClientID.ToHexString(psID);
}

void CPrefs::SetKadID(const CUInt128 &puID)
{
	m_uClientID = puID;
}

CUInt128 CPrefs::GetKadID() const
{
	return m_uClientID;
}

void CPrefs::GetClientHash(CUInt128 *puID) const
{
	puID->SetValue(m_uClientHash);
}

void CPrefs::GetClientHash(CString *psID) const
{
	m_uClientHash.ToHexString(psID);
}

void CPrefs::SetClientHash(const CUInt128 &puID)
{
	m_uClientHash = puID;
}

CUInt128 CPrefs::GetClientHash() const
{
	return m_uClientHash;
}

uint32 CPrefs::GetIPAddress() const
{
	return m_uIP;
}

bool CPrefs::GetRecheckIP() const
{
	return (m_uRecheckip < KADEMLIAFIREWALLCHECKS);
}

void CPrefs::SetRecheckIP()
{
	m_uRecheckip = 0;
	SetFirewalled();
}

void CPrefs::IncRecheckIP()
{
	++m_uRecheckip;
}

void CPrefs::SetLastContact()
{
	m_tLastContact = time(NULL);
}

time_t CPrefs::GetLastContact() const
{
	return m_tLastContact;
}

uint8 CPrefs::GetTotalFile() const
{
	return m_uTotalFile;
}

void CPrefs::SetTotalFile(uint8 uVal)
{
	m_uTotalFile = uVal;
}

uint8 CPrefs::GetTotalStoreSrc() const
{
	return m_uTotalStoreSrc;
}

void CPrefs::SetTotalStoreSrc(uint8 uVal)
{
	m_uTotalStoreSrc = uVal;
}

uint8 CPrefs::GetTotalStoreKey() const
{
	return m_uTotalStoreKey;
}

void CPrefs::SetTotalStoreKey(uint8 uVal)
{
	m_uTotalStoreKey = uVal;
}

uint8 CPrefs::GetTotalSource() const
{
	return m_uTotalSource;
}

void CPrefs::SetTotalSource(uint8 uVal)
{
	m_uTotalSource = uVal;
}

uint8 CPrefs::GetTotalNotes() const
{
	return m_uTotalNotes;
}

void CPrefs::SetTotalNotes(uint8 uVal)
{
	m_uTotalNotes = uVal;
}

uint8 CPrefs::GetTotalStoreNotes() const
{
	return m_uTotalStoreNotes;
}

void CPrefs::SetTotalStoreNotes(uint8 uVal)
{
	m_uTotalStoreNotes = uVal;
}

uint32 CPrefs::GetKademliaUsers() const
{
	return m_uKademliaUsers;
}

void CPrefs::SetKademliaUsers(uint32 uVal)
{
	m_uKademliaUsers = uVal;
}

uint32 CPrefs::GetKademliaFiles() const
{
	return m_uKademliaFiles;
}

bool CPrefs::GetPublish() const
{
	return m_bPublish;
}

void CPrefs::SetPublish(bool bVal)
{
	m_bPublish = bVal;
}

void CPrefs::SetFindBuddy(bool bVal)
{
	m_bFindBuddy = bVal;
}

uint32 CPrefs::GetUDPVerifyKey(uint32 dwTargetIP)
{
	uint64 ui64Buffer = thePrefs.GetKadUDPKey();
	ui64Buffer <<= 32;
	ui64Buffer |= dwTargetIP;
	MD5Sum md5((uchar*)&ui64Buffer, 8);
	return ((uint32)(PeekUInt32(md5.GetRawHash() + 0) ^ PeekUInt32(md5.GetRawHash() + 4) ^ PeekUInt32(md5.GetRawHash() + 8) ^ PeekUInt32(md5.GetRawHash() + 12)) % 0xFFFFFFFE) + 1;
}

bool CPrefs::GetUseExternKadPort() const
{
	return m_bUseExternKadPort && !Kademlia::CKademlia::IsRunningInLANMode();
}

void CPrefs::SetUseExternKadPort(bool bVal)
{
	m_bUseExternKadPort = bVal;
}

uint16 CPrefs::GetExternalKadPort() const
{
	return m_nExternKadPort;
}

#define EXTERNAL_PORT_ASKIPS	3
void CPrefs::SetExternKadPort(uint16 uVal, uint32 uFromIP)
{
	if (FindExternKadPort(false)) {
		for (INT_PTR i = m_anExternPortIPs.GetCount(); --i >= 0;)
			if (m_anExternPortIPs[i] == uFromIP)
				return;

		m_anExternPortIPs.Add(uFromIP);
		DebugLog(_T("Received possible external Kad Port %u from %s"), uVal, (LPCTSTR)ipstr(htonl(uFromIP)));
		// if 2 out of 3 tries result in the same external port its fine, otherwise consider it as unreliable
		for (INT_PTR i = m_anExternPorts.GetCount(); --i >= 0;) {
			if (m_anExternPorts[i] == uVal) {
				m_nExternKadPort = uVal;
				DebugLog(_T("Set external Kad Port to %u"), uVal);
				while (m_anExternPortIPs.GetCount() < EXTERNAL_PORT_ASKIPS)
					m_anExternPortIPs.Add(0); // add empty entries so we know the check has finished even if we asked less than max IPs
				return;
			}
		}
		m_anExternPorts.Add(uVal);
		if (!FindExternKadPort(false)) {
			DebugLog(_T("Our external port seems unreliable, not using it for firewall checks"), uVal);
			m_nExternKadPort = 0;
		}
	}
}

uint16 CPrefs::GetInternKadPort()
{
	return thePrefs.GetUDPPort();
}

bool CPrefs::FindExternKadPort(bool bReset)
{
	if (!bReset)
		return  m_anExternPortIPs.GetCount() < EXTERNAL_PORT_ASKIPS && !Kademlia::CKademlia::IsRunningInLANMode();
	m_anExternPortIPs.RemoveAll();
	m_anExternPorts.RemoveAll();
	return true;
}

uint8 CPrefs::GetMyConnectOptions(bool bEncryption, bool bCallback)
{
	return ::GetMyConnectOptions(bEncryption, bCallback);
}

float CPrefs::StatsGetFirewalledRatio(bool bUDP) const
{
	// gives an estimated percentage of TCP firewalled clients in the network
	// will only work if enough >0.49b nodes have spread and only if we are not UDP firewalled ourself
	if (bUDP) {
		if (m_nStatsUDPFirewalledNodes > 0 && m_nStatsUDPOpenNodes > 10)
			return m_nStatsUDPFirewalledNodes / (float)(m_nStatsUDPFirewalledNodes + m_nStatsUDPOpenNodes);
		return 0;
	}
	if (m_nStatsTCPFirewalledNodes > 0 && m_nStatsTCPOpenNodes > 10)
		return m_nStatsTCPFirewalledNodes / (float)(m_nStatsTCPFirewalledNodes + m_nStatsTCPOpenNodes);
	return 0;
}

void CPrefs::StatsIncUDPFirewalledNodes(bool bFirewalled)
{
	if (bFirewalled)
		++m_nStatsUDPFirewalledNodes;
	else
		++m_nStatsUDPOpenNodes;
}

void CPrefs::StatsIncTCPFirewalledNodes(bool bFirewalled)
{
	if (bFirewalled)
		++m_nStatsTCPFirewalledNodes;
	else
		++m_nStatsTCPOpenNodes;
}

float CPrefs::StatsGetKadV8Ratio()
{
	// this function is basically just a buffer, so we don't recount all nodes every time we need the result
	if (time(NULL) >= m_nStatsKadV8LastChecked + 60) {
		m_nStatsKadV8LastChecked = time(NULL);
		uint32 nV8Contacts = 0;
		uint32 nNonV8Contacts = 0;
		CKademlia::GetRoutingZone()->GetNumContacts(nV8Contacts, nNonV8Contacts, KADEMLIA_VERSION8_49b);
		//DEBUG_ONLY( AddDebugLogLine(DLP_LOW, false, _T("Counted Kad V8 Contacts: %u out of %u in routing table. FirewalledRatios: UDP - %.02f%% | TCP - %.02f%%")
		//	, nV8Contacts, nNonV8Contacts + nV8Contacts, StatsGetFirewalledRatio(true) * 100, StatsGetFirewalledRatio(false) * 100) );
		if (nV8Contacts > 0)
			m_fKadV8Ratio = nV8Contacts / (float)(nV8Contacts + nNonV8Contacts);
		else
			m_fKadV8Ratio = 0;
	}
	return m_fKadV8Ratio;
}