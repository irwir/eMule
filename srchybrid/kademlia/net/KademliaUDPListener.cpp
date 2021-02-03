/*
Copyright (C)2003 Barry Dunne (http://www.emule-project.net)
Copyright (C)2007-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )


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
#include "clientlist.h"
#include "ClientUDPSocket.h"
#include "emule.h"
#include "emuledlg.h"
#include "ipfilter.h"
#include "KadContactListCtrl.h"
#include "kademliawnd.h"
#include "listensocket.h"
#include "Log.h"
#include "opcodes.h"
#include "Packets.h"
#include "Statistics.h"
#include "updownclient.h"
#include "KademliaUDPListener.h"
#include "kademlia/io/ByteIO.h"
#include "kademlia/kademlia/Prefs.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/SearchManager.h"
#include "kademlia/kademlia/Indexed.h"
#include "kademlia/kademlia/Defines.h"
#include "kademlia/kademlia/Entry.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/routing/RoutingZone.h"
#include "kademlia/utils/KadUDPKey.h"
#include "kademlia/utils/KadClientSearcher.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern LPCSTR g_aszInvKadKeywordCharsA;
extern LPCWSTR g_awszInvKadKeywordChars;

using namespace Kademlia;

CKademliaUDPListener::~CKademliaUDPListener()
{
// report timeout to all pending FetchNodeIDRequests
	for (POSITION pos = listFetchNodeIDRequests.GetHeadPosition(); pos != NULL;)
		listFetchNodeIDRequests.GetNext(pos).pRequester->KadSearchNodeIDByIPResult(KCSR_TIMEOUT, NULL);
}

// Used by Kad1.0 and Kad 2.0
void CKademliaUDPListener::Bootstrap(LPCTSTR szHost, uint16 uUDPPort)
{
	const CStringA sHost(szHost);
	uint32 uRetVal = inet_addr(sHost);
	if (uRetVal == INADDR_NONE) {
		addrinfo hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		addrinfo *ipv4;
		if (getaddrinfo(sHost, NULL, &hints, &ipv4))
			return;
		uRetVal = reinterpret_cast<LPSOCKADDR_IN>(ipv4->ai_addr)->sin_addr.s_addr;
		freeaddrinfo(ipv4);
	}
	Bootstrap(ntohl(uRetVal), uUDPPort);
}

// Used by Kad1.0 and Kad 2.0
void CKademliaUDPListener::Bootstrap(uint32 uIP, uint16 uUDPPort, uint8 byKadVersion, const CUInt128 *uCryptTargetID)
{
	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA2_BOOTSTRAP_REQ", uIP, uUDPPort);
	CSafeMemFile fileIO(0);
	SendPacket(&fileIO, KADEMLIA2_BOOTSTRAP_REQ, uIP, uUDPPort, CKadUDPKey()
		, (byKadVersion >= KADEMLIA_VERSION6_49aBETA) ? uCryptTargetID : NULL);
}

// Used by Kad1.0 and Kad 2.0
void CKademliaUDPListener::SendMyDetails(byte byOpcode, uint32 uIP, uint16 uUDPPort, uint8 byKadVersion, const CKadUDPKey &targetUDPKey, const CUInt128 *uCryptTargetID, bool bRequestAckPackage)
{
	if (byKadVersion > 1) {
		byte byPacket[1024];
		CByteIO byteIOResponse(byPacket, sizeof(byPacket));
		byteIOResponse.WriteByte(OP_KADEMLIAHEADER);
		byteIOResponse.WriteByte(byOpcode);
		byteIOResponse.WriteUInt128(CKademlia::GetPrefs()->GetKadID());
		byteIOResponse.WriteUInt16(thePrefs.GetPort());
		byteIOResponse.WriteUInt8(KADEMLIA_VERSION);

		// Tag Count.
		uint8 byTagCount = static_cast<uint8>(!CKademlia::GetPrefs()->GetUseExternKadPort());
		if (byKadVersion >= KADEMLIA_VERSION8_49b
			&& (bRequestAckPackage || CKademlia::GetPrefs()->GetFirewalled() || CUDPFirewallTester::IsFirewalledUDP(true)))
		{
			++byTagCount;
		}
		byteIOResponse.WriteUInt8(byTagCount);
		if (!CKademlia::GetPrefs()->GetUseExternKadPort())
			byteIOResponse.WriteTag(&CKadTagUInt16(TAG_SOURCEUPORT, CKademlia::GetPrefs()->GetInternKadPort()));
		if (byKadVersion >= KADEMLIA_VERSION8_49b
			&& (bRequestAckPackage || CKademlia::GetPrefs()->GetFirewalled() || CUDPFirewallTester::IsFirewalledUDP(true)))
		{
			// if we are firewalled we sent this tag, so the other client doesn't adds us to his routing table (if UDP firewalled) and for statistics reasons (TCP firewalled)
			// 5 - reserved (!)
			// 1 - Requesting HELLO_RES_ACK
			// 1 - TCP firewalled
			// 1 - UDP firewalled
			const uint8 uUDPFirewalled = static_cast<uint8>(CUDPFirewallTester::IsFirewalledUDP(true));
			const uint8 uTCPFirewalled = static_cast<uint8>(CKademlia::GetPrefs()->GetFirewalled());
			const uint8 uRequestACK = static_cast<uint8>(bRequestAckPackage);
			const uint8 byMiscOptions = (uRequestACK << 2) | (uTCPFirewalled << 1) | (uUDPFirewalled << 0);
			byteIOResponse.WriteTag(&CKadTagUInt8(TAG_KADMISCOPTIONS, byMiscOptions));
		}
		//byteIOResponse.WriteTag(&CKadTagUInt(TAG_USER_COUNT, CKademlia::GetPrefs()->GetKademliaUsers()));
		//byteIOResponse.WriteTag(&CKadTagUInt(TAG_FILE_COUNT, CKademlia::GetPrefs()->GetKademliaFiles()));

		uint32 uLen = (sizeof byPacket) - byteIOResponse.GetAvailable();
		if (byKadVersion >= KADEMLIA_VERSION6_49aBETA) {
			if (uCryptTargetID && isnulmd4(uCryptTargetID->GetDataPtr())) {
				DebugLogWarning(_T("Sending hello response to crypt enabled Kad Node which provided an empty NodeID: %s (%u)"), (LPCTSTR)ipstr(htonl(uIP)), byKadVersion);
				SendPacket(byPacket, uLen, uIP, uUDPPort, targetUDPKey, NULL);
			} else
				SendPacket(byPacket, uLen, uIP, uUDPPort, targetUDPKey, uCryptTargetID);
		} else {
			SendPacket(byPacket, uLen, uIP, uUDPPort, CKadUDPKey(), NULL);
			ASSERT(targetUDPKey.IsEmpty());
		}
	}
	//else
	//	ASSERT( false );
}

// Kad1.0 & Kad2.0 currently.
void CKademliaUDPListener::FirewalledCheck(uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey, uint8 byKadVersion)
{
	if (byKadVersion > KADEMLIA_VERSION6_49aBETA) {
		// new Opcode since 0.49a with extended informations to support obfuscated connections properly
		CSafeMemFile fileIO(19);
		fileIO.WriteUInt16(thePrefs.GetPort());
		fileIO.WriteUInt128(&CKademlia::GetPrefs()->GetClientHash());
		fileIO.WriteUInt8(CKademlia::GetPrefs()->GetMyConnectOptions(true, false));
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugSend("KADEMLIA_FIREWALLED2_REQ", uIP, uUDPPort);
		SendPacket(&fileIO, KADEMLIA_FIREWALLED2_REQ, uIP, uUDPPort, senderUDPKey, NULL);
	} else {
		CSafeMemFile fileIO(2);
		fileIO.WriteUInt16(thePrefs.GetPort());
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugSend("KADEMLIA_FIREWALLED_REQ", uIP, uUDPPort);
		SendPacket(&fileIO, KADEMLIA_FIREWALLED_REQ, uIP, uUDPPort, senderUDPKey, NULL);
	}
	theApp.clientlist->AddKadFirewallRequest(ntohl(uIP));
}

void CKademliaUDPListener::SendNullPacket(byte byOpcode, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &targetUDPKey, const CUInt128 *uCryptTargetID)
{
	CSafeMemFile fileIO(0);
	SendPacket(&fileIO, byOpcode, uIP, uUDPPort, targetUDPKey, uCryptTargetID);
}

void CKademliaUDPListener::SendPublishSourcePacket(CContact *pContact, const CUInt128 &uTargetID, const CUInt128 &uContactID, const TagList &tags)
{
	//We need to get the tag lists working with CSafeMemFiles.
	byte byPacket[1024];
	CByteIO byteIO(byPacket, sizeof byPacket);
	byteIO.WriteByte(OP_KADEMLIAHEADER);
	if (pContact->GetVersion() >= KADEMLIA_VERSION4_47c) {
		byteIO.WriteByte(KADEMLIA2_PUBLISH_SOURCE_REQ);
		byteIO.WriteUInt128(uTargetID);
		byteIO.WriteUInt128(uContactID);
		byteIO.WriteTagList(tags);
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugSend("KADEMLIA2_PUBLISH_SOURCE_REQ", pContact->GetIPAddress(), pContact->GetUDPPort());
	} else {
		byteIO.WriteByte(KADEMLIA_PUBLISH_REQ);
		byteIO.WriteUInt128(uTargetID);
		//We only use this for publishing sources now. So we always send one here.
		byteIO.WriteUInt16(1);
		byteIO.WriteUInt128(uContactID);
		byteIO.WriteTagList(tags);
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugSend("KADEMLIA_PUBLISH_REQ", pContact->GetIPAddress(), pContact->GetUDPPort());
	}
	uint32 uLen = (sizeof byPacket) - byteIO.GetAvailable();
	if (pContact->GetVersion() >= KADEMLIA_VERSION6_49aBETA) { // obfuscated?
		CUInt128 uClientID = pContact->GetClientID();
		SendPacket(byPacket, uLen, pContact->GetIPAddress(), pContact->GetUDPPort(), pContact->GetUDPKey(), &uClientID);
	} else
		SendPacket(byPacket, uLen, pContact->GetIPAddress(), pContact->GetUDPPort(), CKadUDPKey(), NULL);
}

void CKademliaUDPListener::ProcessPacket(const byte *pbyData, uint32 uLenData, uint32 uIP, uint16 uUDPPort, bool bValidReceiverKey, const CKadUDPKey &senderUDPKey)
{
	// we do not accept (<= 0.48a) unencrypted incoming packets from port 53 (DNS) to avoid attacks based on DNS protocol confusion
	if (uUDPPort == 53 && senderUDPKey.IsEmpty()) {
		DEBUG_ONLY(DebugLog(_T("Dropping incoming unencrypted packet on port 53 (DNS), IP: %s"), (LPCTSTR)ipstr(htonl(uIP))));
		return;
	}

	//Update connection state only when it changes.
	bool bCurCon = CKademlia::GetPrefs()->HasHadContact();
	CKademlia::GetPrefs()->SetLastContact();
	CUDPFirewallTester::Connected();
	if (bCurCon != CKademlia::GetPrefs()->HasHadContact())
		theApp.emuledlg->ShowConnectionState();

	uint8 byOpcode = pbyData[1];
	switch (InTrackListIsAllowedPacket(uIP, byOpcode, bValidReceiverKey)) {
	case 2: //massive flood
		{
			CContact *pContact = CKademlia::GetRoutingZone()->GetContact(uIP, uUDPPort, false);
			if (pContact)
				pContact->Expire(); //to be deleted from routing zone
			//AddDebugLogLine(false, _T("KAD massive flood: IP=%s:%hu, opcode 0x%02x, in routing zone: %s"), (LPCTSTR)ipstr(htonl(uIP)), uUDPPort, byOpcode, pContact ? _T("yes") : _T("no"));
		}
		return;
	case 1: //flood
		//AddDebugLogLine(false, _T("KAD flood: IP=%s:%hu, opcode: 0x%02x"), (LPCTSTR)ipstr(htonl(uIP)), uUDPPort, byOpcode);
		return;
	}

	const byte *pbyPacketData = pbyData + 2;
	uLenData -= 2;
	//AddDebugLogLine( false, _T("Processing UDP Packet from %s port %ld : opcode length %ld", (LPCTSTR)ipstr(senderAddress->sin_addr), ntohs(senderAddress->sin_port), uLenData);
	//CMiscUtils::debugHexDump(pbyPacketData, uLenData);
	switch (byOpcode) {
	case KADEMLIA2_BOOTSTRAP_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_BOOTSTRAP_REQ", uIP, uUDPPort);
		Process_KADEMLIA2_BOOTSTRAP_REQ(uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA2_BOOTSTRAP_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_BOOTSTRAP_RES", uIP, uUDPPort);
		Process_KADEMLIA2_BOOTSTRAP_RES(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey, bValidReceiverKey);
		break;
	case KADEMLIA2_HELLO_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_HELLO_REQ", uIP, uUDPPort);
		Process_KADEMLIA2_HELLO_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey, bValidReceiverKey);
		break;
	case KADEMLIA2_HELLO_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_HELLO_RES", uIP, uUDPPort);
		Process_KADEMLIA2_HELLO_RES(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey, bValidReceiverKey);
		break;
	case KADEMLIA2_HELLO_RES_ACK:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_HELLO_RES_ACK", uIP, uUDPPort);
		Process_KADEMLIA2_HELLO_RES_ACK(pbyPacketData, uLenData, uIP, bValidReceiverKey);
		break;
	case KADEMLIA2_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_REQ", uIP, uUDPPort);
		Process_KADEMLIA2_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA2_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_RES", uIP, uUDPPort);
		Process_KADEMLIA2_RES(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA2_SEARCH_NOTES_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_SEARCH_NOTES_REQ", uIP, uUDPPort);
		Process_KADEMLIA2_SEARCH_NOTES_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA2_SEARCH_KEY_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_SEARCH_KEY_REQ", uIP, uUDPPort);
		Process_KADEMLIA2_SEARCH_KEY_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA2_SEARCH_SOURCE_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_SEARCH_SOURCE_REQ", uIP, uUDPPort);
		Process_KADEMLIA2_SEARCH_SOURCE_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA_SEARCH_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_SEARCH_RES", uIP, uUDPPort);
		Process_KADEMLIA_SEARCH_RES(pbyPacketData, uLenData, uIP, uUDPPort);
		break;
	case KADEMLIA_SEARCH_NOTES_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_SEARCH_NOTES_RES", uIP, uUDPPort);
		Process_KADEMLIA_SEARCH_NOTES_RES(pbyPacketData, uLenData, uIP, uUDPPort);
		break;
	case KADEMLIA2_SEARCH_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_SEARCH_RES", uIP, uUDPPort);
		Process_KADEMLIA2_SEARCH_RES(pbyPacketData, uLenData, senderUDPKey, uIP, uUDPPort);
		break;
	case KADEMLIA2_PUBLISH_KEY_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_PUBLISH_KEY_REQ", uIP, uUDPPort);
		Process_KADEMLIA2_PUBLISH_KEY_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA2_PUBLISH_SOURCE_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_PUBLISH_SOURCE_REQ", uIP, uUDPPort);
		Process_KADEMLIA2_PUBLISH_SOURCE_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA2_PUBLISH_NOTES_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_PUBLISH_NOTES_REQ", uIP, uUDPPort);
		Process_KADEMLIA2_PUBLISH_NOTES_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA_PUBLISH_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_PUBLISH_RES", uIP, uUDPPort);
		Process_KADEMLIA_PUBLISH_RES(pbyPacketData, uLenData, uIP);
		break;
	case KADEMLIA2_PUBLISH_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_PUBLISH_RES", uIP, uUDPPort);
		Process_KADEMLIA2_PUBLISH_RES(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA_FIREWALLED_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_FIREWALLED_REQ", uIP, uUDPPort);
		Process_KADEMLIA_FIREWALLED_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA_FIREWALLED2_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_FIREWALLED2_REQ", uIP, uUDPPort);
		Process_KADEMLIA_FIREWALLED2_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA_FIREWALLED_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_FIREWALLED_RES", uIP, uUDPPort);
		Process_KADEMLIA_FIREWALLED_RES(pbyPacketData, uLenData, uIP, senderUDPKey);
		break;
	case KADEMLIA_FIREWALLED_ACK_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_FIREWALLED_ACK_RES", uIP, uUDPPort);
		Process_KADEMLIA_FIREWALLED_ACK_RES(uLenData);
		break;
	case KADEMLIA_FINDBUDDY_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_FINDBUDDY_REQ", uIP, uUDPPort);
		Process_KADEMLIA_FINDBUDDY_REQ(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA_FINDBUDDY_RES:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_FINDBUDDY_RES", uIP, uUDPPort);
		Process_KADEMLIA_FINDBUDDY_RES(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA_CALLBACK_REQ:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA_CALLBACK_REQ", uIP, uUDPPort);
		Process_KADEMLIA_CALLBACK_REQ(pbyPacketData, uLenData, uIP, senderUDPKey);
		break;
	case KADEMLIA2_PING:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_PING", uIP, uUDPPort);
		Process_KADEMLIA2_PING(uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA2_PONG:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_PONG", uIP, uUDPPort);
		Process_KADEMLIA2_PONG(pbyPacketData, uLenData, uIP, uUDPPort, senderUDPKey);
		break;
	case KADEMLIA2_FIREWALLUDP:
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugRecv("KADEMLIA2_FIREWALLUDP", uIP, uUDPPort);
		Process_KADEMLIA2_FIREWALLUDP(pbyPacketData, uLenData, uIP, senderUDPKey);
		break;

	// old Kad1 opcodes which we don't handle any more
	case KADEMLIA_BOOTSTRAP_REQ_DEPRECATED:
	case KADEMLIA_BOOTSTRAP_RES_DEPRECATED:
	case KADEMLIA_HELLO_REQ_DEPRECATED:
	case KADEMLIA_HELLO_RES_DEPRECATED:
	case KADEMLIA_REQ_DEPRECATED:
	case KADEMLIA_RES_DEPRECATED:
	case KADEMLIA_PUBLISH_NOTES_REQ_DEPRECATED:
	case KADEMLIA_PUBLISH_NOTES_RES_DEPRECATED:
	case KADEMLIA_SEARCH_REQ:
	case KADEMLIA_PUBLISH_REQ:
	case KADEMLIA_SEARCH_NOTES_REQ:
		break;
	default:
		{
			CString strError;
			strError.Format(_T("Unknown opcode %02x"), byOpcode);
			throw strError;
		}
	}
}

// Used only for Kad2.0
bool CKademliaUDPListener::AddContact_KADEMLIA2(const byte *pbyData, uint32 uLenData, uint32 uIP, uint16 &uUDPPort, uint8 *pnOutVersion, const CKadUDPKey &cUDPKey, bool &rbIPVerified, bool bUpdate, bool bFromHelloReq, bool *pbOutRequestsACK, CUInt128 *puOutContactID)
{
	if (pbOutRequestsACK != NULL)
		*pbOutRequestsACK = false;

	CByteIO byteIO(pbyData, uLenData);
	CUInt128 uID;
	byteIO.ReadUInt128(&uID);
	if (puOutContactID != NULL)
		*puOutContactID = uID;
	uint16 uTCPPort = byteIO.ReadUInt16();
	uint8 uVersion = byteIO.ReadByte();
	if (pnOutVersion != NULL)
		*pnOutVersion = uVersion;

	bool bUDPFirewalled = false;
	bool bTCPFirewalled = false;
	for (unsigned uTags = byteIO.ReadByte(); uTags > 0; --uTags) {
		const CKadTag *pTag = byteIO.ReadTag();

		if (!pTag->m_name.Compare(TAG_SOURCEUPORT)) {
			if (pTag->IsInt() && (uint16)pTag->GetInt() > 0)
				uUDPPort = (uint16)pTag->GetInt();
			else
				ASSERT(0);
		} else if (!pTag->m_name.Compare(TAG_KADMISCOPTIONS)) {
			if (pTag->IsInt() && (uint16)pTag->GetInt() > 0) {
				bUDPFirewalled = (pTag->GetInt() & 0x01) > 0;
				bTCPFirewalled = (pTag->GetInt() & 0x02) > 0;
				if ((pTag->GetInt() & 0x04) > 0) {
					if (pbOutRequestsACK != NULL && uVersion >= KADEMLIA_VERSION8_49b)
						*pbOutRequestsACK = true;
					else
						ASSERT(0);
				}
			} else
				ASSERT(0);
		}

		delete pTag;
	}
	// check if we are waiting for informations (nodeid) about this client and if so inform the requester
	for (POSITION pos = listFetchNodeIDRequests.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		const FetchNodeID_Struct &fnode = listFetchNodeIDRequests.GetNext(pos);
		if (fnode.dwIP == uIP && fnode.dwTCPPort == uTCPPort) {
			CString strID;
			uID.ToHexString(&strID);
			DebugLog(_T("Result Addcontact: %s"), (LPCTSTR)strID);
			uchar uchID[16];
			uID.ToByteArray(uchID);
			fnode.pRequester->KadSearchNodeIDByIPResult(KCSR_SUCCEEDED, uchID);
			listFetchNodeIDRequests.RemoveAt(pos2);
			break;
		}
	}

	if (bFromHelloReq && uVersion >= KADEMLIA_VERSION8_49b) {
		// This is just for statistic calculations. We try to determine the ratio of (UDP) firewalled users,
		// by counting how many of all nodes, which have us in their routing table (our own routing table is supposed
		// to have no UDP firewalled nodes at all) and support the firewalled tag, are firewalled themself.
		// Obviously this only works if we are not firewalled ourselves
		CKademlia::GetPrefs()->StatsIncUDPFirewalledNodes(bUDPFirewalled);
		CKademlia::GetPrefs()->StatsIncTCPFirewalledNodes(bTCPFirewalled);
	}

	if (!bUDPFirewalled) // do not add (or update) UDP firewalled sources to our routing table
		return CKademlia::GetRoutingZone()->Add(uID, uIP, uUDPPort, uTCPPort, uVersion, cUDPKey, rbIPVerified, bUpdate, false, true);

	//DEBUG_ONLY( AddDebugLogLine(DLP_LOW, false, _T("Kad: Not adding firewalled client to routing table (%s)"), (LPCTSTR)ipstr(htonl(uIP))) );
	return false;
}

// Used only for Kad2.0
void CKademliaUDPListener::Process_KADEMLIA2_BOOTSTRAP_REQ(uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	// Get some contacts to return
	ContactList contacts;
	uint16 uNumContacts = (uint16)CKademlia::GetRoutingZone()->GetBootstrapContacts(&contacts, 20);

	// Create response packet
	//We only collect a max of 20 contacts here. Max size is 521.
	//2 + 25*20 + 19
	CSafeMemFile fileIO(521);

	fileIO.WriteUInt128(&CKademlia::GetPrefs()->GetKadID());
	fileIO.WriteUInt16(thePrefs.GetPort());
	fileIO.WriteUInt8(KADEMLIA_VERSION);

	// Write packet info
	fileIO.WriteUInt16(uNumContacts);
	for (ContactList::const_iterator iContactList = contacts.begin(); iContactList != contacts.end(); ++iContactList) {
		CContact *pContact = *iContactList;
		fileIO.WriteUInt128(&pContact->GetClientID());
		fileIO.WriteUInt32(pContact->GetIPAddress());
		fileIO.WriteUInt16(pContact->GetUDPPort());
		fileIO.WriteUInt16(pContact->GetTCPPort());
		fileIO.WriteUInt8(pContact->GetVersion());
	}

	// Send response
	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA2_BOOTSTRAP_RES", uIP, uUDPPort);

	SendPacket(&fileIO, KADEMLIA2_BOOTSTRAP_RES, uIP, uUDPPort, senderUDPKey, NULL);
}

// Used only for Kad2.0
void CKademliaUDPListener::Process_KADEMLIA2_BOOTSTRAP_RES(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey, bool bValidReceiverKey)
{
	if (!IsOnOutTrackList(uIP, KADEMLIA2_BOOTSTRAP_REQ)) {
		CString strError;
		strError.Format(_T("***NOTE: Received unrequested response packet, size (%u) in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	CRoutingZone *pRoutingZone = CKademlia::GetRoutingZone();

	// How many contacts were given
	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 uContactID;
	fileIO.ReadUInt128(&uContactID);
	uint16 uTCPPort = fileIO.ReadUInt16();
	uint8 uVersion = fileIO.ReadUInt8();
	// If we don't know any Contacts yet and try to Bootstrap, we assume that all contacts are verified,
	// in order to speed up the connecting process. The attack vectors to exploit this are very small
	// with no major effects, so that's a good trade-off
	bool bAssumeVerified = CKademlia::GetRoutingZone()->GetNumContacts() == 0;

	if (CKademlia::s_liBootstrapList.IsEmpty())
		pRoutingZone->Add(uContactID, uIP, uUDPPort, uTCPPort, uVersion, senderUDPKey, bValidReceiverKey, true, false, false);

	DEBUG_ONLY(AddDebugLogLine(DLP_LOW, false, _T("Inc Kad2 Bootstrap Packet from %s"), (LPCTSTR)ipstr(htonl(uIP))));

	for (unsigned uNumContacts = fileIO.ReadUInt16(); uNumContacts > 0; --uNumContacts) {
		fileIO.ReadUInt128(&uContactID);
		uIP = fileIO.ReadUInt32();
		uUDPPort = fileIO.ReadUInt16();
		uTCPPort = fileIO.ReadUInt16();
		uVersion = fileIO.ReadUInt8();
		bool bVerified = bAssumeVerified;
		pRoutingZone->Add(uContactID, uIP, uUDPPort, uTCPPort, uVersion, CKadUDPKey(), bVerified, false, false, false);
	}
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_HELLO_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey, bool bValidReceiverKey)
{
	//uint16 dbgOldUDPPort = uUDPPort;
	uint8 byContactVersion;
	CUInt128 uContactID;
	bool bAddedOrUpdated = AddContact_KADEMLIA2(pbyPacketData, uLenPacket, uIP, uUDPPort, &byContactVersion, senderUDPKey, bValidReceiverKey, true, true, NULL, &uContactID); // might change uUDPPort, bValidReceiverKey

	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA2_HELLO_RES", uIP, uUDPPort);
	// if this contact was added or updated (in other words, not filtered or invalid) to our routing table and did not already sent a valid
	// receiver key or is already verified in the routing table, we request an additional ACK package to complete a three-way-handshake and
	// verify the remotes IP
	SendMyDetails(KADEMLIA2_HELLO_RES, uIP, uUDPPort, byContactVersion, senderUDPKey, &uContactID, bAddedOrUpdated && !bValidReceiverKey);

	if (bAddedOrUpdated && !bValidReceiverKey && byContactVersion == KADEMLIA_VERSION7_49a && !HasActiveLegacyChallenge(uIP)) {
		// Kad Version 7 doesn't supports HELLO_RES_ACK, but sender/receiver keys, so send a ping to validate
		AddLegacyChallenge(uContactID, CUInt128(0ul), uIP, KADEMLIA2_PING);
		SendNullPacket(KADEMLIA2_PING, uIP, uUDPPort, senderUDPKey, NULL);
#ifdef _DEBUG
		CContact *pContact = CKademlia::GetRoutingZone()->GetContact(uContactID);
		if (pContact != NULL) {
			if (pContact->GetType() < 2)
				DebugLogWarning(_T("Process_KADEMLIA2_HELLO_REQ: Sending (ping) challenge to a long known contact (should be verified already) - %s"), (LPCTSTR)ipstr(htonl(uIP)));
		} else
			ASSERT(0);
#endif
	} else if (CKademlia::GetPrefs()->FindExternKadPort(false) && byContactVersion > KADEMLIA_VERSION5_48a) // do we need to find out our extern port?
		SendNullPacket(KADEMLIA2_PING, uIP, uUDPPort, senderUDPKey, NULL);

	if (bAddedOrUpdated && !bValidReceiverKey && byContactVersion < KADEMLIA_VERSION7_49a && !HasActiveLegacyChallenge(uIP)) {
		// we need to verify this contact but it doesn't support HELLO_RES_ACK nor Keys, do a little work around
		SendLegacyChallenge(uIP, uUDPPort, uContactID);
	}

	// Check if firewalled
	if (CKademlia::GetPrefs()->GetRecheckIP())
		FirewalledCheck(uIP, uUDPPort, senderUDPKey, byContactVersion);
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_HELLO_RES_ACK(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, bool bValidReceiverKey)
{
	if (uLenPacket < 17) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	if (!IsOnOutTrackList(uIP, KADEMLIA2_HELLO_RES)) {
		CString strError;
		strError.Format(_T("***NOTE: Received unrequested response packet, size (%u) in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	if (!bValidReceiverKey) {
		DebugLogWarning(_T("Kad: Process_KADEMLIA2_HELLO_RES_ACK: Receiver key is invalid! (sender: %s)"), (LPCTSTR)ipstr(htonl(uIP)));
		return;
	}
	// additional packet to complete a three-way-handshake, making sure the remote contact is not using a spoofed IP
	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 uRemoteID;
	fileIO.ReadUInt128(&uRemoteID);
	if (!CKademlia::GetRoutingZone()->VerifyContact(uRemoteID, uIP))
		DebugLogWarning(_T("Kad: Process_KADEMLIA2_HELLO_RES_ACK: Unable to find valid sender in routing table (sender: %s)"), (LPCTSTR)ipstr(htonl(uIP)));
	//else
	//	DEBUG_ONLY( AddDebugLogLine(DLP_LOW, false, _T("Verified contact (%s) by HELLO_RES_ACK"), (LPCTSTR)ipstr(htonl(uIP))) );
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_HELLO_RES(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey, bool bValidReceiverKey)
{
	if (!IsOnOutTrackList(uIP, KADEMLIA2_HELLO_REQ)) {
		CString strError;
		strError.Format(_T("***NOTE: Received unrequested response packet, size (%u) in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	// Add or Update contact.
	uint8 byContactVersion;
	CUInt128 uContactID;
	bool bSendACK;
	bool bAddedOrUpdated = AddContact_KADEMLIA2(pbyPacketData, uLenPacket, uIP, uUDPPort, &byContactVersion, senderUDPKey, bValidReceiverKey, true, false, &bSendACK, &uContactID);

	if (bSendACK) {
		// the client requested us to send an ACK package, which proves that we are not a spoofed fake contact
		// fulfill his wish
		if (senderUDPKey.IsEmpty()) {
			// but we don't have a valid senderkey - there is no point to reply in this case
			// most likely a bug in the remote client:
			DebugLogWarning(_T("Kad: Process_KADEMLIA2_HELLO_RES: Remote clients demands ACK, but didn't send any Senderkey! (%s)"), (LPCTSTR)ipstr(htonl(uIP)));
		} else {
			CSafeMemFile fileIO(17);
			CUInt128 uID(CKademlia::GetPrefs()->GetKadID());
			fileIO.WriteUInt128(&uID);
			fileIO.WriteUInt8(0); // no tags at this time
			if (thePrefs.GetDebugClientKadUDPLevel() > 0)
				DebugSend("KADEMLIA2_HELLO_RES_ACK", uIP, uUDPPort);
			SendPacket(&fileIO, KADEMLIA2_HELLO_RES_ACK, uIP, uUDPPort, senderUDPKey, NULL);
			//DEBUG_ONLY( AddDebugLogLine(DLP_LOW, false, _T("Sent HELLO_RES_ACK to %s"), (LPCTSTR)ipstr(htonl(uIP))) );
		}
	} else if (bAddedOrUpdated && !bValidReceiverKey && byContactVersion < KADEMLIA_VERSION7_49a) {
		// even through this is supposedly an answer to a request from us,
		// there are still possibilities to spoof it, as long as the attacker knows
		// that we would send a HELLO_REQ (which is the case quite often),
		// so for old Kad Version which don't support keys, we need
		SendLegacyChallenge(uIP, uUDPPort, uContactID);
	}

	// do we need to find out our extern port?
	if (CKademlia::GetPrefs()->FindExternKadPort(false) && byContactVersion > KADEMLIA_VERSION5_48a)
		SendNullPacket(KADEMLIA2_PING, uIP, uUDPPort, senderUDPKey, NULL);

	// Check if firewalled
	if (CKademlia::GetPrefs()->GetRecheckIP())
		FirewalledCheck(uIP, uUDPPort, senderUDPKey, byContactVersion);
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	// Get target and type
	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	byte byType = fileIO.ReadUInt8();
	byType = byType & 0x1F;
	if (byType == 0) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong type (0x%02x) in %hs"), byType, __FUNCTION__);
		throw strError;
	}

	//This is the target node trying to be found.
	CUInt128 uTarget;
	fileIO.ReadUInt128(&uTarget);
	//Convert Target to Distance as this is how we store contacts.
	CUInt128 uDistance(CKademlia::GetPrefs()->GetKadID());
	uDistance.Xor(uTarget);

	//This makes sure we are not mistaken identify. Some client may have fresh installed and have a new KadID.
	CUInt128 uCheck;
	fileIO.ReadUInt128(&uCheck);
	if (CKademlia::GetPrefs()->GetKadID() == uCheck) {
		// Get required number closest to target
		ContactMap results;
		CKademlia::GetRoutingZone()->GetClosestTo(2, uTarget, uDistance, (uint32)byType, &results);
		uint8 uCount = static_cast<uint8>(results.size());

		// Write response
		// Max count is 32, size 817.
		// 16 + 1 + 25*32
		CSafeMemFile fileIO2(817);
		fileIO2.WriteUInt128(&uTarget);
		fileIO2.WriteUInt8(uCount);
		for (ContactMap::const_iterator itContactMap = results.begin(); itContactMap != results.end(); ++itContactMap) {
			CUInt128 uID;
			CContact *pContact = itContactMap->second;
			pContact->GetClientID(&uID);
			fileIO2.WriteUInt128(&uID);
			fileIO2.WriteUInt32(pContact->GetIPAddress());
			fileIO2.WriteUInt16(pContact->GetUDPPort());
			fileIO2.WriteUInt16(pContact->GetTCPPort());
			fileIO2.WriteUInt8(pContact->GetVersion()); //<- Kad Version inserted to allow backward compatibility.
		}

		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugSendF("KADEMLIA2_RES", uIP, uUDPPort, _T("Count=%u"), uCount);

		SendPacket(&fileIO2, KADEMLIA2_RES, uIP, uUDPPort, senderUDPKey, NULL);
	}
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_RES(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey& /*senderUDPKey*/)
{
	if (!IsOnOutTrackList(uIP, KADEMLIA2_REQ)) {
		CString strError;
		strError.Format(_T("***NOTE: Received unrequested response packet, size (%u) in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	//Used Pointers
	CRoutingZone *pRoutingZone = CKademlia::GetRoutingZone();

	// don't do firewall checks on this opcode any more, since we need the contacts kad version - hello opcodes are good enough
	/*if(CKademlia::GetPrefs()->GetRecheckIP())
	{
		FirewalledCheck(uIP, uUDPPort, senderUDPKey);
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugSend("KADEMLIA2_HELLO_REQ", uIP, uUDPPort);
		SendMyDetails(KADEMLIA2_HELLO_REQ, uIP, uUDPPort, KADEMLIA_VERSION, senderUDPKey, NULL);
	}*/

	// What search does this relate to
	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 uTarget;
	fileIO.ReadUInt128(&uTarget);
	uint8 uNumContacts = fileIO.ReadUInt8();

	// is this one of our legacy challenge packets?
	CUInt128 uContactID;
	if (IsLegacyChallenge(uTarget, uIP, KADEMLIA2_REQ, uContactID)) {
		// yup it is, set the contact as verified
		if (!CKademlia::GetRoutingZone()->VerifyContact(uContactID, uIP))
			DebugLogWarning(_T("Kad: KADEMLIA2_RES: Unable to find valid sender in routing table (sender: %s)"), (LPCTSTR)ipstr(htonl(uIP)));
		else
			DEBUG_ONLY(AddDebugLogLine(DLP_VERYLOW, false, _T("Verified contact with legacy challenge (KADEMLIA2_REQ) - %s"), (LPCTSTR)ipstr(htonl(uIP))));
		return; // we do not actually care for its other content
	}

	// Verify packet is expected size
	if (uLenPacket != (uint32)(16 + 1 + (16 + 4 + 2 + 2 + 1)*uNumContacts)) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	// Verify that the search is still active and contains no more than the requested numbers of contacts
	if (uNumContacts > CSearchManager::GetExpectedResponseContactCount(uTarget)) {
		if (CSearchManager::GetExpectedResponseContactCount(uTarget) == 0)
			DebugLogWarning(_T("Kad: KADEMLIA2_RES: Search already expired, ignoring answer (sender: %s)"), (LPCTSTR)ipstr(htonl(uIP)));
		else
			DebugLogWarning(_T("Kad: KADEMLIA2_RES: Contact sent more nodes (%u) than requested (%u), ignoring answer (sender: %s)")
				, uNumContacts, CSearchManager::GetExpectedResponseContactCount(uTarget), (LPCTSTR)ipstr(htonl(uIP)));
		return;
	}


	// is this a search for firewall check ips?
	bool bIsFirewallUDPCheckSearch = false;
	if (CUDPFirewallTester::IsFWCheckUDPRunning() && CSearchManager::IsFWCheckUDPSearch(uTarget))
		bIsFirewallUDPCheckSearch = true;

	uint32 nIgnoredCount = 0;
	ContactList *pResults = new ContactList;
	CUInt128 uIDResult;
	try {
		for (uint8 iIndex = 0; iIndex < uNumContacts; ++iIndex) {
			fileIO.ReadUInt128(&uIDResult);
			uint32 uIPResult = fileIO.ReadUInt32();
			uint16 uUDPPortResult = fileIO.ReadUInt16();
			uint16 uTCPPortResult = fileIO.ReadUInt16();
			uint8 uVersion = fileIO.ReadUInt8();
			uint32 uhostIPResult = htonl(uIPResult);
			if (uVersion >= KADEMLIA_VERSION2_47a) { // Kad1 nodes are no longer accepted, and are ignored
				if (IsGoodIPPort(uhostIPResult, uUDPPortResult)) {
					if (!theApp.ipfilter->IsFiltered(uhostIPResult) && !(uUDPPortResult == 53 && uVersion <= KADEMLIA_VERSION5_48a)  /*No DNS Port without encryption*/) {
						if (bIsFirewallUDPCheckSearch) {
							// UDP FirewallCheck searches are special. The point is we need an IP which we didn't send a UDP message yet
							// (or in the near future), so we do not try to add those contacts to our routing zone, and we also don't
							// deliver them back to the search manager (because he would UDP-ask them for further results), but only report
							// them to FirewallCheck - this will of course cripple the search but that's not the point, since we only
							// care for IPs and not the randomly set target
							CUDPFirewallTester::AddPossibleTestContact(uIDResult, uIPResult, uUDPPortResult, uTCPPortResult, uTarget, uVersion, CKadUDPKey(), false);
						} else {
							bool bVerified = false;
							bool bWasAdded = pRoutingZone->AddUnfiltered(uIDResult, uIPResult, uUDPPortResult, uTCPPortResult, uVersion, CKadUDPKey(), bVerified, false, false, false);
							CContact *pTemp = new CContact(uIDResult, uIPResult, uUDPPortResult, uTCPPortResult, uTarget, uVersion, CKadUDPKey(), false);
							if (bWasAdded || pRoutingZone->IsAcceptableContact(pTemp))
								pResults->push_back(pTemp);
							else {
								++nIgnoredCount;
								delete pTemp;
							}
						}
					} else if (!(uUDPPortResult == 53 && uVersion <= KADEMLIA_VERSION5_48a) && thePrefs.GetLogFilteredIPs())
						AddDebugLogLine(false, _T("Ignored kad contact (IP=%s:%u) - IP filter (%s)"), (LPCTSTR)ipstr(uhostIPResult), uUDPPortResult, (LPCTSTR)theApp.ipfilter->GetLastHit());
					else if (thePrefs.GetLogFilteredIPs())
						AddDebugLogLine(false, _T("Ignored kad contact (IP=%s:%u) - Bad port (Kad2_Res)"), (LPCTSTR)ipstr(uhostIPResult), uUDPPortResult);
				} else if (thePrefs.GetLogFilteredIPs())
					AddDebugLogLine(false, _T("Ignored kad contact (IP=%s) - Bad IP"), (LPCTSTR)ipstr(uhostIPResult));
			}
		}
	} catch (...) {
		for (ContactList::const_iterator it = pResults->begin(); it != pResults->end(); ++it)
			delete *it;
		delete pResults;
		throw;
	}
	if (nIgnoredCount > 0)
		DebugLogWarning(_T("Ignored %u bad contacts in routing answer from %s"), nIgnoredCount, (LPCTSTR)ipstr(htonl(uIP)));
	CSearchManager::ProcessResponse(uTarget, uIP, uUDPPort, pResults);
}

void CKademliaUDPListener::Free(SSearchTerm *pSearchTerms)
{
	if (pSearchTerms) {
		if (pSearchTerms->m_pLeft)
			Free(pSearchTerms->m_pLeft);
		if (pSearchTerms->m_pRight)
			Free(pSearchTerms->m_pRight);
		delete pSearchTerms;
	}
}

static void TokenizeOptQuotedSearchTerm(LPCTSTR pszString, CStringWArray *lst)
{
	for (LPCTSTR pch = pszString; *pch != _T('\0');) {
		if (*pch == _T('\"')) {
			// Start of quoted string found. If there is no terminating quote character found,
			// the start quote character is just skipped. If the quoted string is empty, no
			// new entry is added to 'list'.
			//
			++pch;
			LPCTSTR pchNextQuote = _tcschr(pch, _T('\"'));
			if (pchNextQuote) {
				size_t nLenQuoted = pchNextQuote - pch;
				if (nLenQuoted)
					lst->Add(CString(pch, (int)nLenQuoted));
				pch = pchNextQuote + 1;
			}
		} else {
			// Search for next delimiter or quote character
			//
			size_t nNextDelimiter = _tcscspn(pch, _T(INV_KAD_KEYWORD_CHARS));
			if (nNextDelimiter) {
				lst->Add(CString(pch, (int)nNextDelimiter));
				pch += nNextDelimiter;
				if (*pch == _T('\0'))
					break;
				if (*pch == _T('\"'))
					continue;
			}
			++pch;
		}
	}
}

static CString *s_pstrDbgSearchExpr;

SSearchTerm* CKademliaUDPListener::CreateSearchExpressionTree(CSafeMemFile &fileIO, int iLevel)
{
	// the max. depth has to match our own limit for creating the search expression
	// (see also 'ParsedSearchExpression' and 'GetSearchPacket')
	if (iLevel >= 24) {
		AddDebugLogLine(false, _T("***NOTE: Search expression tree exceeds depth limit!"));
		return NULL;
	}
	++iLevel;

	uint8 uOp = fileIO.ReadUInt8();
	switch (uOp) {
	case 0x00: // boolean op
		{
			uint8 uBoolOp = fileIO.ReadUInt8();
			switch (uBoolOp) {
			case 0x00: // AND
				{
					SSearchTerm *pSearchTerm = new SSearchTerm;
					pSearchTerm->m_type = SSearchTerm::AND;
					if (s_pstrDbgSearchExpr)
						s_pstrDbgSearchExpr->Append(_T(" AND"));
					if ((pSearchTerm->m_pLeft = CreateSearchExpressionTree(fileIO, iLevel)) == NULL) {
						ASSERT(0);
						delete pSearchTerm;
						return NULL;
					}
					if ((pSearchTerm->m_pRight = CreateSearchExpressionTree(fileIO, iLevel)) == NULL) {
						ASSERT(0);
						Free(pSearchTerm->m_pLeft);
						delete pSearchTerm;
						return NULL;
					}
					return pSearchTerm;
				}
			case 0x01: // OR
				{
					SSearchTerm *pSearchTerm = new SSearchTerm;
					pSearchTerm->m_type = SSearchTerm::OR;
					if (s_pstrDbgSearchExpr)
						s_pstrDbgSearchExpr->Append(_T(" OR"));
					if ((pSearchTerm->m_pLeft = CreateSearchExpressionTree(fileIO, iLevel)) == NULL) {
						ASSERT(0);
						delete pSearchTerm;
						return NULL;
					}
					if ((pSearchTerm->m_pRight = CreateSearchExpressionTree(fileIO, iLevel)) == NULL) {
						ASSERT(0);
						Free(pSearchTerm->m_pLeft);
						delete pSearchTerm;
						return NULL;
					}
					return pSearchTerm;
				}
			case 0x02: // NOT
				{
					SSearchTerm *pSearchTerm = new SSearchTerm;
					pSearchTerm->m_type = SSearchTerm::NOT;
					if (s_pstrDbgSearchExpr)
						s_pstrDbgSearchExpr->Append(_T(" NOT"));
					if ((pSearchTerm->m_pLeft = CreateSearchExpressionTree(fileIO, iLevel)) == NULL) {
						ASSERT(0);
						delete pSearchTerm;
						return NULL;
					}
					if ((pSearchTerm->m_pRight = CreateSearchExpressionTree(fileIO, iLevel)) == NULL) {
						ASSERT(0);
						Free(pSearchTerm->m_pLeft);
						delete pSearchTerm;
						return NULL;
					}
					return pSearchTerm;
				}
			}
			AddDebugLogLine(false, _T("*** Unknown boolean search operator 0x%02x (CreateSearchExpressionTree)"), uBoolOp);
			return NULL;
		}
	case 0x01: // string
		{
			CKadTagValueString str(fileIO.ReadStringUTF8());

			KadTagStrMakeLower(str); // make lower case, the search code expects lower case strings!
			if (s_pstrDbgSearchExpr)
				s_pstrDbgSearchExpr->AppendFormat(_T(" \"%ls\""), (LPCTSTR)str);

			SSearchTerm *pSearchTerm = new SSearchTerm;
			pSearchTerm->m_type = SSearchTerm::String;
			pSearchTerm->m_pastr = new CStringWArray;

			// pre-tokenize the string term (care about quoted parts)
			TokenizeOptQuotedSearchTerm(str, pSearchTerm->m_pastr);

			return pSearchTerm;
		}
	case 0x02: // Meta tag
		{
			// read tag value
			CKadTagValueString strValue(fileIO.ReadStringUTF8());

			KadTagStrMakeLower(strValue); // make lower case, the search code expects lower case strings!

			// read tag name
			CStringA strTagName;
			uint16 lenTagName = fileIO.ReadUInt16();
			fileIO.Read(strTagName.GetBuffer(lenTagName), lenTagName);
			strTagName.ReleaseBuffer(lenTagName);

			SSearchTerm *pSearchTerm = new SSearchTerm;
			pSearchTerm->m_type = SSearchTerm::MetaTag;
			pSearchTerm->m_pTag = new Kademlia::CKadTagStr(strTagName, strValue);
			if (s_pstrDbgSearchExpr) {
				if (lenTagName == 1)
					s_pstrDbgSearchExpr->AppendFormat(_T(" Tag%02X=\"%ls\""), (BYTE)strTagName[0], (LPCTSTR)strValue);
				else
					s_pstrDbgSearchExpr->AppendFormat(_T(" \"%hs\"=\"%ls\""), (LPCSTR)strTagName, (LPCTSTR)strValue);
			}
			return pSearchTerm;
		}
	case 0x03: // Numeric Relation 32-bit
	case 0x08: // Numeric Relation 64-bit
		{
			static const struct
			{
				SSearchTerm::ESearchTermType eSearchTermOp;
				LPCTSTR pszOp;
			}
			_aOps[] =
			{
				{ SSearchTerm::OpEqual,			_T("=")	 }, // mmop=0x00
				{ SSearchTerm::OpGreater,		_T(">")	 }, // mmop=0x01
				{ SSearchTerm::OpLess,			_T("<")	 }, // mmop=0x02
				{ SSearchTerm::OpGreaterEqual,	_T(">=") }, // mmop=0x03
				{ SSearchTerm::OpLessEqual,		_T("<=") }, // mmop=0x04
				{ SSearchTerm::OpNotEqual,		_T("<>") }  // mmop=0x05
			};

			// read tag value
			uint64 ullValue = (uOp == 0x03) ? fileIO.ReadUInt32() : fileIO.ReadUInt64();

			// read integer operator
			uint8 mmop = fileIO.ReadUInt8();
			if (mmop >= _countof(_aOps)) {
				AddDebugLogLine(false, _T("*** Unknown integer search op=0x%02x (CreateSearchExpressionTree)"), mmop);
				return NULL;
			}

			// read tag name
			CStringA strTagName;
			uint16 uLenTagName = fileIO.ReadUInt16();
			fileIO.Read(strTagName.GetBuffer(uLenTagName), uLenTagName);
			strTagName.ReleaseBuffer(uLenTagName);

			SSearchTerm *pSearchTerm = new SSearchTerm;
			pSearchTerm->m_type = _aOps[mmop].eSearchTermOp;
			pSearchTerm->m_pTag = new Kademlia::CKadTagUInt64(strTagName, ullValue);

			if (s_pstrDbgSearchExpr)
				if (uLenTagName == 1)
					s_pstrDbgSearchExpr->AppendFormat(_T(" Tag%02X%s%I64u"), (BYTE)strTagName[0], _aOps[mmop].pszOp, ullValue);
				else
					s_pstrDbgSearchExpr->AppendFormat(_T(" \"%hs\"%s%I64u"), (LPCSTR)strTagName, _aOps[mmop].pszOp, ullValue);

			return pSearchTerm;
		}
	}
	AddDebugLogLine(false, _T("*** Unknown search op=0x%02x (CreateSearchExpressionTree)"), uOp);
	return NULL;
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_SEARCH_KEY_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 uTarget;
	fileIO.ReadUInt128(&uTarget);
	uint16 uStartPosition = fileIO.ReadUInt16();
	bool uRestrictive = ((uStartPosition & 0x8000) != 0);
	uStartPosition &= 0x7FFF;
	SSearchTerm *pSearchTerms = NULL;
	if (uRestrictive) {
		try {
#if defined(_DEBUG) || defined(USE_DEBUG_DEVICE)
			s_pstrDbgSearchExpr = (thePrefs.GetDebugServerSearchesLevel() > 0) ? new CString() : NULL;
#endif
			pSearchTerms = CreateSearchExpressionTree(fileIO, 0);
			if (s_pstrDbgSearchExpr) {
				Debug(_T("KadSearchTerm=%s\n"), (LPCTSTR)*s_pstrDbgSearchExpr);
				delete s_pstrDbgSearchExpr;
				s_pstrDbgSearchExpr = NULL;
			}
		} catch (...) {
			delete s_pstrDbgSearchExpr;
			s_pstrDbgSearchExpr = NULL;
			Free(pSearchTerms);
			throw;
		}
		if (pSearchTerms == NULL)
			throw CString(_T("Invalid search expression"));
	}
	CKademlia::GetIndexed()->SendValidKeywordResult(uTarget, pSearchTerms, uIP, uUDPPort, false, uStartPosition, senderUDPKey);
	Free(pSearchTerms);
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_SEARCH_SOURCE_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 uTarget;
	fileIO.ReadUInt128(&uTarget);
	uint16 uStartPosition = (fileIO.ReadUInt16() & 0x7FFF);
	uint64 uFileSize = fileIO.ReadUInt64();
	CKademlia::GetIndexed()->SendValidSourceResult(uTarget, uIP, uUDPPort, uStartPosition, uFileSize, senderUDPKey);
}

// Used in Kad1.0 only
void CKademliaUDPListener::Process_KADEMLIA_SEARCH_RES(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort)
{
	// Verify packet is expected size
	if (uLenPacket < 37) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	// What search does this relate to
	CByteIO byteIO(pbyPacketData, uLenPacket);
	CUInt128 uTarget;
	byteIO.ReadUInt128(&uTarget);

	// How many results. Not supported yet.
	CUInt128 uAnswer;
	for (unsigned uCount = byteIO.ReadUInt16(); uCount > 0; --uCount) {
		// What is the answer
		byteIO.ReadUInt128(&uAnswer);

		// Get info about answer
		// NOTE: this is the one and only place in Kad where we allow string conversion to local code page in
		// case we did not receive a UTF-8 string. This is for backward compatibility for search results which are
		// supposed to be 'viewed' by user only and not fed into the Kad engine again!
		// If that tag list is once used for something else than for viewing, special care has to be taken for any
		// string conversion!
		TagList *pTags = NULL;
		try {
			pTags = new TagList;
			byteIO.ReadTagList(pTags, true);
		} catch (...) {
			if (pTags) {
				deleteTagListEntries(pTags);
				delete pTags;
			}
			throw;
		}
		CSearchManager::ProcessResult(uTarget, uAnswer, pTags, uIP, uUDPPort);
	}
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_SEARCH_RES(const byte *pbyPacketData, uint32 uLenPacket, const CKadUDPKey& /*senderUDPKey*/, uint32 uIP, uint16 uUDPPort)
{
	CByteIO byteIO(pbyPacketData, uLenPacket);

	// Who sent this packet.
	CUInt128 uSource;
	byteIO.ReadUInt128(&uSource);

	// What search does this relate to
	CUInt128 uTarget;
	byteIO.ReadUInt128(&uTarget);

	// Total results.
	CUInt128 uAnswer;
	for (unsigned uCount = byteIO.ReadUInt16(); uCount > 0; --uCount) {
		// What is the answer
		byteIO.ReadUInt128(&uAnswer);

		// Get info about answer
		// NOTE: this is the one and only place in Kad where we allow string conversion to local code page in
		// case we did not receive a UTF-8 string. this is for backward compatibility for search results which are
		// supposed to be 'viewed' by user only and not feed into the Kad engine again!
		// If that tag list is once used for something else than for viewing, special care has to be taken for any
		// string conversion!
		TagList *pTags = NULL;
		try {
			pTags = new TagList;
			byteIO.ReadTagList(pTags, true);
		} catch (...) {
			if (pTags) {
				deleteTagListEntries(pTags);
				delete pTags;
			}
			throw;
		}
		CSearchManager::ProcessResult(uTarget, uAnswer, pTags, uIP, uUDPPort);
	}
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_PUBLISH_KEY_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	// check if we are UDP firewalled
	if (CUDPFirewallTester::IsFirewalledUDP(true))
		//We are firewalled. We should not index this entry and give publisher a false report.
		return;

	CByteIO byteIO(pbyPacketData, uLenPacket);
	CUInt128 uFile;
	byteIO.ReadUInt128(&uFile);

	CUInt128 uDistance(CKademlia::GetPrefs()->GetKadID());
	uDistance.Xor(uFile);

	// Shouldn't LAN IPs already be filtered?
	if (uDistance.Get32BitChunk(0) > SEARCHTOLERANCE && !IsLANIP(ntohl(uIP)))
		return;

	bool bDbgInfo = (thePrefs.GetDebugClientKadUDPLevel() > 0);
	uint8 uLoad = 0;
	CUInt128 uTarget;
	for (unsigned uCount = byteIO.ReadUInt16(); uCount > 0; --uCount) {
		CKadTag *pTag = NULL;
		CKeyEntry *pEntry = NULL;
		try {
			CString sInfo;
			byteIO.ReadUInt128(&uTarget);
			pEntry = new Kademlia::CKeyEntry();
			pEntry->m_uIP = uIP;
			pEntry->m_uUDPPort = uUDPPort;
			pEntry->m_uKeyID.SetValue(uFile);
			pEntry->m_uSourceID.SetValue(uTarget);
			pEntry->m_tLifetime = time(NULL) + KADEMLIAREPUBLISHTIMEK;
			pEntry->m_bSource = false;
			for (unsigned uTags = byteIO.ReadByte(); uTags > 0; --uTags) {
				pTag = byteIO.ReadTag();
				if (pTag) {
					if (!pTag->m_name.Compare(TAG_FILENAME)) {
						if (pEntry->GetCommonFileName().IsEmpty()) {
							pEntry->SetFileName(pTag->GetStr());
							if (bDbgInfo)
								sInfo.AppendFormat(_T("  Name=\"%ls\""), (LPCTSTR)pEntry->GetCommonFileName());
						}
					} else if (!pTag->m_name.Compare(TAG_FILESIZE)) {
						if (pEntry->m_uSize == 0) {
							if (pTag->IsBsob() && pTag->GetBsobSize() == 8)
								pEntry->m_uSize = *((uint64*)pTag->GetBsob());
							else
								pEntry->m_uSize = pTag->GetInt();
							if (bDbgInfo)
								sInfo.AppendFormat(_T("  Size=%I64u"), pEntry->m_uSize);
						}
					} else if (!pTag->m_name.Compare(TAG_KADAICHHASHPUB)) {
						if (pTag->IsBsob() && pTag->GetBsobSize() == CAICHHash::GetHashSize()) {
							if (pEntry->GetAICHHashCount() == 0) {
								pEntry->AddRemoveAICHHash(CAICHHash((uchar*)pTag->GetBsob()), true);
								if (bDbgInfo)
									sInfo.AppendFormat(_T("  AICH Hash=%s"), (LPCTSTR)CAICHHash((uchar*)pTag->GetBsob()).GetString());
							} else
								DebugLogWarning(_T("Multiple TAG_KADAICHHASHPUB tags received for single file from %s"), (LPCTSTR)ipstr(htonl(uIP)));
						} else
							DEBUG_ONLY(DebugLogWarning(_T("Bad TAG_KADAICHHASHPUB received from %s"), (LPCTSTR)ipstr(htonl(uIP))));
					} else {
						//TODO: Filter tags - we do some basic filtering already within this function, might want to do more at some point
						pEntry->AddTag(pTag);
						pTag = NULL;
					}
					delete pTag; //tag is no longer stored, but membervar is used
					pTag = NULL;
				}
			}
			if (bDbgInfo && !sInfo.IsEmpty())
				Debug(_T("%s\n"), (LPCTSTR)sInfo);
		} catch (...) {
			delete pTag;
			delete pEntry;
			throw;
		}

		if (!CKademlia::GetIndexed()->AddKeyword(uFile, uTarget, pEntry, uLoad)) {
			//We already indexed the maximum number of keywords.
			//We do not index any more but we still send a success.
			//Reason: Because if a VERY busy node tells the publisher it failed,
			//this busy node will spread to all the surrounding nodes causing popular
			//keywords to be stored on MANY nodes.
			//So, once we are full, we will periodically clean our list until we can
			//begin storing again.
			delete pEntry;
		}
	}
	CSafeMemFile fileIO2(17);
	fileIO2.WriteUInt128(&uFile);
	fileIO2.WriteUInt8(uLoad);
	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA2_PUBLISH_RES", uIP, uUDPPort);
	SendPacket(&fileIO2, KADEMLIA2_PUBLISH_RES, uIP, uUDPPort, senderUDPKey, NULL);
}

// Used in Kad2.0 only
void CKademliaUDPListener::Process_KADEMLIA2_PUBLISH_SOURCE_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	// check if we are UDP firewalled
	if (CUDPFirewallTester::IsFirewalledUDP(true))
		//We are firewalled. We should not index this entry and give publisher a false report.
		return;

	CByteIO byteIO(pbyPacketData, uLenPacket);
	CUInt128 uFile;
	byteIO.ReadUInt128(&uFile);

	CUInt128 uDistance(CKademlia::GetPrefs()->GetKadID());
	uDistance.Xor(uFile);

	if (uDistance.Get32BitChunk(0) > SEARCHTOLERANCE && !IsLANIP(ntohl(uIP)))
		return;

	bool bDbgInfo = (thePrefs.GetDebugClientKadUDPLevel() > 0);
	uint8 uLoad = 0;
	CUInt128 uTarget;
	byteIO.ReadUInt128(&uTarget);
	CKadTag *pTag = NULL;
	CEntry *pEntry = NULL;
	try {
		CString sInfo;
		pEntry = new Kademlia::CEntry();
		pEntry->m_uIP = uIP;
		pEntry->m_uUDPPort = uUDPPort;
		pEntry->m_uKeyID.SetValue(uFile);
		pEntry->m_uSourceID.SetValue(uTarget);
		pEntry->m_bSource = false;
		pEntry->m_tLifetime = time(NULL) + KADEMLIAREPUBLISHTIMES;
		bool bAddUDPPortTag = true;
		for (unsigned uTags = byteIO.ReadByte(); uTags > 0; --uTags) {
			pTag = byteIO.ReadTag();
			if (pTag) {
				if (!pTag->m_name.Compare(TAG_SOURCETYPE)) {
					if (!pEntry->m_bSource) {
						pEntry->AddTag(new CKadTagUInt(TAG_SOURCEIP, pEntry->m_uIP));
						pEntry->m_bSource = true;
						pEntry->AddTag(pTag);
						pTag = NULL;
					}
				} else if (!pTag->m_name.Compare(TAG_FILESIZE)) {
					if (pEntry->m_uSize == 0) {
						if (pTag->IsBsob() && pTag->GetBsobSize() == 8)
							pEntry->m_uSize = *((uint64*)pTag->GetBsob());
						else
							pEntry->m_uSize = pTag->GetInt();
						if (bDbgInfo)
							sInfo.AppendFormat(_T("  Size=%I64u"), pEntry->m_uSize);
					}
				} else if (!pTag->m_name.Compare(TAG_SOURCEPORT)) {
					if (pEntry->m_uTCPPort == 0) {
						pEntry->m_uTCPPort = (uint16)pTag->GetInt();
						pEntry->AddTag(pTag);
						pTag = NULL;
					}
				} else if (!pTag->m_name.Compare(TAG_SOURCEUPORT)) {
					if (bAddUDPPortTag && pTag->IsInt() && (uint16)pTag->GetInt() > 0) {
						pEntry->m_uUDPPort = (uint16)pTag->GetInt();
						bAddUDPPortTag = false;
						pEntry->AddTag(pTag);
						pTag = NULL;
					}
				} else if (!pTag->m_name.Compare(TAG_SERVERIP)) {
					//drop lowID sources with unreachable buddy (Enig123)
					if (pTag->IsInt()) {
						LPCTSTR p = NULL;
						uint32 buddyip = (uint32)pTag->GetInt();
						//if (!IsGoodIP(buddyip)) {
						//	if (thePrefs.GetLogFilteredIPs())
						//		p = _T("bad");
						//} else
						if (theApp.ipfilter->IsFiltered(buddyip)) {
							if (thePrefs.GetLogFilteredIPs())
								p = _T("IP-filtered");
						} else if (theApp.clientlist->IsBannedClient(buddyip)) {
							if (thePrefs.GetLogBannedClients())
								p = _T("banned");
						} else {
							pEntry->AddTag(pTag);
							pTag = NULL;
						}

						if (pTag) {
							pEntry->m_bSource = false;
							if (p)
								AddDebugLogLine(false, _T("Publish request from source %s with %s buddy IP=%s"), (LPCTSTR)ipstr(htonl(uIP)), p, (LPCTSTR)ipstr(buddyip));
						}
					}
				} else {
					//TODO: Filter tags
					pEntry->AddTag(pTag);
					pTag = NULL; //do not delete
				}
				delete pTag;
				pTag = NULL;
			}
		}
		if (bAddUDPPortTag)
			pEntry->AddTag(new CKadTagUInt(TAG_SOURCEUPORT, pEntry->m_uUDPPort));

		if (bDbgInfo && !sInfo.IsEmpty())
			Debug(_T("%s\n"), (LPCTSTR)sInfo);
	} catch (...) {
		delete pTag;
		delete pEntry;
		throw;
	}

	if (pEntry->m_bSource && CKademlia::GetIndexed()->AddSources(uFile, uTarget, pEntry, uLoad)) {
		CSafeMemFile fileIO2(17);
		fileIO2.WriteUInt128(&uFile);
		fileIO2.WriteUInt8(uLoad);
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugSend("KADEMLIA2_PUBLISH_RES", uIP, uUDPPort);
		SendPacket(&fileIO2, KADEMLIA2_PUBLISH_RES, uIP, uUDPPort, senderUDPKey, NULL);
	} else
		delete pEntry;
}

// Used only by Kad1.0
void CKademliaUDPListener::Process_KADEMLIA_PUBLISH_RES(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP)
{
	// Verify packet is expected size
	if (uLenPacket < 16) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	if (!IsOnOutTrackList(uIP, KADEMLIA_PUBLISH_REQ)) {
		CString strError;
		strError.Format(_T("***NOTE: Received unrequested response packet, size (%u) in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 uFile;
	fileIO.ReadUInt128(&uFile);

	bool bLoadResponse = false;
	uint8 uLoad = 0;
	if (fileIO.GetLength() > fileIO.GetPosition()) {
		bLoadResponse = true;
		uLoad = fileIO.ReadUInt8();
	}

	CSearchManager::ProcessPublishResult(uFile, uLoad, bLoadResponse);
}

// Used only by Kad2.0
void CKademliaUDPListener::Process_KADEMLIA2_PUBLISH_RES(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	if (!IsOnOutTrackList(uIP, KADEMLIA2_PUBLISH_KEY_REQ) && !IsOnOutTrackList(uIP, KADEMLIA2_PUBLISH_SOURCE_REQ) && !IsOnOutTrackList(uIP, KADEMLIA2_PUBLISH_NOTES_REQ)) {
		CString strError;
		strError.Format(_T("***NOTE: Received unrequested response packet, size (%u) in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 uFile;
	fileIO.ReadUInt128(&uFile);
	uint8 uLoad = fileIO.ReadUInt8();
	CSearchManager::ProcessPublishResult(uFile, uLoad, true);
	if (fileIO.GetLength() > fileIO.GetPosition()) {
		// for future use
		uint8 byOptions = fileIO.ReadUInt8();
		bool bRequestACK = (byOptions & 0x01) > 0;
		if (bRequestACK && !senderUDPKey.IsEmpty()) {
			DEBUG_ONLY(DebugLogWarning(_T("KADEMLIA2_PUBLISH_RES_ACK requested (%s)"), (LPCTSTR)ipstr(htonl(uIP))));
			if (thePrefs.GetDebugClientKadUDPLevel() > 0)
				DebugSend("KADEMLIA2_PUBLISH_RES_ACK", uIP, uUDPPort);
			SendNullPacket(KADEMLIA2_PUBLISH_RES_ACK, uIP, uUDPPort, senderUDPKey, NULL);
		}
	}
}

// Used only by Kad2.0
void CKademliaUDPListener::Process_KADEMLIA2_SEARCH_NOTES_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 uTarget;
	fileIO.ReadUInt128(&uTarget);
	uint64 uFileSize = fileIO.ReadUInt64();
	CKademlia::GetIndexed()->SendValidNoteResult(uTarget, uIP, uUDPPort, uFileSize, senderUDPKey);
}

// Used only by Kad1.0
void CKademliaUDPListener::Process_KADEMLIA_SEARCH_NOTES_RES(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort)
{
	// Verify packet is expected size
	if (uLenPacket < 37) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	if (!IsOnOutTrackList(uIP, KADEMLIA_SEARCH_NOTES_REQ, true)) {
		CString strError;
		strError.Format(_T("***NOTE: Received unrequested response packet, size (%u) in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	// What search does this relate to
	CByteIO byteIO(pbyPacketData, uLenPacket);
	CUInt128 uTarget;
	byteIO.ReadUInt128(&uTarget);

	CUInt128 uAnswer;
	for (unsigned uCount = byteIO.ReadUInt16(); uCount > 0; --uCount) {
		// What is the answer
		byteIO.ReadUInt128(&uAnswer);

		// Get info about answer
		// NOTE: this is the one and only place in Kad where we allow string conversion to local code page in
		// case we did not receive a UTF-8 string. This is for backward compatibility for search results which are
		// supposed to be 'viewed' by user only and not feed into the Kad engine again!
		// If that tag list is once used for something else than for viewing, special care has to be taken for any
		// string conversion!
		TagList *pTags = NULL;
		try {
			pTags = new TagList;
			byteIO.ReadTagList(pTags, true);
		} catch (...) {
			if (pTags) {
				deleteTagListEntries(pTags);
				delete pTags;
			}
			throw;
		}
		CSearchManager::ProcessResult(uTarget, uAnswer, pTags, uIP, uUDPPort);
	}
}

// Used only by Kad2.0
void CKademliaUDPListener::Process_KADEMLIA2_PUBLISH_NOTES_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	// check if we are UDP firewalled
	if (CUDPFirewallTester::IsFirewalledUDP(true)) {
		//We are firewalled. We should not index this entry and give publisher a false report.
		return;
	}

	CByteIO byteIO(pbyPacketData, uLenPacket);
	CUInt128 uTarget;
	byteIO.ReadUInt128(&uTarget);

	CUInt128 uDistance(CKademlia::GetPrefs()->GetKadID());
	uDistance.Xor(uTarget);

	// Shouldn't LAN IPs already be filtered?
	if (uDistance.Get32BitChunk(0) > SEARCHTOLERANCE && !IsLANIP(ntohl(uIP)))
		return;

	CUInt128 uSource;
	byteIO.ReadUInt128(&uSource);

	CKadTag *pTag = NULL;
	Kademlia::CEntry *pEntry = new Kademlia::CEntry();
	try {
		pEntry->m_uIP = uIP;
		pEntry->m_uUDPPort = uUDPPort;
		pEntry->m_uKeyID.SetValue(uTarget);
		pEntry->m_uSourceID.SetValue(uSource);
		pEntry->m_bSource = false;
		for (unsigned uTags = byteIO.ReadByte(); uTags > 0; --uTags) {
			pTag = byteIO.ReadTag();
			if (pTag) {
				if (!pTag->m_name.Compare(TAG_FILENAME)) {
					if (pEntry->GetCommonFileName().IsEmpty())
						pEntry->SetFileName(pTag->GetStr());
				} else if (!pTag->m_name.Compare(TAG_FILESIZE)) {
					if (pEntry->m_uSize == 0)
						pEntry->m_uSize = pTag->GetInt();
				} else {
					//TODO: Filter tags
					pEntry->AddTag(pTag);
					pTag = NULL;
				}
				delete pTag;
				pTag = NULL;
			}
		}
	} catch (...) {
		delete pTag;
		delete pEntry;
		throw;
	}

	uint8 uLoad = 0;
	if (CKademlia::GetIndexed()->AddNotes(uTarget, uSource, pEntry, uLoad)) {
		CSafeMemFile fileIO2(17);
		fileIO2.WriteUInt128(&uTarget);
		fileIO2.WriteUInt8(uLoad);
		if (thePrefs.GetDebugClientKadUDPLevel() > 0)
			DebugSend("KADEMLIA2_PUBLISH_RES", uIP, uUDPPort);

		SendPacket(&fileIO2, KADEMLIA2_PUBLISH_RES, uIP, uUDPPort, senderUDPKey, NULL);
	} else
		delete pEntry;
}

// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::Process_KADEMLIA_FIREWALLED_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	// Verify packet is expected size
	if (uLenPacket != 2) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	uint16 uTCPPort = fileIO.ReadUInt16();

	CContact contact;
	contact.SetIPAddress(uIP);
	contact.SetTCPPort(uTCPPort);
	contact.SetUDPPort(uUDPPort);
	if (!theApp.clientlist->RequestTCP(&contact, 0))
		return; // cancelled for some reason, don't send a response

	// Send response
	CSafeMemFile fileIO2(4);
	fileIO2.WriteUInt32(uIP);
	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA_FIREWALLED_RES", uIP, uUDPPort);

	SendPacket(&fileIO2, KADEMLIA_FIREWALLED_RES, uIP, uUDPPort, senderUDPKey, NULL);
}

// Used by Kad2.0 Prot.Version 7+
void CKademliaUDPListener::Process_KADEMLIA_FIREWALLED2_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	// Verify packet is expected size
	if (uLenPacket < 19) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	uint16 uTCPPort = fileIO.ReadUInt16();
	CUInt128 userID;
	fileIO.ReadUInt128(&userID);
	uint8 byConnectOptions = fileIO.ReadUInt8();

	CContact contact;
	contact.SetIPAddress(uIP);
	contact.SetTCPPort(uTCPPort);
	contact.SetUDPPort(uUDPPort);
	contact.SetClientID(userID);
	if (!theApp.clientlist->RequestTCP(&contact, byConnectOptions))
		return;  // cancelled for some reason, don't send a response

	// Send response
	CSafeMemFile fileIO2(4);
	fileIO2.WriteUInt32(uIP);
	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA_FIREWALLED_RES", uIP, uUDPPort);

	SendPacket(&fileIO2, KADEMLIA_FIREWALLED_RES, uIP, uUDPPort, senderUDPKey, NULL);
}

// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::Process_KADEMLIA_FIREWALLED_RES(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, const CKadUDPKey& /*senderUDPKey*/)
{
	// Verify packet is expected size
	if (uLenPacket != 4) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	if (!theApp.clientlist->IsKadFirewallCheckIP(ntohl(uIP))) { /*KADEMLIA_FIREWALLED2_REQ + KADEMLIA_FIREWALLED_REQ*/
		CString strError;
		strError.Format(_T("Received unrequested firewall response packet in %hs"), __FUNCTION__);
		throw strError;
	}

	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	uint32 uFirewalledIP = fileIO.ReadUInt32();

	//Update con state only if something changes.
	if (CKademlia::GetPrefs()->GetIPAddress() != uFirewalledIP) {
		CKademlia::GetPrefs()->SetIPAddress(uFirewalledIP);
		theApp.emuledlg->ShowConnectionState();
	}
	CKademlia::GetPrefs()->IncRecheckIP();
}

// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::Process_KADEMLIA_FIREWALLED_ACK_RES(uint32 uLenPacket)
{
	// deprecated since KadVersion 7+, the result is now sent per TCP instead of UDP, because this will fail if our intern UDP port is unreachable.
	// But we want the TCP test result regardless if UDP is firewalled, the new UDP state and test takes care of the rest
	// Verify packet is expected size
	if (uLenPacket != 0) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	CKademlia::GetPrefs()->IncFirewalled();
}

// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::Process_KADEMLIA_FINDBUDDY_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
	// Verify packet is expected size
	if (uLenPacket < 34) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	if (CKademlia::GetPrefs()->GetFirewalled() || CUDPFirewallTester::IsFirewalledUDP(true) || !CUDPFirewallTester::IsVerified())
		//We are firewalled but somehow we still got this packet. Don't send a response.
		return;
	if (theApp.clientlist->GetBuddyStatus() == Connected)
		// we already have a buddy
		return;

	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 BuddyID;
	fileIO.ReadUInt128(&BuddyID);
	CUInt128 userID;
	fileIO.ReadUInt128(&userID);
	uint16 uTCPPort = fileIO.ReadUInt16();

	CContact contact;
	contact.SetIPAddress(uIP);
	contact.SetTCPPort(uTCPPort);
	contact.SetUDPPort(uUDPPort);
	contact.SetClientID(userID);
	if (!theApp.clientlist->IncomingBuddy(&contact, &BuddyID))
		return; // cancelled for some reason, don't send a response

	CSafeMemFile fileIO2(34);
	fileIO2.WriteUInt128(&BuddyID);
	fileIO2.WriteUInt128(&CKademlia::GetPrefs()->GetClientHash());
	fileIO2.WriteUInt16(thePrefs.GetPort());
	if (!senderUDPKey.IsEmpty()) // remove check for later versions
		fileIO2.WriteUInt8(CKademlia::GetPrefs()->GetMyConnectOptions(true, false)); // new since 0.49a, old mules will ignore it (hopefully ;) )
	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA_FINDBUDDY_RES", uIP, uUDPPort);

	SendPacket(&fileIO2, KADEMLIA_FINDBUDDY_RES, uIP, uUDPPort, senderUDPKey, NULL);
}

// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::Process_KADEMLIA_FINDBUDDY_RES(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 uUDPPort, const CKadUDPKey& /*senderUDPKey*/)
{
	// Verify packet is expected size
	if (uLenPacket < 34) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	if (!IsOnOutTrackList(uIP, KADEMLIA_FINDBUDDY_REQ)) {
		CString strError;
		strError.Format(_T("***NOTE: Received unrequested response packet, size (%u) in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}


	CSafeMemFile fileIO(pbyPacketData, uLenPacket);
	CUInt128 uCheck;
	fileIO.ReadUInt128(&uCheck);
	uCheck.Xor(CUInt128(true));
	if (CKademlia::GetPrefs()->GetKadID() == uCheck) {
		CUInt128 userID;
		fileIO.ReadUInt128(&userID);
		uint16 uTCPPort = fileIO.ReadUInt16();
		uint8 byConnectOptions = 0;
		if (uLenPacket > 34)
			// 0.49+ (kad version 7) sends its additional connect options so we know if to use an obfuscated connection
			byConnectOptions = fileIO.ReadUInt8();

		CContact contact;
		contact.SetIPAddress(uIP);
		contact.SetTCPPort(uTCPPort);
		contact.SetUDPPort(uUDPPort);
		contact.SetClientID(userID);

		theApp.clientlist->RequestBuddy(&contact, byConnectOptions);
	}
}

// Used by Kad1.0 and Kad2.0
void CKademliaUDPListener::Process_KADEMLIA_CALLBACK_REQ(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, const CKadUDPKey& /*senderUDPKey*/)
{
	// Verify packet is expected size
	if (uLenPacket < 34) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	CUpDownClient *pBuddy = theApp.clientlist->GetBuddy();
	if (pBuddy != NULL) {
		CSafeMemFile fileIO(pbyPacketData, uLenPacket);
		CUInt128 uCheck;
		fileIO.ReadUInt128(&uCheck);
		//JOHNTODO: Begin filtering bad buddy ID's.
		//CUInt128 bud(pBuddy->GetBuddyID());
		CUInt128 uFile;
		fileIO.ReadUInt128(&uFile);
		uint16 uTCP = fileIO.ReadUInt16();

		if (pBuddy->socket == NULL)
			throw CString(__FUNCTION__ ": Buddy has no valid socket.");
		CSafeMemFile fileIO2(uLenPacket + 6);
		fileIO2.WriteUInt128(&uCheck);
		fileIO2.WriteUInt128(&uFile);
		fileIO2.WriteUInt32(uIP);
		fileIO2.WriteUInt16(uTCP);
		Packet *pPacket = new Packet(&fileIO2, OP_EMULEPROT, OP_CALLBACK);
		if (thePrefs.GetDebugClientKadUDPLevel() > 0 || thePrefs.GetDebugClientTCPLevel() > 0)
			DebugSend("OP_CALLBACK", pBuddy);
		theStats.AddUpDataOverheadFileRequest(pPacket->size);
		pBuddy->socket->SendPacket(pPacket);
	}
}

void CKademliaUDPListener::Process_KADEMLIA2_PING(uint32 uIP, uint16 uUDPPort, const CKadUDPKey &senderUDPKey)
{
// can be used just as PING, currently it is however only used to determine ones external port
	CSafeMemFile fileIO2(2);
	fileIO2.WriteUInt16(uUDPPort);
	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA2_PONG", uIP, uUDPPort);
	SendPacket(&fileIO2, KADEMLIA2_PONG, uIP, uUDPPort, senderUDPKey, NULL);
}

void CKademliaUDPListener::Process_KADEMLIA2_PONG(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, uint16 /*uUDPPort*/, const CKadUDPKey& /*senderUDPKey*/)
{
	if (uLenPacket < 2) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	if (!IsOnOutTrackList(uIP, KADEMLIA2_PING)) {
		CString strError;
		strError.Format(_T("***NOTE: Received unrequested response packet, size (%u) in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}

	// is this one of our legacy challenge packets?
	CUInt128 uContactID;
	if (IsLegacyChallenge(CUInt128(0ul), uIP, KADEMLIA2_PING, uContactID)) {
		// yup it is, set the contact as verified
		if (!CKademlia::GetRoutingZone()->VerifyContact(uContactID, uIP))
			DebugLogWarning(_T("Kad: KADEMLIA2_PONG: Unable to find valid sender in routing table (sender: %s)"), (LPCTSTR)ipstr(htonl(uIP)));
		else
			DEBUG_ONLY(AddDebugLogLine(DLP_LOW, false, _T("Verified contact with legacy challenge (KADEMLIA2_PING) - %s"), (LPCTSTR)ipstr(htonl(uIP))));
		// we might care for its other content
	}

	if (CKademlia::GetPrefs()->FindExternKadPort(false)) {
		// the reported port doesn't always has to be our true external port, esp. if we used our intern port
		// and communicated recently with the client some routers might remember this and assign the intern port as source
		// but this shouldn't be a problem because we prefer intern ports anyway.
		// might have to be reviewed in later versions when more data is available
		CKademlia::GetPrefs()->SetExternKadPort(PeekUInt16(pbyPacketData), uIP);
		if (CUDPFirewallTester::IsFWCheckUDPRunning())
			CUDPFirewallTester::QueryNextClient();
	}
	theApp.emuledlg->ShowConnectionState();
}

void CKademliaUDPListener::Process_KADEMLIA2_FIREWALLUDP(const byte *pbyPacketData, uint32 uLenPacket, uint32 uIP, const CKadUDPKey& /*senderUDPKey*/)
{
	// Verify packet is expected size
	if (uLenPacket < 3) {
		CString strError;
		strError.Format(_T("***NOTE: Received wrong size (%u) packet in %hs"), uLenPacket, __FUNCTION__);
		throw strError;
	}
	uint8 byErrorCode = PeekUInt8(pbyPacketData);
	uint16 nIncomingPort = PeekUInt16(pbyPacketData + 1);

	if ((nIncomingPort != CKademlia::GetPrefs()->GetExternalKadPort() && nIncomingPort != CKademlia::GetPrefs()->GetInternKadPort())
		|| nIncomingPort == 0)
	{
		DebugLogWarning(_T("Received UDP FirewallCheck on unexpected incoming port %u (%s)"), nIncomingPort, (LPCTSTR)ipstr(htonl(uIP)));
		CUDPFirewallTester::SetUDPFWCheckResult(false, true, uIP, 0);
	} else if (byErrorCode == 0) {
		DebugLog(_T("Received UDP FirewallCheck packet from %s with incoming port %u"), (LPCTSTR)ipstr(htonl(uIP)), nIncomingPort);
		CUDPFirewallTester::SetUDPFWCheckResult(true, false, uIP, nIncomingPort);
	} else {
		DebugLog(_T("Received UDP FirewallCheck packet from %s with incoming port %u with remote errorcode %u - ignoring result"), (LPCTSTR)ipstr(htonl(uIP)), nIncomingPort, byErrorCode);
		CUDPFirewallTester::SetUDPFWCheckResult(false, true, uIP, 0);
	}
}

void CKademliaUDPListener::SendPacket(const byte *pbyData, uint32 uLenData, uint32 uDestinationHost, uint16 uDestinationPort, const CKadUDPKey &targetUDPKey, const CUInt128 *uCryptTargetID)
{
	if (uLenData < 2) {
		ASSERT(0);
		return;
	}
	Packet *pPacket = new Packet(OP_KADEMLIAHEADER);
	pPacket->opcode = pbyData[1];
	pPacket->pBuffer = new char[uLenData + 8];
	memcpy(pPacket->pBuffer, pbyData + 2, uLenData - 2);
	pPacket->size = uLenData - 2;
	if (uLenData > 200)
		pPacket->PackPacket();
	theStats.AddUpDataOverheadKad(pPacket->size);
	AddTrackedOutPacket(uDestinationHost, pbyData[1]);
	theApp.clientudp->SendPacket(pPacket, ntohl(uDestinationHost), uDestinationPort, true
		, (uCryptTargetID != NULL) ? uCryptTargetID->GetData() : NULL
		, true, targetUDPKey.GetKeyValue(theApp.GetPublicIP(false)));
}

void CKademliaUDPListener::SendPacket(const byte *pbyData, uint32 uLenData, byte byOpcode, uint32 uDestinationHost, uint16 uDestinationPort, const CKadUDPKey &targetUDPKey, const CUInt128 *uCryptTargetID)
{
	Packet *pPacket = new Packet(OP_KADEMLIAHEADER);
	pPacket->opcode = byOpcode;
	pPacket->pBuffer = new char[uLenData];
	memcpy(pPacket->pBuffer, pbyData, uLenData);
	pPacket->size = uLenData;
	if (uLenData > 200)
		pPacket->PackPacket();
	theStats.AddUpDataOverheadKad(pPacket->size);
	AddTrackedOutPacket(uDestinationHost, byOpcode);
	theApp.clientudp->SendPacket(pPacket, ntohl(uDestinationHost), uDestinationPort, true
		, (uCryptTargetID != NULL) ? uCryptTargetID->GetData() : NULL
		, true, targetUDPKey.GetKeyValue(theApp.GetPublicIP(false)));
}

void CKademliaUDPListener::SendPacket(CSafeMemFile *pbyData, byte byOpcode, uint32 uDestinationHost, uint16 uDestinationPort, const CKadUDPKey &targetUDPKey, const CUInt128 *uCryptTargetID)
{
	Packet *pPacket = new Packet(pbyData, OP_KADEMLIAHEADER);
	pPacket->opcode = byOpcode;
	if (pPacket->size > 200)
		pPacket->PackPacket();
	theStats.AddUpDataOverheadKad(pPacket->size);
	AddTrackedOutPacket(uDestinationHost, byOpcode);
	theApp.clientudp->SendPacket(pPacket, htonl(uDestinationHost), uDestinationPort, true
		, (uCryptTargetID != NULL) ? uCryptTargetID->GetData() : NULL
		, true, targetUDPKey.GetKeyValue(theApp.GetPublicIP(false)));
}

bool CKademliaUDPListener::FindNodeIDByIP(CKadClientSearcher *pRequester, uint32 dwIP, uint16 nTCPPort, uint16 nUDPPort)
{
	// send a hello packet to the given IP in order to get a HELLO_RES with the NodeID

	// we will drop support for Kad1 soon, so don't bother sending two packets in case we don't know if kad2 is supported
	// (if we know that it's not, this function isn't called in the first place)
	DebugLog(_T("FindNodeIDByIP: Requesting NodeID from %s by sending KADEMLIA2_HELLO_REQ"), (LPCTSTR)ipstr(htonl(dwIP)));
	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA2_HELLO_REQ", dwIP, nUDPPort);
	SendMyDetails(KADEMLIA2_HELLO_REQ, dwIP, nUDPPort, KADEMLIA_VERSION, CKadUDPKey(), NULL, false); // todo: we send this unobfuscated, which is not perfect, see if this can be avoided in the future
	FetchNodeID_Struct sRequest = {dwIP, nTCPPort, ::GetTickCount() + SEC2MS(60), pRequester};
	listFetchNodeIDRequests.AddTail(sRequest);
	return true;
}

void CKademliaUDPListener::ExpireClientSearch(CKadClientSearcher *pExpireImmediately)
{
	for (POSITION pos = listFetchNodeIDRequests.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		const FetchNodeID_Struct &sRequest = listFetchNodeIDRequests.GetNext(pos);
		if (sRequest.pRequester == pExpireImmediately)
			listFetchNodeIDRequests.RemoveAt(pos2);
		else if (::GetTickCount() >= sRequest.dwExpire) {
			sRequest.pRequester->KadSearchNodeIDByIPResult(KCSR_TIMEOUT, NULL);
			listFetchNodeIDRequests.RemoveAt(pos2);
		}
	}
}

void CKademliaUDPListener::SendLegacyChallenge(uint32 uIP, uint16 uUDPPort, const CUInt128 &uContactID)
{
	// We want to verify that a pre-0.49a contact is valid and not sent from a spoofed IP.
	// Because those versions don't support any direct validating, we sent a KAD_REQ with a random ID,
	// which is our challenge. If we receive an answer packet for this request, we can be sure the
	// contact is not spoofed
#ifdef _DEBUG
	CContact *pContact = CKademlia::GetRoutingZone()->GetContact(uContactID);
	if (pContact != NULL) {
		if (pContact->GetType() < 2)
			DebugLogWarning(_T("Process_KADEMLIA_HELLO_RES: Sending challenge to a long known contact (should be verified already) - %s"), (LPCTSTR)ipstr(htonl(uIP)));
	} else
		ASSERT(0);
#endif
	if (HasActiveLegacyChallenge(uIP)) // don't sent more than one challenge at a time
		return;
	CSafeMemFile fileIO(33);
	fileIO.WriteUInt8(KADEMLIA_FIND_VALUE);
	CUInt128 uChallenge;
	uChallenge.SetValueRandom();
	if (uChallenge == 0) {
		// hey there is a 2^128 chance that this happens ;)
		ASSERT(0);
		uChallenge = 1;
	}
	// Put the target we want into the packet. This is our challenge
	fileIO.WriteUInt128(&uChallenge);
	// Add the ID of the contact we are contacting for sanity checks on the other end.
	fileIO.WriteUInt128(&uContactID);
	// those versions we send those requests to don't support encryption / obfuscation
	CKademlia::GetUDPListener()->SendPacket(&fileIO, KADEMLIA2_REQ, uIP, uUDPPort, CKadUDPKey(), NULL);
	if (thePrefs.GetDebugClientKadUDPLevel() > 0)
		DebugSend("KADEMLIA2_REQ(SendLegacyChallenge)", uIP, uUDPPort);
	AddLegacyChallenge(uContactID, uChallenge, uIP, KADEMLIA2_REQ);
}