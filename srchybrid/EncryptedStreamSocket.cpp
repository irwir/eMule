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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

/* Basic Obfuscated Handshake Protocol Client <-> Client:
	-Key creation:
	 - Client A (Outgoing connection):
				Sendkey:	Md5(<UserHashClientB 16><MagicValue34 1><RandomKeyPartClientA 4>)  21
				Receivekey: Md5(<UserHashClientB 16><MagicValue203 1><RandomKeyPartClientA 4>) 21
	 - Client B (Incoming connection):
				Sendkey:	Md5(<UserHashClientB 16><MagicValue203 1><RandomKeyPartClientA 4>) 21
				Receivekey: Md5(<UserHashClientB 16><MagicValue34 1><RandomKeyPartClientA 4>)  21
		NOTE: First 1024 Bytes are discarded

	- Handshake
			-> The handshake is encrypted - except otherwise noted - by the Keys created above
			-> Handshake is blocking - do not start sending an answer before the request is completely received (this includes the random bytes)
			-> EncryptionMethod = 0 is Obfuscation and the only supported right now
		Client A: <SemiRandomNotProtocolMarker 1[Unencrypted]><RandomKeyPart 4[Unencrypted]><MagicValue 4><EncryptionMethodsSupported 1><EncryptionMethodPreferred 1><PaddingLen 1><RandomBytes PaddingLen%max256>
		Client B: <MagicValue 4><EncryptionMethodsSelected 1><PaddingLen 1><RandomBytes PaddingLen%max 256>
			-> The basic handshake is finished here, if an additional/different EncryptionMethod was selected it may continue negotiating details for this one

	- Overhead: 18-48 (~33) Bytes + 2 * IP/TCP Headers per Connection

	- Security for Basic Obfuscation:
			- Random looking stream, very limited protection against passive eavesdropping single connections

	- Additional Comments:
			- RandomKeyPart is needed to make multiple connections between two clients look different (but still random), since otherwise the same key
			  would be used and RC4 would create the same output. Since the key is a MD5 hash it doesn't weaken the key if that part is known
			- Why DH-KeyAgreement isn't used as basic obfuscation key: It doesn't offers substantial more protection against passive connection based protocol identification, it has about 200 bytes more overhead,
			  needs more CPU time, we cannot say if the received data is junk, unencrypted or part of the key agreement before the handshake is finished without losing the complete randomness,
			  it doesn't offers substantial protection against eavesdropping without added authentication

Basic Obfuscated Handshake Protocol Client <-> Server:
	- RC4 Key creation:
	 - Client (Outgoing connection):
				Sendkey:    Md5(<S 96><MagicValue34 1>)  97
				Receivekey: Md5(<S 96><MagicValue203 1>) 97
	 - Server (Incoming connection):
				Sendkey:    Md5(<S 96><MagicValue203 1>)  97
				Receivekey: Md5(<S 96><MagicValue34 1>) 97

	 NOTE: First 1024 Bytes are discarded

	- Handshake
			-> The handshake is encrypted - except otherwise noted - with the Keys created above
			-> Handshake is blocking - do not start sending an answer before the request is completely received (this includes the random bytes)
			-> EncryptionMethod = 0 is Obfuscation and the only supported right now

		Client: <SemiRandomNotProtocolMarker 1[Unencrypted]><G^A 96 [Unencrypted]><RandomBytes 0-15 [Unencrypted]>
		Server: <G^B 96 [Unencrypted]><MagicValue 4><EncryptionMethodsSupported 1><EncryptionMethodPreferred 1><PaddingLen 1><RandomBytes PaddingLen>
		Client: <MagicValue 4><EncryptionMethodsSelected 1><PaddingLen 1><RandomBytes PaddingLen> (Answer delayed till first payload to save a frame)


			-> The basic handshake is finished here, if an additional/different EncryptionMethod was selected it may continue negotiating details for this one

	- Overhead: 206-251 (~229) Bytes + 2 * IP/TCP Headers per Connection

	- DH Agreement Specifics: sizeof(a) and sizeof(b) = 128 Bits, g = 2, p = dh768_p (see below), sizeof p, s, etc. = 768 bits
*/

#include "stdafx.h"
#include "EncryptedStreamSocket.h"
#include "emule.h"
#include "md5sum.h"
#include "Log.h"
#include "preferences.h"
#include "otherfunctions.h"
#include "safefile.h"
#include "opcodes.h"
#include "clientlist.h"
#include "ServerConnect.h"
#include <cryptopp/osrng.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#define	MAGICVALUE_REQUESTER	34							// modification of the requester-send and server-receive key
#define	MAGICVALUE_SERVER		203							// modification of the server-send and requester-receive key
#define	MAGICVALUE_SYNC			0x835E6FC4					// value to check if we have a working encrypted stream
#define DHAGREEMENT_A_BITS		128

#define PRIMESIZE_BYTES	 96
static unsigned char dh768_p[] = {
		0xF2,0xBF,0x52,0xC5,0x5F,0x58,0x7A,0xDD,0x53,0x71,0xA9,0x36,
		0xE8,0x86,0xEB,0x3C,0x62,0x17,0xA3,0x3E,0xC3,0x4C,0xB4,0x0D,
		0xC7,0x3A,0x41,0xA6,0x43,0xAF,0xFC,0xE7,0x21,0xFC,0x28,0x63,
		0x66,0x53,0x5B,0xDB,0xCE,0x25,0x9F,0x22,0x86,0xDA,0x4A,0x91,
		0xB2,0x07,0xCB,0xAA,0x52,0x55,0xD4,0xF6,0x1C,0xCE,0xAE,0xD4,
		0x5A,0xD5,0xE0,0x74,0x7D,0xF7,0x78,0x18,0x28,0x10,0x5F,0x34,
		0x0F,0x76,0x23,0x87,0xF8,0x8B,0x28,0x91,0x42,0xFB,0x42,0x68,
		0x8F,0x05,0x15,0x0F,0x54,0x8B,0x5F,0x43,0x6A,0xF7,0x0D,0xF3
};

static CryptoPP::AutoSeededRandomPool cryptRandomGen;

IMPLEMENT_DYNAMIC(CEncryptedStreamSocket, CAsyncSocketEx)

CEncryptedStreamSocket::CEncryptedStreamSocket()
	: m_dbgbyEncryptionSupported(0xff)
	, m_dbgbyEncryptionRequested(0xff)
	, m_dbgbyEncryptionMethodSet(0xff)
	, m_StreamCryptState(thePrefs.IsClientCryptLayerSupported() ? ECS_UNKNOWN : ECS_NONE)
	, m_EncryptionMethod(ENM_OBFUSCATION)
	, m_bFullReceive(true)
	, m_bServerCrypt()
	, m_pRC4SendKey()
	, m_pRC4ReceiveKey()
	, m_pfiReceiveBuffer()
	, m_pfiSendBuffer()
	, m_nRandomKeyPart()
	, m_nReceiveBytesWanted()
	, m_nObfuscatedBytesReceived()
	, m_NegotiatingState(ONS_NONE)
{
};

CEncryptedStreamSocket::~CEncryptedStreamSocket()
{
	delete m_pRC4ReceiveKey;
	delete m_pRC4SendKey;
	if (m_pfiReceiveBuffer != NULL) {
		free(m_pfiReceiveBuffer->Detach());
		delete m_pfiReceiveBuffer;
	}
	delete m_pfiSendBuffer;
};

void CEncryptedStreamSocket::CryptPrepareSendData(uchar *pBuffer, uint32 nLen)
{
	if (!IsEncryptionLayerReady()) {
		ASSERT(0); // must be a bug
		return;
	}
	if (m_StreamCryptState == ECS_UNKNOWN) {
		//this happens when the encryption option was not set on an outgoing connection
		//or if we try to send before receiving on an incoming connection - both shouldn't happen
		m_StreamCryptState = ECS_NONE;
		DebugLogError(_T("CEncryptedStreamSocket: Overwriting State ECS_UNKNOWN with ECS_NONE because of premature Send() (%s)"), (LPCTSTR)DbgGetIPString());
	}
	if (m_StreamCryptState == ECS_ENCRYPTING)
		RC4Crypt(pBuffer, nLen, m_pRC4SendKey);
}

// unfortunately sending cannot be made transparent for the derived class, because of WSA_WOULDBLOCK
// together with the fact that each byte must pass the keystream only once
int CEncryptedStreamSocket::Send(const void *lpBuf, int nBufLen, int nFlags)
{
	if (!IsEncryptionLayerReady()) {
		ASSERT(0); // must be a bug
		return 0;
	}
	if (m_bServerCrypt && m_StreamCryptState == ECS_ENCRYPTING && m_pfiSendBuffer != NULL) {
		ASSERT(m_NegotiatingState == ONS_BASIC_SERVER_DELAYEDSENDING);
		// handshake data was delayed to put it into one frame with the first payload to the server
		// do so now with the payload attached
		int nRes = SendNegotiatingData(lpBuf, nBufLen, nBufLen);
		ASSERT(nRes != SOCKET_ERROR);
		(void)nRes;
		return nBufLen;	// report a full send, even if we didn't for some reason - the data is in our buffer and will be handled later
	}
	if (m_NegotiatingState == ONS_BASIC_SERVER_DELAYEDSENDING)
		ASSERT(0);

	if (m_StreamCryptState == ECS_UNKNOWN) {
		//this happens when the encryption option was not set on an outgoing connection
		//or if we try to send before receiving on an incoming connection - both shouldn't happen
		m_StreamCryptState = ECS_NONE;
		DebugLogError(_T("CEncryptedStreamSocket: Overwriting State ECS_UNKNOWN with ECS_NONE because of premature Send() (%s)"), (LPCTSTR)DbgGetIPString());
	}
	return CAsyncSocketEx::Send(lpBuf, nBufLen, nFlags);
}

int CEncryptedStreamSocket::SendOv(CArray<WSABUF> &raBuffer, LPWSAOVERLAPPED lpOverlapped)
{
	if (!IsEncryptionLayerReady()) {
		ASSERT(0); // must be a bug
		return -1;
	}
	if (m_bServerCrypt && m_StreamCryptState == ECS_ENCRYPTING && m_pfiSendBuffer != NULL) {
		ASSERT(m_NegotiatingState == ONS_BASIC_SERVER_DELAYEDSENDING);
		// handshake data was delayed to put it into one frame with the first payload to the server
		// attach it now to the sendbuffer
		WSABUF wbuf = {(ULONG)m_pfiSendBuffer->GetLength()};
		wbuf.buf = new CHAR[wbuf.len];
		m_pfiSendBuffer->SeekToBegin();
		m_pfiSendBuffer->Read(wbuf.buf, wbuf.len);
		raBuffer.InsertAt(0, wbuf);
		m_NegotiatingState = ONS_COMPLETE;
		delete m_pfiSendBuffer;
		m_pfiSendBuffer = NULL;
	} else
		ASSERT(m_NegotiatingState != ONS_BASIC_SERVER_DELAYEDSENDING);

	if (m_StreamCryptState == ECS_UNKNOWN) {
		//this happens when the encryption option was not set on an outgoing connection
		//or if we try to send before receiving on an incoming connection - both shouldn't happen
		m_StreamCryptState = ECS_NONE;
		DebugLogError(_T("CEncryptedStreamSocket: Overwriting State ECS_UNKNOWN with ECS_NONE because of premature Send() (%s)"), (LPCTSTR)DbgGetIPString());
	}
	return WSASend(GetSocketHandle(), raBuffer.GetData(), (DWORD)raBuffer.GetCount(), NULL, 0, lpOverlapped, NULL);
}

bool CEncryptedStreamSocket::IsEncryptionLayerReady()
{
	return (m_StreamCryptState == ECS_NONE || m_StreamCryptState == ECS_ENCRYPTING || m_StreamCryptState == ECS_UNKNOWN)
		&& (m_pfiSendBuffer == NULL || (m_bServerCrypt && m_NegotiatingState == ONS_BASIC_SERVER_DELAYEDSENDING));
}

int CEncryptedStreamSocket::Receive(void *lpBuf, int nBufLen, int nFlags)
{
	m_nObfuscatedBytesReceived = CAsyncSocketEx::Receive(lpBuf, nBufLen, nFlags);
	m_bFullReceive = (m_nObfuscatedBytesReceived == nBufLen);

	if (m_nObfuscatedBytesReceived == SOCKET_ERROR || m_nObfuscatedBytesReceived <= 0)
		return m_nObfuscatedBytesReceived;

	switch (m_StreamCryptState) {
	case ECS_NONE: // disabled, just pass it through
		break;
	case ECS_UNKNOWN:
		switch (*static_cast<uchar*>(lpBuf)) {
		case OP_EDONKEYPROT:
		case OP_PACKEDPROT:
		case OP_EMULEPROT:
			break; //normal header
		default:
			{
				int nRead = 1;
				StartNegotiation(false);
				const int nNegRes = Negotiate(static_cast<uchar*>(lpBuf) + nRead, m_nObfuscatedBytesReceived - nRead);
				if (nNegRes != -1) {
					nRead += nNegRes;
					if (nRead != m_nObfuscatedBytesReceived) {
						// this means we have more data then the current negotiation step required,
						// or there is a bug, and this should never happen.
						// (note: even if it just finished the handshake here, there still can be
						// no data left, since the other client didn't receive our response yet)
						DebugLogError(_T("CEncryptedStreamSocket: Client %s sent more data then expected while negotiating, disconnecting (1)"), (LPCTSTR)DbgGetIPString());
						OnError(ERR_ENCRYPTION);
					}
				}
			}
			return 0;
		}
		// doesn't seem to be encrypted
		m_StreamCryptState = ECS_NONE;

		// if we require an encrypted connection, cut the connection here.
		// This shouldn't happen that often at least with other up-to-date eMule clients
		// because they check for incompatibility before connecting if possible
		if (thePrefs.IsClientCryptLayerRequired()) {
			// TODO: Remove me when i have been solved
			// Even if the Require option is enabled, we currently have to accept unencrypted connection
			// which are made for lowid/firewall checks from servers and other selected client from us.
			// Otherwise, this option would always result in a lowid/firewalled status.
			// This is of course not nice, but we can't avoid this workaround until servers and kad
			// completely support encryption too, which will at least for kad take a bit
			// only exception is the .ini option ClientCryptLayerRequiredStrict
			// which will even ignore test connections
			// Update: New server now support encrypted callbacks

			SOCKADDR_IN sockAddr = {};
			int nSockAddrLen = sizeof sockAddr;
			GetPeerName((LPSOCKADDR)&sockAddr, &nSockAddrLen);
			if (thePrefs.IsClientCryptLayerRequiredStrict() || (!theApp.serverconnect->AwaitingTestFromIP(sockAddr.sin_addr.s_addr)
				&& !theApp.clientlist->IsKadFirewallCheckIP(sockAddr.sin_addr.s_addr)))
			{
#if defined(_DEBUG) || defined(_BETA) || defined(_DEVBUILD)
				// TODO: Remove after testing
				AddDebugLogLine(DLP_DEFAULT, false, _T("Rejected incoming connection because Obfuscation was required but not used %s"), (LPCTSTR)DbgGetIPString());
#endif
				OnError(ERR_ENCRYPTION_NOTALLOWED);
				return 0;
			}
			AddDebugLogLine(DLP_DEFAULT, false, _T("Incoming unencrypted firewall check connection permitted despite RequireEncryption setting  - %s"), (LPCTSTR)DbgGetIPString());
		}
		break; // buffer was unchanged, we can just pass it through
	case ECS_PENDING:
	case ECS_PENDING_SERVER:
		ASSERT(0);
		DebugLogError(_T("CEncryptedStreamSocket Received data before sending on outgoing connection"));
		m_StreamCryptState = ECS_NONE;
		break;
	case ECS_NEGOTIATING:
		{
			const int nRead = Negotiate(static_cast<uchar*>(lpBuf), m_nObfuscatedBytesReceived);
			if (nRead != -1 && nRead != m_nObfuscatedBytesReceived) {
				if (m_StreamCryptState == ECS_ENCRYPTING) {
					// we finished the handshake and if this was an outgoing connection it is allowed (but strange and unlikely) that the client sent payload
					DebugLogWarning(_T("CEncryptedStreamSocket: Client %s has finished the handshake but also sent payload on an outgoing connection"), (LPCTSTR)DbgGetIPString());
					memmove(lpBuf, (uchar*)lpBuf + nRead, m_nObfuscatedBytesReceived - nRead);
					return m_nObfuscatedBytesReceived - nRead;
				}
				// this means we have more data then the current negotiation step required (or there is a bug) and this should never happen
				DebugLogError(_T("CEncryptedStreamSocket: Client %s sent more data then expected while negotiating, disconnecting (2)"), (LPCTSTR)DbgGetIPString());
				OnError(ERR_ENCRYPTION);
			}
		}
		return 0;
	case ECS_ENCRYPTING:
		// basic obfuscation enabled and set, so decrypt and pass along
		RC4Crypt(static_cast<uchar*>(lpBuf), m_nObfuscatedBytesReceived, m_pRC4ReceiveKey);
		break;
	default:
		ASSERT(0);
	}
	return m_nObfuscatedBytesReceived;
}

void CEncryptedStreamSocket::SetConnectionEncryption(bool bEnabled, const uchar *pTargetClientHash, bool bServerConnection)
{
	if (m_StreamCryptState != ECS_UNKNOWN && m_StreamCryptState != ECS_NONE) {
		ASSERT(m_StreamCryptState == ECS_NONE || bEnabled);
		return;
	}
	ASSERT(m_pRC4SendKey == NULL);
	ASSERT(m_pRC4ReceiveKey == NULL);

	if (bEnabled && pTargetClientHash != NULL && !bServerConnection) {
		m_StreamCryptState = ECS_PENDING;
		// create obfuscation keys, see on top for key format

		// use the crypt random generator
		m_nRandomKeyPart = cryptRandomGen.GenerateWord32();

		uchar achKeyData[21];
		md4cpy(achKeyData, pTargetClientHash);
		achKeyData[16] = MAGICVALUE_REQUESTER;
		PokeUInt32(&achKeyData[17], m_nRandomKeyPart);
		MD5Sum md5(achKeyData, sizeof achKeyData);
		m_pRC4SendKey = RC4CreateKey(md5.GetRawHash(), 16, NULL);

		achKeyData[16] = MAGICVALUE_SERVER;
		md5.Calculate(achKeyData, sizeof achKeyData);
		m_pRC4ReceiveKey = RC4CreateKey(md5.GetRawHash(), 16, NULL);
	} else if (bServerConnection && bEnabled) {
		m_bServerCrypt = true;
		m_StreamCryptState = ECS_PENDING_SERVER;
	} else {
		ASSERT(!bEnabled);
		m_StreamCryptState = ECS_NONE;
	}
}

void CEncryptedStreamSocket::OnSend(int)
{
	// if the socket just connected and this is outgoing, we might want to start the handshake here
	if (m_StreamCryptState == ECS_PENDING || m_StreamCryptState == ECS_PENDING_SERVER) {
		StartNegotiation(true);
		return;
	}
	// check if we have negotiating data pending
	if (m_pfiSendBuffer != NULL) {
		ASSERT(m_StreamCryptState >= ECS_NEGOTIATING);
		SendNegotiatingData(NULL, 0);
	}
}

void CEncryptedStreamSocket::StartNegotiation(bool bOutgoing)
{

	if (!bOutgoing) {
		m_NegotiatingState = ONS_BASIC_CLIENTA_RANDOMPART;
		m_StreamCryptState = ECS_NEGOTIATING;
		m_nReceiveBytesWanted = 4;
	} else if (m_StreamCryptState == ECS_PENDING) {
		CSafeMemFile fileRequest(29);
		const uint8 bySemiRandomNotProtocolMarker = GetSemiRandomNotProtocolMarker();
		fileRequest.WriteUInt8(bySemiRandomNotProtocolMarker);
		fileRequest.WriteUInt32(m_nRandomKeyPart);
		fileRequest.WriteUInt32(MAGICVALUE_SYNC);
		const uint8 bySupportedEncryptionMethod = ENM_OBFUSCATION; // we do not support any other encryption in this version
		fileRequest.WriteUInt8(bySupportedEncryptionMethod);
		fileRequest.WriteUInt8(bySupportedEncryptionMethod); // so we also prefer this one
		uint8 byPadding = (uint8)(cryptRandomGen.GenerateByte() % (thePrefs.GetCryptTCPPaddingLength() + 1));
		fileRequest.WriteUInt8(byPadding);
		for (int i = byPadding; --i >= 0;)
			fileRequest.WriteUInt8(cryptRandomGen.GenerateByte());

		m_NegotiatingState = ONS_BASIC_CLIENTB_MAGICVALUE;
		m_StreamCryptState = ECS_NEGOTIATING;
		m_nReceiveBytesWanted = 4;

		SendNegotiatingData(fileRequest.GetBuffer(), (int)fileRequest.GetLength(), 5);
	} else if (m_StreamCryptState == ECS_PENDING_SERVER) {
		CSafeMemFile fileRequest(113);
		const uint8 bySemiRandomNotProtocolMarker = GetSemiRandomNotProtocolMarker();
		fileRequest.WriteUInt8(bySemiRandomNotProtocolMarker);

		m_cryptDHA.Randomize(cryptRandomGen, DHAGREEMENT_A_BITS); // our random a
		ASSERT(m_cryptDHA.MinEncodedSize() <= DHAGREEMENT_A_BITS / 8);
		CryptoPP::Integer cryptDHPrime((byte*)dh768_p, PRIMESIZE_BYTES);  // our fixed prime
		// calculate g^a % p
		CryptoPP::Integer cryptDHGexpAmodP = CryptoPP::a_exp_b_mod_c(CryptoPP::Integer(2), m_cryptDHA, cryptDHPrime);
		ASSERT(m_cryptDHA.MinEncodedSize() <= PRIMESIZE_BYTES);
		// put the result into a buffer
		uchar aBuffer[PRIMESIZE_BYTES];
		cryptDHGexpAmodP.Encode(aBuffer, PRIMESIZE_BYTES);

		fileRequest.Write(aBuffer, PRIMESIZE_BYTES);
		uint8 byPadding = (uint8)(cryptRandomGen.GenerateByte() % 16); // add random padding
		fileRequest.WriteUInt8(byPadding);
		for (int i = byPadding; --i >= 0;)
			fileRequest.WriteUInt8(cryptRandomGen.GenerateByte());

		m_NegotiatingState = ONS_BASIC_SERVER_DHANSWER;
		m_StreamCryptState = ECS_NEGOTIATING;
		m_nReceiveBytesWanted = 96;

		SendNegotiatingData(fileRequest.GetBuffer(), (int)fileRequest.GetLength(), (int)fileRequest.GetLength());
	} else {
		ASSERT(0);
		m_StreamCryptState = ECS_NONE;
	}
}

int CEncryptedStreamSocket::Negotiate(const uchar *pBuffer, int nLen)
{
	ASSERT(m_nReceiveBytesWanted > 0);
	try {
		int nRead = 0;
		while (m_NegotiatingState != ONS_COMPLETE && m_nReceiveBytesWanted > 0) {
			if (m_nReceiveBytesWanted > 512) {
				ASSERT(0);
				return 0;
			}

			if (m_pfiReceiveBuffer == NULL) {
				BYTE *pReceiveBuffer = (BYTE*)malloc(512); // use a fixed size buffer
				if (pReceiveBuffer == NULL)
					AfxThrowMemoryException();
				m_pfiReceiveBuffer = new CSafeMemFile(pReceiveBuffer, 512);
			}
			const UINT nToRead = min(nLen - nRead, m_nReceiveBytesWanted);
			m_pfiReceiveBuffer->Write(pBuffer + nRead, nToRead);
			nRead += nToRead;
			m_nReceiveBytesWanted -= nToRead;
			if (m_nReceiveBytesWanted > 0)
				return nRead;
			const uint32 nCurrentBytesLen = (uint32)m_pfiReceiveBuffer->GetPosition();

			if (m_NegotiatingState != ONS_BASIC_CLIENTA_RANDOMPART && m_NegotiatingState != ONS_BASIC_SERVER_DHANSWER) { // don't have the keys yet
				BYTE *pCryptBuffer = m_pfiReceiveBuffer->Detach();
				RC4Crypt(pCryptBuffer, nCurrentBytesLen, m_pRC4ReceiveKey);
				m_pfiReceiveBuffer->Attach(pCryptBuffer, 512);
			}
			m_pfiReceiveBuffer->SeekToBegin();

			switch (m_NegotiatingState) {
			case ONS_NONE: // would be a bug
				ASSERT(0);
				return 0;
			case ONS_BASIC_CLIENTA_RANDOMPART:
				{
					ASSERT(m_pRC4ReceiveKey == NULL);

					uchar achKeyData[21];
					md4cpy(achKeyData, thePrefs.GetUserHash());
					achKeyData[16] = MAGICVALUE_REQUESTER;
					m_pfiReceiveBuffer->Read(achKeyData + 17, 4); // random key part sent from remote client

					MD5Sum md5(achKeyData, sizeof achKeyData);
					m_pRC4ReceiveKey = RC4CreateKey(md5.GetRawHash(), 16, NULL);
					achKeyData[16] = MAGICVALUE_SERVER;
					md5.Calculate(achKeyData, sizeof achKeyData);
					m_pRC4SendKey = RC4CreateKey(md5.GetRawHash(), 16, NULL);

					m_NegotiatingState = ONS_BASIC_CLIENTA_MAGICVALUE;
					m_nReceiveBytesWanted = 4;
				}
				break;
			case ONS_BASIC_CLIENTA_MAGICVALUE:
				{
					uint32 dwValue = m_pfiReceiveBuffer->ReadUInt32();
					if (dwValue != MAGICVALUE_SYNC) {
						DebugLogError(_T("CEncryptedStreamSocket: Received wrong magic value from clientIP %s on a supposedly encrypted stream / Wrong Header"), (LPCTSTR)DbgGetIPString());
						OnError(ERR_ENCRYPTION);
						return -1;
					}
					// yup, the one or the other way it worked, this is an encrypted stream
					//DEBUG_ONLY( DebugLog(_T("Received proper magic value, clientIP: %s"), DbgGetIPString()) );
					// set the receiver key
					m_NegotiatingState = ONS_BASIC_CLIENTA_METHODTAGSPADLEN;
					m_nReceiveBytesWanted = 3;
				}
				break;
			case ONS_BASIC_CLIENTA_METHODTAGSPADLEN:
				m_dbgbyEncryptionSupported = m_pfiReceiveBuffer->ReadUInt8();
				m_dbgbyEncryptionRequested = m_pfiReceiveBuffer->ReadUInt8();
				if (m_dbgbyEncryptionRequested != ENM_OBFUSCATION)
					AddDebugLogLine(DLP_LOW, false, _T("CEncryptedStreamSocket: Client %s preferred unsupported encryption method (%i)"), (LPCTSTR)DbgGetIPString(), m_dbgbyEncryptionRequested);
				m_nReceiveBytesWanted = m_pfiReceiveBuffer->ReadUInt8();
				m_NegotiatingState = ONS_BASIC_CLIENTA_PADDING;
				//if (m_nReceiveBytesWanted > 16)
				//	AddDebugLogLine(DLP_LOW, false, _T("CEncryptedStreamSocket: Client %s sent more than 16 (%i) padding bytes"), DbgGetIPString(), m_nReceiveBytesWanted);
				if (m_nReceiveBytesWanted > 0)
					break;
			case ONS_BASIC_CLIENTA_PADDING:
				{
					// ignore the random bytes, send the response, set status complete
					CSafeMemFile fileResponse(26);
					fileResponse.WriteUInt32(MAGICVALUE_SYNC);
					const uint8 bySelectedEncryptionMethod = ENM_OBFUSCATION; // we do not support any further encryption in this version, so no need to look which the other client preferred
					fileResponse.WriteUInt8(bySelectedEncryptionMethod);

					SOCKADDR_IN sockAddr = {};
					int nSockAddrLen = sizeof sockAddr;
					GetPeerName((LPSOCKADDR)&sockAddr, &nSockAddrLen);
					const uint8 byPaddingLen = theApp.serverconnect->AwaitingTestFromIP(sockAddr.sin_addr.s_addr) ? 16 : (thePrefs.GetCryptTCPPaddingLength() + 1);
					uint8 byPadding = (uint8)(cryptRandomGen.GenerateByte() % byPaddingLen);

					fileResponse.WriteUInt8(byPadding);
					for (int i = byPadding; --i >= 0;)
						fileResponse.WriteUInt8((uint8)rand());
					SendNegotiatingData(fileResponse.GetBuffer(), (int)fileResponse.GetLength());
					m_NegotiatingState = ONS_COMPLETE;
					m_StreamCryptState = ECS_ENCRYPTING;
					//DEBUG_ONLY( DebugLog(_T("CEncryptedStreamSocket: Finished Obfuscation handshake with client %s (incoming)"), DbgGetIPString()) );
				}
				break;
			case ONS_BASIC_CLIENTB_MAGICVALUE:
				{
					if (m_pfiReceiveBuffer->ReadUInt32() != MAGICVALUE_SYNC) {
						DebugLogError(_T("CEncryptedStreamSocket: EncryptedstreamSyncError: Client sent wrong Magic Value as answer, cannot complete handshake (%s)"), (LPCTSTR)DbgGetIPString());
						OnError(ERR_ENCRYPTION);
						return -1;
					}
					m_NegotiatingState = ONS_BASIC_CLIENTB_METHODTAGSPADLEN;
					m_nReceiveBytesWanted = 2;
				}
				break;
			case ONS_BASIC_CLIENTB_METHODTAGSPADLEN:
				{
					m_dbgbyEncryptionMethodSet = m_pfiReceiveBuffer->ReadUInt8();
					if (m_dbgbyEncryptionMethodSet != ENM_OBFUSCATION) {
						DebugLogError(_T("CEncryptedStreamSocket: Client %s set unsupported encryption method (%i), handshake failed"), (LPCTSTR)DbgGetIPString(), m_dbgbyEncryptionMethodSet);
						OnError(ERR_ENCRYPTION);
						return -1;
					}
					m_nReceiveBytesWanted = m_pfiReceiveBuffer->ReadUInt8();
					m_NegotiatingState = ONS_BASIC_CLIENTB_PADDING;
					if (m_nReceiveBytesWanted > 0)
						break;
				}
			case ONS_BASIC_CLIENTB_PADDING:
				// ignore the random bytes, the handshake is complete
				m_NegotiatingState = ONS_COMPLETE;
				m_StreamCryptState = ECS_ENCRYPTING;
				//DEBUG_ONLY( DebugLog(_T("CEncryptedStreamSocket: Finished Obfuscation handshake with client %s (outgoing)"), DbgGetIPString()) );
				break;
			case ONS_BASIC_SERVER_DHANSWER:
				{
					ASSERT(!m_cryptDHA.IsZero());
					uchar aBuffer[PRIMESIZE_BYTES + 1];
					m_pfiReceiveBuffer->Read(aBuffer, PRIMESIZE_BYTES);
					CryptoPP::Integer cryptDHAnswer(static_cast<byte*>(aBuffer), PRIMESIZE_BYTES);
					CryptoPP::Integer cryptDHPrime(static_cast<byte*>(dh768_p), PRIMESIZE_BYTES);  // our fixed prime
					CryptoPP::Integer cryptResult = CryptoPP::a_exp_b_mod_c(cryptDHAnswer, m_cryptDHA, cryptDHPrime);

					m_cryptDHA = 0;
					DEBUG_ONLY(memset(aBuffer, 0, sizeof aBuffer));
					ASSERT(cryptResult.MinEncodedSize() <= PRIMESIZE_BYTES);

					// create the keys
					cryptResult.Encode(aBuffer, PRIMESIZE_BYTES);
					aBuffer[PRIMESIZE_BYTES] = MAGICVALUE_REQUESTER;
					MD5Sum md5(aBuffer, sizeof aBuffer);
					m_pRC4SendKey = RC4CreateKey(md5.GetRawHash(), 16, NULL);
					aBuffer[PRIMESIZE_BYTES] = MAGICVALUE_SERVER;
					md5.Calculate(aBuffer, sizeof aBuffer);
					m_pRC4ReceiveKey = RC4CreateKey(md5.GetRawHash(), 16, NULL);

					m_NegotiatingState = ONS_BASIC_SERVER_MAGICVALUE;
					m_nReceiveBytesWanted = 4;
				}
				break;
			case ONS_BASIC_SERVER_MAGICVALUE:
				{
					uint32 dwValue = m_pfiReceiveBuffer->ReadUInt32();
					if (dwValue != MAGICVALUE_SYNC) {
						DebugLogError(_T("CEncryptedStreamSocket: Received wrong magic value after DH-Agreement from Server connection"), (LPCTSTR)DbgGetIPString());
						OnError(ERR_ENCRYPTION);
						return -1;
					}
					// yup, the one or the other way it worked, this is an encrypted stream
					DebugLog(_T("Received proper magic value after DH-Agreement from Server connection IP: %s"), (LPCTSTR)DbgGetIPString());
					// set the receiver key
					m_NegotiatingState = ONS_BASIC_SERVER_METHODTAGSPADLEN;
					m_nReceiveBytesWanted = 3;
				}
				break;
			case ONS_BASIC_SERVER_METHODTAGSPADLEN:
				m_dbgbyEncryptionSupported = m_pfiReceiveBuffer->ReadUInt8();
				m_dbgbyEncryptionRequested = m_pfiReceiveBuffer->ReadUInt8();
				if (m_dbgbyEncryptionRequested != ENM_OBFUSCATION)
					AddDebugLogLine(DLP_LOW, false, _T("CEncryptedStreamSocket: Server %s preferred unsupported encryption method (%i)"), (LPCTSTR)DbgGetIPString(), m_dbgbyEncryptionRequested);
				m_nReceiveBytesWanted = m_pfiReceiveBuffer->ReadUInt8();
				m_NegotiatingState = ONS_BASIC_SERVER_PADDING;
				if (m_nReceiveBytesWanted > 16)
					AddDebugLogLine(DLP_LOW, false, _T("CEncryptedStreamSocket: Server %s sent more than 16 (%i) padding bytes"), (LPCTSTR)DbgGetIPString(), m_nReceiveBytesWanted);
				if (m_nReceiveBytesWanted > 0)
					break;
			case ONS_BASIC_SERVER_PADDING:
				{
					// ignore the random bytes (they are decrypted already), send the response, set status complete
					CSafeMemFile fileResponse(26);
					fileResponse.WriteUInt32(MAGICVALUE_SYNC);
					const uint8 bySelectedEncryptionMethod = ENM_OBFUSCATION; // we do not support any further encryption in this version, so no need to look which the other client preferred
					fileResponse.WriteUInt8(bySelectedEncryptionMethod);
					uint8 byPadding = (uint8)(cryptRandomGen.GenerateByte() % 16);
					fileResponse.WriteUInt8(byPadding);
					for (int i = byPadding; --i >= 0;)
						fileResponse.WriteUInt8((uint8)rand());

					m_NegotiatingState = ONS_BASIC_SERVER_DELAYEDSENDING;
					SendNegotiatingData(fileResponse.GetBuffer(), (int)fileResponse.GetLength(), 0, true); // don't actually send it right now, store it in our sendbuffer
					m_StreamCryptState = ECS_ENCRYPTING;
					DEBUG_ONLY(DebugLog(_T("CEncryptedStreamSocket: Finished DH Obfuscation handshake with Server %s"), (LPCTSTR)DbgGetIPString()));
				}
				break;
			default:
				ASSERT(0);
			}
			m_pfiReceiveBuffer->SeekToBegin();
		}
		if (m_pfiReceiveBuffer != NULL) {
			free(m_pfiReceiveBuffer->Detach());
			delete m_pfiReceiveBuffer;
			m_pfiReceiveBuffer = NULL;
		}
		return nRead;
	} catch (CFileException *error) {
		// can only be caused by a bug in negotiation handling, not by the data stream
		error->Delete();
		ASSERT(0);
		OnError(ERR_ENCRYPTION);
		if (m_pfiReceiveBuffer != NULL) {
			free(m_pfiReceiveBuffer->Detach());
			delete m_pfiReceiveBuffer;
			m_pfiReceiveBuffer = NULL;
		}
	}
	return -1;
}

int CEncryptedStreamSocket::SendNegotiatingData(const void *lpBuf, int nBufLen, int nStartCryptFromByte, bool bDelaySend)
{
	ASSERT(m_StreamCryptState == ECS_NEGOTIATING || m_StreamCryptState == ECS_ENCRYPTING);
	ASSERT(nStartCryptFromByte <= nBufLen);
	ASSERT(m_NegotiatingState == ONS_BASIC_SERVER_DELAYEDSENDING || !bDelaySend);

	BYTE *pBuffer;
	if (lpBuf == NULL)
		pBuffer = NULL;
	else {
		pBuffer = (BYTE*)malloc(nBufLen);
		if (pBuffer == NULL)
			AfxThrowMemoryException();
		if (nStartCryptFromByte > 0)
			memcpy(pBuffer, lpBuf, nStartCryptFromByte);
		if (nBufLen > nStartCryptFromByte)
			RC4Crypt((uchar*)lpBuf + nStartCryptFromByte,  pBuffer + nStartCryptFromByte, nBufLen - nStartCryptFromByte, m_pRC4SendKey);
		if (m_pfiSendBuffer != NULL) {
			// we already have data pending. Attach it and try to send
			if (m_NegotiatingState == ONS_BASIC_SERVER_DELAYEDSENDING)
				m_NegotiatingState = ONS_COMPLETE;
			else
				ASSERT(0);
			m_pfiSendBuffer->SeekToEnd();
			m_pfiSendBuffer->Write(pBuffer, nBufLen);
			free(pBuffer);
			pBuffer = NULL;
			nStartCryptFromByte = 0;
			// we want to try to send it right now
		}
	}
	if (pBuffer == NULL) {
		// this call is for processing pending data
		if (m_pfiSendBuffer == NULL || nStartCryptFromByte > 0) {
			ASSERT(0);
			return 0;							// or not
		}
		nBufLen = (uint32)m_pfiSendBuffer->GetLength();
		pBuffer = m_pfiSendBuffer->Detach();
		delete m_pfiSendBuffer;
		m_pfiSendBuffer = NULL;
	}
	ASSERT(m_pfiSendBuffer == NULL);
	int result = bDelaySend ? 0 : CAsyncSocketEx::Send(pBuffer, nBufLen);
	if (result == SOCKET_ERROR || bDelaySend) {
		m_pfiSendBuffer = new CSafeMemFile(128);
		m_pfiSendBuffer->Write(pBuffer, nBufLen);
	} else if (result < nBufLen) {
		m_pfiSendBuffer = new CSafeMemFile(128);
		m_pfiSendBuffer->Write(pBuffer + result, nBufLen - result);
	}
	free(pBuffer);
	return result;
}

CString CEncryptedStreamSocket::DbgGetIPString()
{
	SOCKADDR_IN sockAddr = {};
	int nSockAddrLen = sizeof sockAddr;
	GetPeerName((LPSOCKADDR)&sockAddr, &nSockAddrLen);
	return ipstr(sockAddr.sin_addr.s_addr);
}

uint8 CEncryptedStreamSocket::GetSemiRandomNotProtocolMarker()
{
	for (int i = 32; --i >= 0;) {
		uint8 bySemiRandomNotProtocolMarker = cryptRandomGen.GenerateByte();
		switch (bySemiRandomNotProtocolMarker) {
		case OP_EDONKEYPROT:
		case OP_PACKEDPROT:
		case OP_EMULEPROT:
			continue; // not allowed values
		}
		return bySemiRandomNotProtocolMarker;
	}
	// either we have _real_ bad luck or the random generator is a bit messed up
	ASSERT(0);
	return 0x01;
}