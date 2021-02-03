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
#include "ClientUDPSocket.h"
#include "Packets.h"
#include "DownloadQueue.h"
#include "Statistics.h"
#include "PartFile.h"
#include "SharedFileList.h"
#include "UploadQueue.h"
#include "UpDownClient.h"
#include "Preferences.h"
#include "ClientList.h"
#include "EncryptedDatagramSocket.h"
#include "IPFilter.h"
#include "Listensocket.h"
#include "Log.h"
#include "OtherFunctions.h"
#include "SafeFile.h"
#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/UDPFirewallTester.h"
#include "kademlia/net/KademliaUDPListener.h"
#include "kademlia/io/IOException.h"
#include "kademlia/kademlia/prefs.h"
#include "kademlia/utils/KadUDPKey.h"
#include "zlib/zlib.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// CClientUDPSocket

CClientUDPSocket::CClientUDPSocket()
{
	m_bWouldBlock = false;
	m_port = 0;
}

CClientUDPSocket::~CClientUDPSocket()
{
	theApp.uploadBandwidthThrottler->RemoveFromAllQueuesLocked(this); // ZZ:UploadBandWithThrottler (UDP)
	while (!controlpacket_queue.IsEmpty()) {
		const UDPPack *p = controlpacket_queue.RemoveHead();
		delete p->packet;
		delete p;
	}
}

void CClientUDPSocket::OnReceive(int nErrorCode)
{
	if (nErrorCode) {
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Error: Client UDP socket, error on receive event: %s"), (LPCTSTR)GetErrorMessage(nErrorCode, 1));
	}

	BYTE buffer[8192]; //5000 was too low sometimes
	SOCKADDR_IN sockAddr = {};
	int iSockAddrLen = sizeof sockAddr;
	int nRealLen = ReceiveFrom(buffer, sizeof buffer, (LPSOCKADDR)&sockAddr, &iSockAddrLen);
	if (!(theApp.ipfilter->IsFiltered(sockAddr.sin_addr.s_addr) || theApp.clientlist->IsBannedClient(sockAddr.sin_addr.s_addr))) {
		BYTE *pBuffer;
		uint32 nReceiverVerifyKey;
		uint32 nSenderVerifyKey;
		int nPacketLen = DecryptReceivedClient(buffer, nRealLen, &pBuffer, sockAddr.sin_addr.s_addr, &nReceiverVerifyKey, &nSenderVerifyKey);
		if (nPacketLen >= 1) {
			CString strError;
			try {
				switch (pBuffer[0]) {
				case OP_EMULEPROT:
					if (nPacketLen < 2)
						throw CString(_T("eMule packet too short"));
					ProcessPacket(pBuffer + 2, nPacketLen - 2, pBuffer[1], sockAddr.sin_addr.s_addr, ntohs(sockAddr.sin_port));
					break;
				case OP_KADEMLIAPACKEDPROT:
					{
						theStats.AddDownDataOverheadKad(nPacketLen);
						if (nPacketLen < 2)
							throw CString(_T("Kad packet (compressed) too short"));
						uint32 nNewSize = nPacketLen * 10 + 300;
						BYTE *unpack = NULL;
						uLongf unpackedsize = 0;
						int iZLibResult = Z_OK;
						do {
							delete[] unpack;
							unpack = new BYTE[nNewSize];
							unpackedsize = nNewSize - 2;
							iZLibResult = uncompress(unpack + 2, &unpackedsize, pBuffer + 2, nPacketLen - 2);
							nNewSize *= 2; // size for the next try if needed
						} while (iZLibResult == Z_BUF_ERROR && nNewSize < 250000);

						if (iZLibResult != Z_OK) {
							delete[] unpack;
							strError.Format(_T("Failed to uncompress Kad packet: zip error: %d (%hs)"), iZLibResult, zError(iZLibResult));
							throw strError;
						}

						unpack[0] = OP_KADEMLIAHEADER;
						unpack[1] = pBuffer[1];
						try {
							Kademlia::CKademlia::ProcessPacket(unpack, unpackedsize + 2, ntohl(sockAddr.sin_addr.s_addr), ntohs(sockAddr.sin_port)
								, (Kademlia::CPrefs::GetUDPVerifyKey(sockAddr.sin_addr.s_addr) == nReceiverVerifyKey)
								, Kademlia::CKadUDPKey(nSenderVerifyKey, theApp.GetPublicIP(false)));
						} catch (...) {
							delete[] unpack;
							throw;
						}
						delete[] unpack;
					}
					break;
				case OP_KADEMLIAHEADER:
					theStats.AddDownDataOverheadKad(nPacketLen);
					if (nPacketLen < 2)
						throw CString(_T("Kad packet too short"));
					Kademlia::CKademlia::ProcessPacket(pBuffer, nPacketLen, ntohl(sockAddr.sin_addr.s_addr), ntohs(sockAddr.sin_port)
						, (Kademlia::CPrefs::GetUDPVerifyKey(sockAddr.sin_addr.s_addr) == nReceiverVerifyKey)
						, Kademlia::CKadUDPKey(nSenderVerifyKey, theApp.GetPublicIP(false)));

					break;
				default:
					strError.Format(_T("Unknown protocol 0x%02x"), pBuffer[0]);
					throw strError;
				}
			} catch (CFileException *error) {
				error->Delete();
				strError = _T("Invalid packet received");
			} catch (CMemoryException *error) {
				error->Delete();
				strError = _T("Memory exception");
			} catch (const CString &error) {
				strError = error;
			} catch (Kademlia::CIOException *error) {
				error->Delete();
				strError = _T("Invalid packet received");
			} catch (CException *error) {
				error->Delete();
				strError = _T("General packet error");
			}
#ifndef _DEBUG
			catch (...) {
				strError = _T("Unknown exception");
				ASSERT(0);
			}
#endif
			if (thePrefs.GetVerbose() && !strError.IsEmpty()) {
				CString strClientInfo;
				CUpDownClient *client;
				if (pBuffer[0] == OP_EMULEPROT)
					client = theApp.clientlist->FindClientByIP_UDP(sockAddr.sin_addr.s_addr, ntohs(sockAddr.sin_port));
				else
					client = theApp.clientlist->FindClientByIP_KadPort(sockAddr.sin_addr.s_addr, ntohs(sockAddr.sin_port));
				if (client)
					strClientInfo = client->DbgGetClientInfo();
				else
					strClientInfo.Format(_T("%s:%hu"), (LPCTSTR)ipstr(sockAddr.sin_addr), ntohs(sockAddr.sin_port));

				DebugLogWarning(_T("Client UDP socket: prot=0x%02x  opcode=0x%02x  sizeaftercrypt=%u realsize=%u  %s: %s"), pBuffer[0], pBuffer[1], nPacketLen, nRealLen, (LPCTSTR)strError, (LPCTSTR)strClientInfo);
			}
		} else if (nPacketLen == SOCKET_ERROR) {
			DWORD dwError = WSAGetLastError();
			if (dwError == WSAECONNRESET) {
				// Depending on local and remote OS and depending on used local (remote?) router we may receive
				// WSAECONNRESET errors. According some KB articles, this is a special way of winsock to report
				// that a sent UDP packet was not received by the remote host because it was not listening on
				// the specified port -> no eMule running there.
				//
				// TODO: So, actually we should do something with this information and drop the related Kad node
				// or eMule client...
				;
			}
			if (thePrefs.GetVerbose() && dwError != WSAECONNRESET) {
				CString strClientInfo;
				if (iSockAddrLen > 0 && sockAddr.sin_addr.s_addr != 0 && sockAddr.sin_addr.s_addr != INADDR_NONE)
					strClientInfo.Format(_T(" from %s:%u"), (LPCTSTR)ipstr(sockAddr.sin_addr), ntohs(sockAddr.sin_port));
				DebugLogError(_T("Error: Client UDP socket, failed to receive data%s: %s"), (LPCTSTR)strClientInfo, (LPCTSTR)GetErrorMessage(dwError, 1));
			}
		}
	}
}

bool CClientUDPSocket::ProcessPacket(const BYTE *packet, UINT size, uint8 opcode, uint32 ip, uint16 port)
{
	switch (opcode) {
	case OP_REASKCALLBACKUDP:
		{
			if (thePrefs.GetDebugClientUDPLevel() > 0)
				DebugRecv("OP_ReaskCallbackUDP", NULL, NULL, ip);
			theStats.AddDownDataOverheadOther(size);
			CUpDownClient *buddy = theApp.clientlist->GetBuddy();
			if (buddy) {
				if (size < 17 || buddy->socket == NULL)
					break;
				if (md4equ(packet, buddy->GetBuddyID())) {
					PokeUInt32(const_cast<BYTE*>(packet) + 10, ip);
					PokeUInt16(const_cast<BYTE*>(packet) + 14, port);
					Packet *response = new Packet(OP_EMULEPROT);
					response->opcode = OP_REASKCALLBACKTCP;
					response->pBuffer = new char[size];
					memcpy(response->pBuffer, packet + 10, size - 10);
					response->size = size - 10;
					if (thePrefs.GetDebugClientTCPLevel() > 0)
						DebugSend("OP_ReaskCallbackTCP", buddy);
					theStats.AddUpDataOverheadFileRequest(response->size);
					buddy->SendPacket(response);
				}
			}
		}
		break;
	case OP_REASKFILEPING:
		{
			theStats.AddDownDataOverheadFileRequest(size);
			CSafeMemFile data_in(packet, size);
			uchar reqfilehash[16];
			data_in.ReadHash16(reqfilehash);
			CKnownFile *reqfile = theApp.sharedfiles->GetFileByID(reqfilehash);

			bool bSenderMultipleIpUnknown = false;
			CUpDownClient *sender = theApp.uploadqueue->GetWaitingClientByIP_UDP(ip, port, true, &bSenderMultipleIpUnknown);
			if (!reqfile) {
				if (thePrefs.GetDebugClientUDPLevel() > 0) {
					DebugRecv("OP_ReaskFilePing", NULL, reqfilehash, ip);
					DebugSend("OP_FileNotFound", NULL);
				}

				Packet *response = new Packet(OP_FILENOTFOUND, 0, OP_EMULEPROT);
				theStats.AddUpDataOverheadFileRequest(response->size);
				if (sender != NULL)
					SendPacket(response, ip, port, sender->ShouldReceiveCryptUDPPackets(), sender->GetUserHash(), false, 0);
				else
					SendPacket(response, ip, port, false, NULL, false, 0);
				break;
			}
			if (sender) {
				if (thePrefs.GetDebugClientUDPLevel() > 0)
					DebugRecv("OP_ReaskFilePing", sender, reqfilehash);

				//Make sure we are still thinking about the same file
				if (md4equ(reqfilehash, sender->GetUploadFileID())) {
					sender->AddAskedCount();
					sender->SetLastUpRequest();
					//I messed up when I first added extended info to UDP
					//I should have originally used the entire ProcessExtendedInfo the first time.
					//So now I am forced to check UDPVersion to see if we are sending all the extended info.
					//From now on, we should not have to change anything here if we change
					//something in the extended info data as this will be taken care of in ProcessExtendedInfo()
					//Update extended info.
					if (sender->GetUDPVersion() > 3)
						sender->ProcessExtendedInfo(&data_in, reqfile);

					//Update our complete source counts.
					else if (sender->GetUDPVersion() > 2) {
						uint16 nCompleteCountLast = sender->GetUpCompleteSourcesCount();
						uint16 nCompleteCountNew = data_in.ReadUInt16();
						sender->SetUpCompleteSourcesCount(nCompleteCountNew);
						if (nCompleteCountLast != nCompleteCountNew)
							reqfile->UpdatePartsInfo();
					}
					CSafeMemFile data_out(128);
					if (sender->GetUDPVersion() > 3) {
						if (reqfile->IsPartFile())
							static_cast<CPartFile*>(reqfile)->WritePartStatus(&data_out);
						else
							data_out.WriteUInt16(0);
					}
					data_out.WriteUInt16((uint16)(theApp.uploadqueue->GetWaitingPosition(sender)));
					if (thePrefs.GetDebugClientUDPLevel() > 0)
						DebugSend("OP_ReaskAck", sender);
					Packet *response = new Packet(&data_out, OP_EMULEPROT);
					response->opcode = OP_REASKACK;
					theStats.AddUpDataOverheadFileRequest(response->size);
					SendPacket(response, ip, port, sender->ShouldReceiveCryptUDPPackets(), sender->GetUserHash(), false, 0);
				} else {
					DebugLogError(_T("Client UDP socket; ReaskFilePing; reqfile does not match"));
					TRACE(_T("m_reqfile:         %s\n"), (LPCTSTR)DbgGetFileInfo(reqfile->GetFileHash()));
					TRACE(_T("sender->GetRequestFile(): %s\n"), sender->GetRequestFile() ? (LPCTSTR)DbgGetFileInfo(sender->GetRequestFile()->GetFileHash()) : _T("(null)"));
				}
			} else {
				if (thePrefs.GetDebugClientUDPLevel() > 0)
					DebugRecv("OP_ReaskFilePing", NULL, reqfilehash, ip);
				// Don't answer him. We probably have him on our queue already, but can't locate him. Force him to establish a TCP connection
				if (!bSenderMultipleIpUnknown) {
					if (theApp.uploadqueue->GetWaitingUserCount() + 50 > thePrefs.GetQueueSize()) {
						if (thePrefs.GetDebugClientUDPLevel() > 0)
							DebugSend("OP_QueueFull", NULL);
						Packet *response = new Packet(OP_QUEUEFULL, 0, OP_EMULEPROT);
						theStats.AddUpDataOverheadFileRequest(response->size);
						SendPacket(response, ip, port, false, NULL, false, 0); // we cannot answer this one encrypted since we don't know this client
					}
				} else
					DebugLogWarning(_T("UDP Packet received - multiple clients with the same IP but different UDP port found. Possible UDP Port mapping problem, enforcing TCP connection. IP: %s, Port: %u"), (LPCTSTR)ipstr(ip), port);
			}
		}
		break;
	case OP_QUEUEFULL:
		{
			theStats.AddDownDataOverheadFileRequest(size);
			CUpDownClient *sender = theApp.downloadqueue->GetDownloadClientByIP_UDP(ip, port, true);
			if (thePrefs.GetDebugClientUDPLevel() > 0)
				DebugRecv("OP_QueueFull", sender, NULL, ip);
			if (sender && sender->UDPPacketPending()) {
				sender->SetRemoteQueueFull(true);
				sender->UDPReaskACK(0);
			} else if (sender != NULL)
				DebugLogError(_T("Received UDP Packet (OP_QUEUEFULL) which was not requested (pendingflag == false); Ignored packet - %s"), (LPCTSTR)sender->DbgGetClientInfo());
		}
		break;
	case OP_REASKACK:
		{
			theStats.AddDownDataOverheadFileRequest(size);
			CUpDownClient *sender = theApp.downloadqueue->GetDownloadClientByIP_UDP(ip, port, true);
			if (thePrefs.GetDebugClientUDPLevel() > 0)
				DebugRecv("OP_ReaskAck", sender, NULL, ip);
			if (sender && sender->UDPPacketPending()) {
				CSafeMemFile data_in(packet, size);
				if (sender->GetUDPVersion() > 3)
					sender->ProcessFileStatus(true, &data_in, sender->GetRequestFile());

				uint16 nRank = data_in.ReadUInt16();
				sender->SetRemoteQueueFull(false);
				sender->UDPReaskACK(nRank);
				sender->AddAskedCountDown();
			} else if (sender != NULL)
				DebugLogError(_T("Received UDP Packet (OP_REASKACK) which was not requested (pendingflag == false); Ignored packet - %s"), (LPCTSTR)sender->DbgGetClientInfo());
		}
		break;
	case OP_FILENOTFOUND:
		{
			theStats.AddDownDataOverheadFileRequest(size);
			CUpDownClient *sender = theApp.downloadqueue->GetDownloadClientByIP_UDP(ip, port, true);
			if (thePrefs.GetDebugClientUDPLevel() > 0)
				DebugRecv("OP_FileNotFound", sender, NULL, ip);
			if (sender != NULL)
				if (sender->UDPPacketPending())
					sender->UDPReaskFNF(); // may delete 'sender'!
				else
					DebugLogError(_T("Received UDP Packet (OP_FILENOTFOUND) which was not requested (pendingflag == false); Ignored packet - %s"), (LPCTSTR)sender->DbgGetClientInfo());

			break;
		}
	case OP_PORTTEST:
		if (thePrefs.GetDebugClientUDPLevel() > 0)
			DebugRecv("OP_PortTest", NULL, NULL, ip);
		theStats.AddDownDataOverheadOther(size);
		if (size == 1 && packet[0] == 0x12) {
			bool ret = theApp.listensocket->SendPortTestReply('1', true);
			AddDebugLogLine(true, _T("UDP Port check packet arrived - ACK sent back (status=%i)"), ret);
		}
		break;
	case OP_DIRECTCALLBACKREQ:
		{
			if (thePrefs.GetDebugClientUDPLevel() > 0)
				DebugRecv("OP_DIRECTCALLBACKREQ", NULL, NULL, ip);
			if (!theApp.clientlist->AllowCalbackRequest(ip)) {
				DebugLogWarning(_T("Ignored DirectCallback Request because this IP (%s) has sent too many request within a short time"), (LPCTSTR)ipstr(ip));
				break;
			}
			// do we accept callback requests at all?
			if (Kademlia::CKademlia::IsRunning() && Kademlia::CKademlia::IsFirewalled()) {
				theApp.clientlist->AddTrackCallbackRequests(ip);
				CSafeMemFile data(packet, size);
				uint16 nRemoteTCPPort = data.ReadUInt16();
				uchar uchUserHash[16];
				data.ReadHash16(uchUserHash);
				uint8 byConnectOptions = data.ReadUInt8();
				CUpDownClient *pRequester = theApp.clientlist->FindClientByUserHash(uchUserHash, ip, nRemoteTCPPort);
				if (pRequester == NULL) {
					pRequester = new CUpDownClient(NULL, nRemoteTCPPort, ip, 0, 0, true);
					pRequester->SetUserHash(uchUserHash);
					theApp.clientlist->AddClient(pRequester);
				} else {
					pRequester->SetConnectIP(ip);
					pRequester->SetUserPort(nRemoteTCPPort);
				}
				pRequester->SetConnectOptions(byConnectOptions, true, false);
				pRequester->SetDirectUDPCallbackSupport(false);
				DEBUG_ONLY(DebugLog(_T("Accepting incoming DirectCallbackRequest from %s"), (LPCTSTR)pRequester->DbgGetClientInfo()));
				pRequester->TryToConnect();
			} else
				DebugLogWarning(_T("Ignored DirectCallback Request because we do not accept DirectCall backs at all (%s)"), (LPCTSTR)ipstr(ip));
		}
		break;
	default:
		theStats.AddDownDataOverheadOther(size);
		if (thePrefs.GetDebugClientUDPLevel() > 0) {
			CUpDownClient *sender = theApp.downloadqueue->GetDownloadClientByIP_UDP(ip, port, true);
			Debug(_T("Unknown client UDP packet: host=%s:%u (%s) opcode=0x%02x  size=%u\n"), (LPCTSTR)ipstr(ip), port, sender ? (LPCTSTR)sender->DbgGetClientInfo() : _T(""), opcode, size);
		}
		return false;
	}
	return true;
}

void CClientUDPSocket::OnSend(int nErrorCode)
{
	if (nErrorCode) {
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Error: Client UDP socket, error on send event: %s"), (LPCTSTR)GetErrorMessage(nErrorCode, 1));
		return;
	}

// ZZ:UploadBandWithThrottler (UDP) -->
	sendLocker.Lock();
	m_bWouldBlock = false;

	if (!controlpacket_queue.IsEmpty())
		theApp.uploadBandwidthThrottler->QueueForSendingControlPacket(this);
	sendLocker.Unlock();
// <-- ZZ:UploadBandWithThrottler (UDP)
}

SocketSentBytes CClientUDPSocket::SendControlData(uint32 maxNumberOfBytesToSend, uint32 /*minFragSize*/)
{
// ZZ:UploadBandWithThrottler (UDP) -->
// NOTE: *** This function is invoked from a *different* thread!
	uint32 sentBytes = 0;
	sendLocker.Lock();
// <-- ZZ:UploadBandWithThrottler (UDP)
	while (!controlpacket_queue.IsEmpty() && !IsBusy() && sentBytes < maxNumberOfBytesToSend) { // ZZ:UploadBandWithThrottler (UDP)
		UDPPack *cur_packet = controlpacket_queue.RemoveHead();
		if (::GetTickCount() < cur_packet->dwTime + UDPMAXQUEUETIME) {
			int nLen = (int)cur_packet->packet->size + 2;
			int iLen = cur_packet->bEncrypt && (theApp.GetPublicIP() > 0 || cur_packet->bKad)
				? EncryptOverheadSize(cur_packet->bKad) : 0;
			uchar *sendbuffer = new uchar[nLen + iLen];
			memcpy(sendbuffer + iLen, cur_packet->packet->GetUDPHeader(), 2);
			memcpy(sendbuffer + iLen + 2, cur_packet->packet->pBuffer, cur_packet->packet->size);

			if (iLen) {
				nLen = EncryptSendClient(sendbuffer, nLen, cur_packet->pachTargetClientHashORKadID, cur_packet->bKad, cur_packet->nReceiverVerifyKey, (cur_packet->bKad ? Kademlia::CPrefs::GetUDPVerifyKey(cur_packet->dwIP) : 0u));
				//DEBUG_ONLY(  AddDebugLogLine(DLP_VERYLOW, false, _T("Sent obfuscated UDP packet to clientIP: %s, Kad: %s, ReceiverKey: %u"), (LPCTSTR)ipstr(cur_packet->dwIP), cur_packet->bKad ? _T("Yes") : _T("No"), cur_packet->nReceiverVerifyKey) );
			}
			iLen = SendTo(sendbuffer, nLen, cur_packet->dwIP, cur_packet->nPort);
			if (iLen >= 0) {
				sentBytes += iLen; // ZZ:UploadBandWithThrottler (UDP)
				delete cur_packet->packet;
				delete cur_packet;
			} else {
				controlpacket_queue.AddHead(cur_packet); //try to resend
				::Sleep(20);
			}
			delete[] sendbuffer;
		} else {
			delete cur_packet->packet;
			delete cur_packet;
		}
	}

// ZZ:UploadBandWithThrottler (UDP) -->
	if (!IsBusy() && !controlpacket_queue.IsEmpty())
		theApp.uploadBandwidthThrottler->QueueForSendingControlPacket(this);

	sendLocker.Unlock();

	return SocketSentBytes{0, sentBytes, true};
// <-- ZZ:UploadBandWithThrottler (UDP)
}

int CClientUDPSocket::SendTo(uchar *lpBuf, int nBufLen, uint32 dwIP, uint16 nPort)
{
	// NOTE: *** This function is invoked from a *different* thread!
	//Currently called only locally; sendLocker must be locked by the caller
	int result = CAsyncSocket::SendTo(lpBuf, nBufLen, nPort, ipstr(dwIP));
	if (result == SOCKET_ERROR) {
		DWORD dwError = (DWORD)CAsyncSocket::GetLastError();
		if (dwError == WSAEWOULDBLOCK) {
			m_bWouldBlock = true;
			return -1; //blocked
		}
		if (thePrefs.GetVerbose())
			DebugLogError(_T("Error: Client UDP socket, failed to send data to %s:%u: %s"), (LPCTSTR)ipstr(dwIP), nPort, (LPCTSTR)GetErrorMessage(dwError, 1));
		return 0; //error
	}
	return result; //success
}

bool CClientUDPSocket::SendPacket(Packet *packet, uint32 dwIP, uint16 nPort, bool bEncrypt, const uchar *pachTargetClientHashORKadID, bool bKad, uint32 nReceiverVerifyKey)
{
	UDPPack *newpending = new UDPPack;
	newpending->dwIP = dwIP;
	newpending->nPort = nPort;
	newpending->packet = packet;
	newpending->dwTime = ::GetTickCount();
	newpending->bEncrypt = bEncrypt && (pachTargetClientHashORKadID != NULL || (bKad && nReceiverVerifyKey != 0));
	newpending->bKad = bKad;
	newpending->nReceiverVerifyKey = nReceiverVerifyKey;

#ifdef _DEBUG
	if (newpending->packet->size > UDP_KAD_MAXFRAGMENT)
		DebugLogWarning(_T("Sending UDP packet > UDP_KAD_MAXFRAGMENT, opcode: %X, size: %u"), packet->opcode, packet->size);
#endif

	if (newpending->bEncrypt && pachTargetClientHashORKadID != NULL)
		md4cpy(newpending->pachTargetClientHashORKadID, pachTargetClientHashORKadID);
	else
		md4clr(newpending->pachTargetClientHashORKadID);
// ZZ:UploadBandWithThrottler (UDP) -->
	sendLocker.Lock();
	controlpacket_queue.AddTail(newpending);
	sendLocker.Unlock();

	theApp.uploadBandwidthThrottler->QueueForSendingControlPacket(this);
	return true;
// <-- ZZ:UploadBandWithThrottler (UDP)
}

bool CClientUDPSocket::Create()
{
	if (thePrefs.GetUDPPort()) {
		if (!CAsyncSocket::Create(thePrefs.GetUDPPort(), SOCK_DGRAM, FD_READ | FD_WRITE, thePrefs.GetBindAddrW()))
			return false;
		m_port = thePrefs.GetUDPPort();
		// the default socket size seems to be insufficient for this UDP socket
		// because we tend to drop packets if several arrived at the same time
		int val = 64 * 1024;
		if (!SetSockOpt(SO_RCVBUF, &val, sizeof val))
			DebugLogError(_T("Failed to increase socket size on UDP socket"));
	} else
		m_port = 0;
	return true;
}

bool CClientUDPSocket::Rebind()
{
	if (thePrefs.GetUDPPort() == m_port)
		return false;
	Close();
	return Create();
}