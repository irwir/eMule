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
#include "MapKey.h"
#include <cryptopp/rsa.h>

#define	 MAXPUBKEYSIZE		80

#define CRYPT_CIP_REMOTECLIENT	10
#define CRYPT_CIP_LOCALCLIENT	20
#define CRYPT_CIP_NONECLIENT	30

#pragma pack(push, 1)
struct CreditStruct_29a
{
	uchar		abyKey[16];
	uint32		nUploadedLo;	// uploaded TO him
	uint32		nDownloadedLo;	// downloaded from him
	uint32		nLastSeen;
	uint32		nUploadedHi;	// upload high 32
	uint32		nDownloadedHi;	// download high 32
	uint16		nReserved3;
};
struct CreditStruct
{
	uchar		abyKey[MDX_DIGEST_SIZE];
	uint32		nUploadedLo;	// uploaded TO him
	uint32		nDownloadedLo;	// downloaded from him
	uint32		nLastSeen;
	uint32		nUploadedHi;	// upload high 32
	uint32		nDownloadedHi;	// download high 32
	uint16		nReserved3;
	uint8		nKeySize;
	uchar		abySecureIdent[MAXPUBKEYSIZE];
};
#pragma pack(pop)

enum EIdentState
{
	IS_NOTAVAILABLE,
	IS_IDNEEDED,
	IS_IDENTIFIED,
	IS_IDFAILED,
	IS_IDBADGUY,
};

class CClientCredits
{
	friend class CClientCreditsList;
public:
	explicit CClientCredits(CreditStruct *in_credits);
	explicit CClientCredits(const uchar *key);
	~CClientCredits();
	CClientCredits(const CClientCredits&) = delete;
	CClientCredits& operator=(const CClientCredits&) = delete;

	const uchar*	GetKey() const				{ return m_pCredits->abyKey; }
	uchar*	GetSecureIdent()					{ return m_abyPublicKey; }
	uint8	GetSecIDKeyLen() const				{ return m_nPublicKeyLen; }
	CreditStruct*	GetDataStruct() const		{ return m_pCredits; }
	void	ClearWaitStartTime();
	void	AddDownloaded(uint32 bytes, uint32 dwForIP);
	void	AddUploaded(uint32 bytes, uint32 dwForIP);
	uint64	GetUploadedTotal() const;
	uint64	GetDownloadedTotal() const;
	float	GetScoreRatio(uint32 dwForIP) const;
	void	SetLastSeen()						{ m_pCredits->nLastSeen = static_cast<uint32>(time(NULL)); }
	bool	SetSecureIdent(const uchar *pachIdent, uint8 nIdentLen); // Public key cannot change, use only if there is not public key yet
	uint32	m_dwCryptRndChallengeFor;
	uint32	m_dwCryptRndChallengeFrom;
	EIdentState	GetCurrentIdentState(uint32 dwForIP) const; // can be != IdentState
	uint32	GetSecureWaitStartTime(uint32 dwForIP);
	void	SetSecWaitStartTime(uint32 dwForIP);
protected:
	void	Verified(uint32 dwForIP);
	EIdentState IdentState;
private:
	void	InitalizeIdent();
	CreditStruct	*m_pCredits;
	uint32	m_dwIdentIP;
	uint32	m_dwSecureWaitTime;
	uint32	m_dwUnSecureWaitTime;
	uint32	m_dwWaitTimeIP;			// client IP assigned to the waittime
	byte	m_abyPublicKey[80];		// even keys which are not verified will be stored here, and - if verified - copied into the struct
	uint8	m_nPublicKeyLen;
};

class CClientCreditsList
{
public:
	CClientCreditsList();
	~CClientCreditsList();

			// return signature size, 0 = Failed | use sigkey param for debug only
	uint8	CreateSignature(CClientCredits *pTarget, uchar *pachOutput, uint8 nMaxSize, uint32 ChallengeIP, uint8 byChaIPKind, CryptoPP::RSASSA_PKCS1v15_SHA_Signer *sigkey = NULL) const;
	bool	VerifyIdent(CClientCredits *pTarget, const uchar *pachSignature, uint8 nInputSize, uint32 dwForIP, uint8 byChaIPKind);

	CClientCredits* GetCredit(const uchar *key);
	void	Process();
	uint8	GetPubKeyLen() const			{ return m_nMyPublicKeyLen; }
	byte*	GetPublicKey()					{ return m_abyMyPublicKey; }
	bool	CryptoAvailable() const;
protected:
	void	LoadList();
	void	SaveList();
	void	InitalizeCrypting();
	static bool CreateKeyPair();
#ifdef _DEBUG
	bool	Debug_CheckCrypting();
#endif
private:
	CMap<CCKey, const CCKey&, CClientCredits*, CClientCredits*> m_mapClients;
	CryptoPP::RSASSA_PKCS1v15_SHA_Signer *m_pSignkey;
	uint32			m_nLastSaved;
	byte			m_abyMyPublicKey[80];
	uint8			m_nMyPublicKeyLen;
};