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
#include "clientlist.h"
#include "emule.h"
#include "Log.h"
#include "opcodes.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/net/PacketTracking.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

using namespace Kademlia;

CPacketTracking::CPacketTracking()
	: dwLastTrackInCleanup()
{
}

CPacketTracking::~CPacketTracking()
{
	m_mapTrackPacketsIn.RemoveAll();
	while (!m_liTrackPacketsIn.IsEmpty())
		delete m_liTrackPacketsIn.RemoveHead();
}

void CPacketTracking::AddTrackedOutPacket(uint32 dwIP, uint8 byOpcode)
{
	// this tracklist tacks _outgoing_ request packets, to make sure incoming answer packets were requested
	// only track packets which we actually check for later
	if (!IsTrackedOutListRequestPacket(byOpcode))
		return;
	const uint32 tick = ::GetTickCount();
	listTrackedRequests.AddHead(TrackPackets_Struct{tick, dwIP, byOpcode});
	while (!listTrackedRequests.IsEmpty() && tick >= listTrackedRequests.GetTail().dwInserted + SEC2MS(180))
		listTrackedRequests.RemoveTail();
}

bool CPacketTracking::IsTrackedOutListRequestPacket(uint8 byOpcode)
{
	switch (byOpcode) {
	case KADEMLIA2_BOOTSTRAP_REQ:
	case KADEMLIA2_HELLO_REQ:
	case KADEMLIA2_HELLO_RES:
	case KADEMLIA2_REQ:
	case KADEMLIA_SEARCH_NOTES_REQ:
	case KADEMLIA2_SEARCH_NOTES_REQ:
	case KADEMLIA_PUBLISH_REQ:
	case KADEMLIA2_PUBLISH_KEY_REQ:
	case KADEMLIA2_PUBLISH_SOURCE_REQ:
	case KADEMLIA2_PUBLISH_NOTES_REQ:
	case KADEMLIA_FINDBUDDY_REQ:
	case KADEMLIA_CALLBACK_REQ:
	case KADEMLIA2_PING:
		return true;
	}
	return false;
}

bool CPacketTracking::IsOnOutTrackList(uint32 dwIP, uint8 byOpcode, bool bDontRemove)
{
#ifdef _DEBUG
	if (!IsTrackedOutListRequestPacket(byOpcode))
		ASSERT(0); // code error / bug
#endif
	const uint32 tick = ::GetTickCount();
	for (POSITION pos = listTrackedRequests.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		const TrackPackets_Struct &req = listTrackedRequests.GetNext(pos);
		if (tick >= req.dwInserted + SEC2MS(180))
			break;
		if (req.dwIP == dwIP && req.byOpcode == byOpcode) {
			if (!bDontRemove)
				listTrackedRequests.RemoveAt(pos2);
			return true;
		}
	}
	return false;
}

//return codes: 0 - no error; 1 - flood; 2 - massive flood
int CPacketTracking::InTrackListIsAllowedPacket(uint32 uIP, uint8 byOpcode, bool /*bValidSenderkey*/)
{
	// this tracklist monitors _incoming_ request packets and acts as a general flood protection
	// by dropping too frequent requests from a single IP, thus avoiding response floods, saving CPU time,
	// preventing DOS attacks and slowing down other possible attacks/behavior
	// (scanning indexed files, fake publish floods, etc)

	// first figure out if this is a request packet to be tracked and its time limits
	// time limits are chosen by estimating the max. frequency of such packets in normal operation (+ buffer)
	// (those limits are not meant be fine for normal usage, but only supposed to be a flood detection)
	uint32 iAllowedPacketsPerMinute;
	const byte byDbgOrgOpcode = byOpcode;
	switch (byOpcode) {
	case KADEMLIA2_BOOTSTRAP_REQ:
		iAllowedPacketsPerMinute = 2;
		break;
	case KADEMLIA2_HELLO_REQ:
		iAllowedPacketsPerMinute = 3;
		break;
	case KADEMLIA2_REQ:
		iAllowedPacketsPerMinute = 10;
		break;
	case KADEMLIA2_SEARCH_NOTES_REQ:
		iAllowedPacketsPerMinute = 3;
		break;
	case KADEMLIA2_SEARCH_KEY_REQ:
		iAllowedPacketsPerMinute = 3;
		break;
	case KADEMLIA2_SEARCH_SOURCE_REQ:
		iAllowedPacketsPerMinute = 3;
		break;
	case KADEMLIA2_PUBLISH_KEY_REQ:
		iAllowedPacketsPerMinute = 4;
		break;
	case KADEMLIA2_PUBLISH_SOURCE_REQ:
		iAllowedPacketsPerMinute = 3;
		break;
	case KADEMLIA2_PUBLISH_NOTES_REQ:
		iAllowedPacketsPerMinute = 2;
		break;
	case KADEMLIA_FIREWALLED2_REQ:
		byOpcode = KADEMLIA_FIREWALLED_REQ;
	case KADEMLIA_FIREWALLED_REQ:
		iAllowedPacketsPerMinute = 2;
		break;
	case KADEMLIA_FINDBUDDY_REQ:
		iAllowedPacketsPerMinute = 2;
		break;
	case KADEMLIA_CALLBACK_REQ:
		iAllowedPacketsPerMinute = 1;
		break;
	case KADEMLIA2_PING:
		iAllowedPacketsPerMinute = 2;
		break;
	default:
		// not a request packets, so it's a response - no further checks at this point
		return 0;
	}
	const uint32 milliSecsPerPacket = MIN2MS(1) / iAllowedPacketsPerMinute;
	const uint32 dwCurrentTick = ::GetTickCount();
	// time for cleaning up?
	if (dwCurrentTick >= dwLastTrackInCleanup + MIN2MS(12))
		InTrackListCleanup();

	// check for existing entries
	TrackPacketsIn_Struct *pTrackEntry;
	if (!m_mapTrackPacketsIn.Lookup(uIP, pTrackEntry)) {
		pTrackEntry = new TrackPacketsIn_Struct();
		pTrackEntry->m_uIP = uIP;
		m_mapTrackPacketsIn.SetAt(uIP, pTrackEntry);
		m_liTrackPacketsIn.AddHead(pTrackEntry);
	}

	// search specific request tracks
	for (int i = 0; i < pTrackEntry->m_aTrackedRequests.GetCount(); ++i) {
		if (pTrackEntry->m_aTrackedRequests[i].m_byOpcode == byOpcode) {
			// already tracked requests with this opcode, remove expired request counts
			TrackPacketsIn_Struct::TrackedRequestIn_Struct &rCurTrackedRequest = pTrackEntry->m_aTrackedRequests[i];
			if (rCurTrackedRequest.m_nCount > 0
				&& dwCurrentTick >= rCurTrackedRequest.m_dwFirstAdded + milliSecsPerPacket + 500) //allow small extra time
			{
				uint32 nRemoveCount = (dwCurrentTick - rCurTrackedRequest.m_dwFirstAdded - 500) / milliSecsPerPacket;
				if (nRemoveCount >= rCurTrackedRequest.m_nCount) {
					rCurTrackedRequest.m_nCount = 0;
					rCurTrackedRequest.m_dwFirstAdded = dwCurrentTick; // for the packet we just process
				} else {
					rCurTrackedRequest.m_nCount -= nRemoveCount;
					rCurTrackedRequest.m_dwFirstAdded += milliSecsPerPacket * nRemoveCount;
				}
			}
			// we increase the counter in any case, even if we drop the packet later
			++rCurTrackedRequest.m_nCount;
			// remember only for easier cleanup
			pTrackEntry->m_dwLastExpire = max(pTrackEntry->m_dwLastExpire, rCurTrackedRequest.m_dwFirstAdded + milliSecsPerPacket * rCurTrackedRequest.m_nCount);

			if (CKademlia::IsRunningInLANMode() && IsLANIP(ntohl(uIP))) // no flood detection in LanMode
				return 0;

			// now the actual check if this request is allowed
			if (rCurTrackedRequest.m_nCount > iAllowedPacketsPerMinute * 4) {
				// this is so far above the limit that it has to be an intentional flood / misuse in any case
				// so we take the next higher punishment and ban the IP
				DebugLogWarning(_T("Kad: Massive request flood detected for opcode 0x%X (0x%X) from IP %s - Banning IP"), byOpcode, byDbgOrgOpcode, (LPCTSTR)ipstr(htonl(uIP)));
				theApp.clientlist->AddBannedClient(ntohl(uIP));
				return 2; // drop the packet, remove the contact from routing
			}
			if (rCurTrackedRequest.m_nCount > iAllowedPacketsPerMinute) {
				// over the limit, drop the packet but do nothing else
				if (!rCurTrackedRequest.m_bDbgLogged) {
					rCurTrackedRequest.m_bDbgLogged = true;
					DebugLog(_T("Kad: Request flood detected for opcode 0x%X (0x%X) from IP %s - Dropping packets with this opcode"), byOpcode, byDbgOrgOpcode, (LPCTSTR)ipstr(htonl(uIP)));
				}
				return 1; // drop the packet
			}
			rCurTrackedRequest.m_bDbgLogged = false;
			return 0;
		}
	}

	// add a new entry for this request, no checks needed since 1 is always OK
	TrackPacketsIn_Struct::TrackedRequestIn_Struct curTrackedRequest;
	curTrackedRequest.m_byOpcode = byOpcode;
	curTrackedRequest.m_bDbgLogged = false;
	curTrackedRequest.m_dwFirstAdded = dwCurrentTick;
	curTrackedRequest.m_nCount = 1;
	// remember only for easier cleanup
	pTrackEntry->m_dwLastExpire = max(pTrackEntry->m_dwLastExpire, dwCurrentTick + milliSecsPerPacket);
	pTrackEntry->m_aTrackedRequests.Add(curTrackedRequest);
	return 0;
}

void CPacketTracking::InTrackListCleanup()
{
	const uint32 dwCurrentTick = ::GetTickCount();
	const INT_PTR dbgOldSize = m_liTrackPacketsIn.GetCount();
	dwLastTrackInCleanup = dwCurrentTick;
	for (POSITION pos = m_liTrackPacketsIn.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		const TrackPacketsIn_Struct *curEntry = m_liTrackPacketsIn.GetNext(pos);
		if (dwCurrentTick >= curEntry->m_dwLastExpire) {
			VERIFY(m_mapTrackPacketsIn.RemoveKey(curEntry->m_uIP));
			m_liTrackPacketsIn.RemoveAt(pos2);
			delete curEntry;
		}
	}
	DebugLog(_T("Cleaned up Kad Incoming Requests Tracklist, entries before: %u, after %u"), (uint32)dbgOldSize, (uint32)m_liTrackPacketsIn.GetCount());
}

void CPacketTracking::AddLegacyChallenge(const CUInt128 &uContactID, const CUInt128 &uChallengeID, uint32 uIP, uint8 byOpcode)
{
	const uint32 tick = ::GetTickCount();
	listChallengeRequests.AddHead(TrackChallenge_Struct{tick, uContactID, uChallengeID, uIP, byOpcode});
	while (!listChallengeRequests.IsEmpty() && tick >= listChallengeRequests.GetTail().dwInserted + SEC2MS(180)) {
		DEBUG_ONLY(DebugLog(_T("Challenge timed out, client not verified - %s"), (LPCTSTR)ipstr(htonl(listChallengeRequests.GetTail().uIP))));
		listChallengeRequests.RemoveTail();
	}
}

bool CPacketTracking::IsLegacyChallenge(const CUInt128 &uChallengeID, uint32 uIP, uint8 byOpcode, CUInt128 &ruContactID)
{
	bool bDbgWarning = false;
	const uint32 tick = ::GetTickCount();
	for (POSITION pos = listChallengeRequests.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		const TrackChallenge_Struct &tc = listChallengeRequests.GetNext(pos);
		if (tick >= tc.dwInserted + SEC2MS(180))
			break;
		if (tc.uIP == uIP && tc.byOpcode == byOpcode) {
			ASSERT(tc.uChallenge != 0 || byOpcode == KADEMLIA2_PING);
			if (tc.uChallenge == 0 || tc.uChallenge == uChallengeID) {
				ruContactID = tc.uContactID;
				listChallengeRequests.RemoveAt(pos2);
				return true;
			}
			bDbgWarning = true;
		}
	}
	if (bDbgWarning)
		DebugLogWarning(_T("Kad: IsLegacyChallenge: Wrong challenge answer received, client not verified (%s)"), (LPCTSTR)ipstr(htonl(uIP)));
	return false;
}

bool CPacketTracking::HasActiveLegacyChallenge(uint32 uIP) const
{
	const uint32 tick = ::GetTickCount();
	for (POSITION pos = listChallengeRequests.GetHeadPosition(); pos != NULL;) {
		const TrackChallenge_Struct &tcstruct = listChallengeRequests.GetNext(pos);
		if (tick >= tcstruct.dwInserted + SEC2MS(180))
			break;
		if (tcstruct.uIP == uIP)
			return true;
	}
	return false;
}