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

/* Basic Obfuscated Handshake Protocol UDP:
	see EncryptedStreamSocket.h

****************************** ED2K Packets

	-Key creation Client <-> Client:
	 - Client A (Outgoing connection):
				Sendkey:	Md5(<UserHashClientB 16><IPClientA 4><MagicValue91 1><RandomKeyPartClientA 2>)  23
	 - Client B (Incoming connection):
				Receivekey: Md5(<UserHashClientB 16><IPClientA 4><MagicValue91 1><RandomKeyPartClientA 2>)  23
	 - Note: The first 1024 Bytes will be _NOT_ discarded for UDP keys to safe CPU time

	- Handshake
			-> The handshake is encrypted - except otherwise noted - by the Keys created above
			-> Padding is currently not used for UDP meaning that PaddingLen will be 0, using PaddingLens up to 16 Bytes is acceptable however
		Client A: <SemiRandomNotProtocolMarker 7 Bits[Unencrypted]><ED2K Marker 1Bit = 1><RandomKeyPart 2[Unencrypted]><MagicValue 4><PaddingLen 1><RandomBytes PaddingLen%16>

	- Additional Comments:
			- For obvious reasons the UDP handshake is actually no handshake. If a different Encryption method (or better a different Key) is to be used this has to be negotiated in a TCP connection
			- SemiRandomNotProtocolMarker is a Byte which has a value unequal any Protocol header byte. This is a compromise, turning in complete randomness (and nice design) but gaining
			  a lower CPU usage
			- Kad/Ed2k Marker are only indicators, which possibility could be tried first, and should not be trusted

****************************** Server Packets

	-Key creation Client <-> Server:
	 - Client A (Outgoing connection client -> server):
				Sendkey:	Md5(<BaseKey 4><MagicValueClientServer 1><RandomKeyPartClientA 2>)  7
	 - Client B (Incoming connection):
				Receivekey: Md5(<BaseKey 4><MagicValueServerClient 1><RandomKeyPartClientA 2>)  7
	 - Note: The first 1024 Bytes will be _NOT_ discarded for UDP keys to safe CPU time

	- Handshake
			-> The handshake is encrypted - except otherwise noted - by the Keys created above
			-> Padding is currently not used for UDP meaning that PaddingLen will be 0, using PaddingLens up to 16 Bytes is acceptable however
		Client A: <SemiRandomNotProtocolMarker 1[Unencrypted]><RandomKeyPart 2[Unencrypted]><MagicValue 4><PaddingLen 1><RandomBytes PaddingLen%16>

	- Overhead: 8 Bytes per UDP Packet

	- Security for Basic Obfuscation:
			- Random looking packets, very limited protection against passive eavesdropping single packets

	- Additional Comments:
			- For obvious reasons the UDP handshake is actually no handshake. If a different Encryption method (or better a different Key) is to be used this has to be negotiated in a TCP connection
			- SemiRandomNotProtocolMarker is a Byte which has a value unequal any Protocol header byte. This is a compromise, turning in complete randomness (and nice design) but gaining
			  a lower CPU usage

****************************** KAD Packets

	-Key creation Client <-> Client:
											(Used in general in request packets)
	 - Client A (Outgoing connection):
				Sendkey:	Md5(<KadID 16><RandomKeyPartClientA 2>)  18
	 - Client B (Incoming connection):
				Receivekey: Md5(<KadID 16><RandomKeyPartClientA 2>)  18
				   -- OR --					(Used in general in response packets)
	 - Client A (Outgoing connection):
				Sendkey:	Md5(<ReceiverKey 4><RandomKeyPartClientA 2>)  6
	 - Client B (Incoming connection):
				Receivekey: Md5(<ReceiverKey 4><RandomKeyPartClientA 2>)  6

	 - Note: The first 1024 Bytes will be _NOT_ discarded for UDP keys to safe CPU time

	- Handshake
			-> The handshake is encrypted - except otherwise noted - by the Keys created above
			-> Padding is currently not used for UDP meaning that PaddingLen will be 0, using PaddingLens up to 16 Bytes is acceptable however
		Client A: <SemiRandomNotProtocolMarker 6 Bits[Unencrypted]><Kad Marker 2Bit = 0 or 2><RandomKeyPart 2[Unencrypted]><MagicValue 4><PaddingLen 1><RandomBytes PaddingLen%16><ReceiverVerifyKey 4><SenderVerifyKey 4>

	- Overhead: 16 Bytes per UDP Packet

	- Kad/Ed2k Marker:
		 x 1	-> Most likely an ED2k Packet, try Userhash as Key first
		 0 0	-> Most likely a Kad Packet, try NodeID as Key first
		 1 0	-> Most likely a Kad Packet, try SenderKey as Key first

	- Additional Comments:
			- For obvious reasons the UDP handshake is actually no handshake. If a different Encryption method (or better a different Key) is to be used this has to be negotiated in a TCP connection
			- SemiRandomNotProtocolMarker is a Byte which has a value unequal any Protocol header byte. This is a compromise, turning in complete randomness (and nice design) but gaining
			  a lower CPU usage
			- Kad/Ed2k Marker are only indicators, which possibility could be tried first, and need not be trusted
			- Packets which use the senderkey are prone to BruteForce attacks, which take only a few minutes (2^32)
			  which is while not acceptable for encryption fair enough for obfuscation
*/

#include "stdafx.h"
#include "EncryptedDatagramSocket.h"
#include "emule.h"
#include "md5sum.h"
#include "Log.h"
#include "preferences.h"
#include "packets.h"
#include "Statistics.h"
#include "safefile.h"
#include "kademlia/kademlia/prefs.h"
#include "kademlia/kademlia/kademlia.h"
#include <cryptopp/osrng.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define CRYPT_HEADER_PADDING	0 // padding is currently disabled for UDP; the length in low 4 bits

#pragma pack(push, 1)
struct Crypt_Header_Struct
{
	uint8	byProtocol;
	uint16	wRandomKeyPart;
	uint32	dwMagic;								//encryption starts here
	uint8	byPadding[1 + CRYPT_HEADER_PADDING];	//length in low 4 bits of byPadding[0]
	uint32	dwReceiverKey;	// can be used only for send
	uint32	dwSenderKey;	// because length of padding may vary
};
// Send: this structure is good for fixed length padding
// Receive: data after byPadding[0] cannot be accessed safely because of variable length padding
#pragma pack(pop)

#define CRYPT_HEADER_KAD (sizeof(Crypt_Header_Struct))
#define CRYPT_HEADER_SIZE (CRYPT_HEADER_KAD - sizeof Crypt_Header_Struct::dwReceiverKey - sizeof Crypt_Header_Struct::dwSenderKey) //ed2k or client
#define CRYPT_HEADER_WITHOUTPADDING (CRYPT_HEADER_SIZE - CRYPT_HEADER_PADDING) //8

#define	MAGICVALUE_UDP					91
#define MAGICVALUE_UDP_SYNC_CLIENT		0x395F2EC1u
#define MAGICVALUE_UDP_SYNC_SERVER		0x13EF24D5u
#define	MAGICVALUE_UDP_SERVERCLIENT		0xA5
#define	MAGICVALUE_UDP_CLIENTSERVER		0x6B

static CryptoPP::AutoSeededRandomPool cryptRandomGen;

int CEncryptedDatagramSocket::DecryptReceivedClient(BYTE *pbyBufIn, int nBufLen, BYTE **ppbyBufOut, uint32 dwIP, uint32 *nReceiverVerifyKey, uint32 *nSenderVerifyKey)
{
	int nResult = nBufLen;
	*ppbyBufOut = pbyBufIn;

	if (nReceiverVerifyKey == NULL || nSenderVerifyKey == NULL) {
		ASSERT(0);
		return nResult;
	}
	*nReceiverVerifyKey = 0;
	*nSenderVerifyKey = 0;

	if (nResult <= CRYPT_HEADER_SIZE /*|| !thePrefs.IsClientCryptLayerSupported()*/)
		return nResult;

	Crypt_Header_Struct &crypt = *reinterpret_cast<Crypt_Header_Struct*>(pbyBufIn);
	switch (crypt.byProtocol) {
	case OP_EMULEPROT:
	case OP_KADEMLIAPACKEDPROT:
	case OP_KADEMLIAHEADER:
	case OP_UDPRESERVEDPROT1:
	case OP_UDPRESERVEDPROT2:
	case OP_PACKEDPROT:
		return nResult; // not an encrypted packet (see description on top)
	}

	// might be an encrypted packet, try to decrypt
	RC4_Key_Struct keyReceiveKey;
	// check the marker bit which type this packet could be and which key to test first
	// this is only an indicator since old clients have it set random
	// see the header for marker bits explanation
	byte byTries, byCurrentTry;
	uint32 dwValue = 0;
	if (Kademlia::CKademlia::GetPrefs() == NULL) {
		// if kad never run, no point in checking anything except ed2k encryption
		byTries = 1;
		byCurrentTry = 1;
	} else {
		byTries = 3;
		byCurrentTry = (crypt.byProtocol & 1) ? 1 : (crypt.byProtocol & 2);
	}
	bool bKadRecvKeyUsed = false;
	bool bKad = false;
	do {
		--byTries;
		MD5Sum md5;
		switch (byCurrentTry) {
		case 0: // kad packet with NodeID as key
			bKad = true;
			bKadRecvKeyUsed = false;
			if (Kademlia::CKademlia::GetPrefs()) {
				uchar achKeyData[18];
				memcpy(achKeyData, Kademlia::CKademlia::GetPrefs()->GetKadID().GetData(), 16);
				PokeUInt16(&achKeyData[16], crypt.wRandomKeyPart); // random key part sent from remote client
				md5.Calculate(achKeyData, sizeof achKeyData);
			}
			break;
		case 1: // ed2k packet
			bKad = false;
			bKadRecvKeyUsed = false;
			{
				uchar achKeyData[23];
				md4cpy(achKeyData, thePrefs.GetUserHash());
				PokeUInt32(&achKeyData[16], dwIP);
				achKeyData[20] = MAGICVALUE_UDP;
				PokeUInt16(&achKeyData[21], crypt.wRandomKeyPart); // random key part sent from remote client
				md5.Calculate(achKeyData, sizeof achKeyData);
			}
			break;
		case 2: // kad packet with ReceiverKey as key
			bKad = true;
			bKadRecvKeyUsed = true;
			if (Kademlia::CKademlia::GetPrefs()) {
				uchar achKeyData[6];
				PokeUInt32(achKeyData, Kademlia::CPrefs::GetUDPVerifyKey(dwIP));
				PokeUInt16(&achKeyData[4], crypt.wRandomKeyPart); // random key part sent from remote client
				md5.Calculate(achKeyData, sizeof achKeyData);
			}
			break;
		default:
			ASSERT(0);
		}

		RC4CreateKey(md5.GetRawHash(), MD5_DIGEST_SIZE, &keyReceiveKey, true);
		RC4Crypt((uchar*)&crypt.dwMagic, (uchar*)&dwValue, sizeof dwValue, &keyReceiveKey);
		byCurrentTry = (byCurrentTry + 1) % 3;
	} while (dwValue != MAGICVALUE_UDP_SYNC_CLIENT && byTries > 0); // try to decrypt as ed2k as well as kad packet if needed (max 3 rounds)

	if (dwValue == MAGICVALUE_UDP_SYNC_CLIENT) {
		// yup this is an encrypted packet
		// debug output notices
		// the following cases are "allowed" but shouldn't happen given that there is only our implementation yet
		if (bKad) {
			LPCTSTR p = NULL;
			if (crypt.byProtocol & 0x01)
				p = _T("ed2k bit");
			else if (!bKadRecvKeyUsed && (crypt.byProtocol & 0x02))
				p = _T("nodeid key, recvkey");
			else if (bKadRecvKeyUsed && !(crypt.byProtocol & 0x02))
				p = _T("recvkey key, nodeid");
			if (p)
				DebugLog(_T("Received obfuscated UDP packet from clientIP: %s with wrong key marker bits (kad packet, %s bit)"), (LPCTSTR)ipstr(dwIP), p);
		}
		RC4Crypt((uchar*)crypt.byPadding, 1, &keyReceiveKey);
		nResult -= CRYPT_HEADER_WITHOUTPADDING;
		crypt.byPadding[0] &= 0xf;
		if (nResult <= crypt.byPadding[0]) {
			DebugLogError(_T("Invalid obfuscated UDP packet from clientIP: %s, Paddingsize (%u) larger than received bytes"), (LPCTSTR)ipstr(dwIP), crypt.byPadding[0]);
			return nBufLen; // pass through, let the Receive function do the error handling on this junk
		}
		if (crypt.byPadding[0] > 0) {
			RC4Crypt(NULL, crypt.byPadding[0], &keyReceiveKey);
			nResult -= crypt.byPadding[0];
		}

		if (bKad) {
			if (nResult <= 8) {
				DebugLogError(_T("Obfuscated Kad packet with mismatching size (verify keys missing) received from clientIP: %s"), (LPCTSTR)ipstr(dwIP));
				return nBufLen; // pass through, let the Receive function do the error handling on this junk;
			}
			// read the verify keys
			RC4Crypt((uchar*)&pbyBufIn[nBufLen - nResult], (uchar*)nReceiverVerifyKey, 4, &keyReceiveKey);
			RC4Crypt((uchar*)&pbyBufIn[nBufLen - nResult] + 4, (uchar*)nSenderVerifyKey, 4, &keyReceiveKey);
			nResult -= 8; //verify keys
		}
		*ppbyBufOut = &pbyBufIn[nBufLen - nResult];
		RC4Crypt((uchar*)*ppbyBufOut, nResult, &keyReceiveKey);
		theStats.AddDownDataOverheadCrypt(nBufLen - nResult);
		//DEBUG_ONLY( DebugLog(_T("Received obfuscated UDP packet from clientIP: %s, Key: %s, RKey: %u, SKey: %u"), (LPCTSTR)ipstr(dwIP), bKad ? (bKadRecvKeyUsed ? _T("ReceiverKey") : _T("NodeID")) : _T("UserHash")
		//	, nReceiverVerifyKey != 0 ? *nReceiverVerifyKey : 0, nSenderVerifyKey != 0 ? *nSenderVerifyKey : 0) );
	} else // pass through, let the Receive function do the error handling on this junk
		DebugLogWarning(_T("Obfuscated packet expected but magicvalue mismatch on UDP packet from clientIP: %s, Possible RecvKey: %u"), (LPCTSTR)ipstr(dwIP), Kademlia::CPrefs::GetUDPVerifyKey(dwIP));
	return nResult;
}

// Encrypt packet. Key used:
// pachClientHashOrKadID != NULL									-> pachClientHashOrKadID
// pachClientHashOrKadID == NULL && bKad && nReceiverVerifyKey != 0 -> nReceiverVerifyKey
// else																-> ASSERT
uint32 CEncryptedDatagramSocket::EncryptSendClient(uchar *pbyBuf, uint32 nBufLen, const uchar *pachClientHashOrKadID, bool bKad, uint32 nReceiverVerifyKey, uint32 nSenderVerifyKey)
{
	ASSERT(theApp.GetPublicIP() != 0 || bKad);
	ASSERT(thePrefs.IsClientCryptLayerSupported());
	ASSERT(pachClientHashOrKadID != NULL || nReceiverVerifyKey != 0);
	ASSERT((nReceiverVerifyKey == 0 && nSenderVerifyKey == 0) || bKad);

	uint8 byKadRecKeyUsed = 0; //nodeid marker
	const uint16 nRandomKeyPart = (uint16)cryptRandomGen.GenerateWord32(0x0000, 0xFFFFu);
	MD5Sum md5;
	if (bKad) {
		if ((pachClientHashOrKadID == NULL || isnulmd4(pachClientHashOrKadID)) && nReceiverVerifyKey != 0) {
			byKadRecKeyUsed = 2; //reckey marker
			uchar achKeyData[6];
			PokeUInt32(achKeyData, nReceiverVerifyKey);
			PokeUInt16(&achKeyData[4], nRandomKeyPart);
			md5.Calculate(achKeyData, sizeof achKeyData);
			//DEBUG_ONLY( DebugLog(_T("Creating obfuscated Kad packet encrypted by ReceiverKey (%u)"), nReceiverVerifyKey) );
		} else if (pachClientHashOrKadID != NULL && !isnulmd4(pachClientHashOrKadID)) {
			uchar achKeyData[18];
			md4cpy(achKeyData, pachClientHashOrKadID);
			PokeUInt16(&achKeyData[16], nRandomKeyPart);
			md5.Calculate(achKeyData, sizeof achKeyData);
			//DEBUG_ONLY( DebugLog(_T("Creating obfuscated Kad packet encrypted by Hash/NodeID %s"), (LPCTSTR)md4str(pachClientHashOrKadID)) );
		} else {
			ASSERT(0);
			return nBufLen;
		}
	} else {
		uchar achKeyData[23];
		md4cpy(achKeyData, pachClientHashOrKadID);
		PokeUInt32(&achKeyData[16], theApp.GetPublicIP());
		achKeyData[20] = MAGICVALUE_UDP;
		PokeUInt16(&achKeyData[21], nRandomKeyPart);
		md5.Calculate(achKeyData, sizeof achKeyData);
	}
	RC4_Key_Struct keySendKey;
	RC4CreateKey(md5.GetRawHash(), MD5_DIGEST_SIZE, &keySendKey, true);

	// create the semi-random byte encryption header
	uint8 bySemiRandomNotProtocolMarker;
	for (int i = 32; i > 0; --i) {
		bySemiRandomNotProtocolMarker = cryptRandomGen.GenerateByte();
		if (bKad) {
			bySemiRandomNotProtocolMarker &= ~3;				// clear marker bits
			bySemiRandomNotProtocolMarker |= byKadRecKeyUsed;	//set kad reckey/nodeid marker bit
		}  else
			bySemiRandomNotProtocolMarker |= 1;					// set the ed2k marker bit

		switch (bySemiRandomNotProtocolMarker) {
		case OP_EMULEPROT:
		case OP_KADEMLIAPACKEDPROT:
		case OP_KADEMLIAHEADER:
		case OP_UDPRESERVEDPROT1:
		case OP_UDPRESERVEDPROT2:
		case OP_PACKEDPROT:
			bySemiRandomNotProtocolMarker = OP_EMULEPROT;
			continue; // not allowed values
		}
		break;
	}
	if (bySemiRandomNotProtocolMarker == OP_EMULEPROT) {
		// either we have _really_ bad luck, or the random generator is a bit messed up
		ASSERT(0);
		bySemiRandomNotProtocolMarker = static_cast<uint8>(!bKad) | byKadRecKeyUsed;
	}

	Crypt_Header_Struct &crypt = *reinterpret_cast<Crypt_Header_Struct*>(pbyBuf);
	crypt.byProtocol = bySemiRandomNotProtocolMarker;
	crypt.wRandomKeyPart = nRandomKeyPart;
	crypt.dwMagic = MAGICVALUE_UDP_SYNC_CLIENT;
	crypt.byPadding[0] = CRYPT_HEADER_PADDING;
	for (int i = CRYPT_HEADER_PADDING; i > 0; --i)
		crypt.byPadding[i] = (uchar)rand();	// they actually don't really need to be random, but it doesn't hurt either
	if (bKad) {
		crypt.dwReceiverKey = nReceiverVerifyKey;
		crypt.dwSenderKey = nSenderVerifyKey;
	}

	const uint32 nCryptHeaderLen = EncryptOverheadSize(bKad);
	nBufLen += nCryptHeaderLen;
	RC4Crypt((uchar*)&crypt.dwMagic, nBufLen - 3, &keySendKey);

	theStats.AddUpDataOverheadCrypt(nCryptHeaderLen);
	return nBufLen;
}

int CEncryptedDatagramSocket::DecryptReceivedServer(BYTE *pbyBufIn, int nBufLen, BYTE **ppbyBufOut, uint32 dwBaseKey, uint32 dbgIP)
{
	int nResult = nBufLen;
	*ppbyBufOut = pbyBufIn;

	if (nResult <= CRYPT_HEADER_SIZE || !thePrefs.IsServerCryptLayerUDPEnabled() || dwBaseKey == 0)
		return nResult;

	Crypt_Header_Struct &crypt = *reinterpret_cast<Crypt_Header_Struct*>(pbyBufIn);
	if (crypt.byProtocol == OP_EDONKEYPROT)
		return nResult; // not an encrypted packet (see description on top)

	// might be an encrypted packet, try to decrypt
	uchar achKeyData[7];
	PokeUInt32(achKeyData, dwBaseKey);
	achKeyData[4] = MAGICVALUE_UDP_SERVERCLIENT;
	PokeUInt16(&achKeyData[5], crypt.wRandomKeyPart); // random key part sent from remote server
	MD5Sum md5(achKeyData, sizeof achKeyData);
	RC4_Key_Struct keyReceiveKey;
	RC4CreateKey(md5.GetRawHash(), MD5_DIGEST_SIZE, &keyReceiveKey, true);

	RC4Crypt((uchar*)&crypt.dwMagic, 4, &keyReceiveKey);
	if (crypt.dwMagic == MAGICVALUE_UDP_SYNC_SERVER) {
		// yup this is an encrypted packet
		if (thePrefs.GetDebugServerUDPLevel() > 0)
			DEBUG_ONLY(DebugLog(_T("Received obfuscated UDP packet from ServerIP: %s"), (LPCTSTR)ipstr(dbgIP)));
		RC4Crypt((uchar*)crypt.byPadding, 1, &keyReceiveKey);
		crypt.byPadding[0] &= 0xf;
		nResult -= CRYPT_HEADER_WITHOUTPADDING;
		if (nResult <= crypt.byPadding[0]) {
			DebugLogError(_T("Invalid obfuscated UDP packet from ServerIP: %s, Padding size (%u) larger than received bytes"), (LPCTSTR)ipstr(dbgIP), crypt.byPadding[0]);
			return nBufLen; // pass through, let the Receive function do the error handling on this junk
		}
		if (crypt.byPadding[0] > 0) {
			RC4Crypt(NULL, crypt.byPadding[0], &keyReceiveKey);
			nResult -= crypt.byPadding[0];
		}
		*ppbyBufOut = &pbyBufIn[nBufLen - nResult];
		RC4Crypt((uchar*)*ppbyBufOut, nResult, &keyReceiveKey);

		theStats.AddDownDataOverheadCrypt(nBufLen - nResult);
	} else // pass through, let the Receive function do the error handling on this junk
		DebugLogWarning(_T("Obfuscated packet expected but magicvalue mismatch on UDP packet from ServerIP: %s"), (LPCTSTR)ipstr(dbgIP));
	return nResult;
}

uint32 CEncryptedDatagramSocket::EncryptSendServer(uchar *pbyBuf, uint32 nBufLen, uint32 dwBaseKey)
{
	ASSERT(thePrefs.IsServerCryptLayerUDPEnabled());
	ASSERT(dwBaseKey != 0);

	const uint16 nRandomKeyPart = (uint16)cryptRandomGen.GenerateWord32(0x0000, 0xFFFFu);

	uchar achKeyData[7];
	PokeUInt32(achKeyData, dwBaseKey);
	achKeyData[4] = MAGICVALUE_UDP_CLIENTSERVER;
	PokeUInt16(&achKeyData[5], nRandomKeyPart);
	MD5Sum md5(achKeyData, sizeof achKeyData);
	RC4_Key_Struct keySendKey;
	RC4CreateKey(md5.GetRawHash(), MD5_DIGEST_SIZE, &keySendKey, true);

	// create the semi-random byte encryption header
	uint8 bySemiRandomNotProtocolMarker;
	for (int i = 8; i > 0; --i) {
		bySemiRandomNotProtocolMarker = cryptRandomGen.GenerateByte();
		if (bySemiRandomNotProtocolMarker != OP_EDONKEYPROT) // not allowed value
			break;
	}
	if (bySemiRandomNotProtocolMarker == OP_EDONKEYPROT) {
		// either we have _really_ bad luck, or the random generator is a bit messed up
		ASSERT(0);
		bySemiRandomNotProtocolMarker = 0x01;
	}

	Crypt_Header_Struct &crypt = *reinterpret_cast<Crypt_Header_Struct*>(pbyBuf);
	crypt.byProtocol = bySemiRandomNotProtocolMarker;
	crypt.wRandomKeyPart = nRandomKeyPart;
	crypt.dwMagic = MAGICVALUE_UDP_SYNC_SERVER;
	crypt.byPadding[0] = CRYPT_HEADER_PADDING;
	for (int i = CRYPT_HEADER_PADDING; i > 0; --i)
		crypt.byPadding[i] = (uchar)rand();	// padding don't really need to be random, but it doesn't hurt either

	const uint32 nCryptHeaderLen = EncryptOverheadSize(false);
	nBufLen += nCryptHeaderLen;
	RC4Crypt((uchar*)&crypt.dwMagic, nBufLen - 3, &keySendKey);

	theStats.AddUpDataOverheadCrypt(nCryptHeaderLen);
	return nBufLen;
}

int CEncryptedDatagramSocket::EncryptOverheadSize(bool bKad)
{
	return bKad ? CRYPT_HEADER_KAD : CRYPT_HEADER_SIZE;
}