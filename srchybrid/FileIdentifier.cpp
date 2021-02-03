//this file is part of eMule
//Copyright (C)2002-2010 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / http://www.emule-project.net )
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
#include "StdAfx.h"
#include "fileidentifier.h"
#include "otherfunctions.h"
#include "safefile.h"
#include "knownfile.h"
#include "log.h"

// File size       Data parts      ED2K parts      ED2K part hashes		AICH part hashes
// -------------------------------------------------------------------------------------------
// 1..PARTSIZE-1   1               1               0(!)					0 (!)
// PARTSIZE        1               2(!)            2(!)					0 (!)
// PARTSIZE+1      2               2               2					2
// PARTSIZE*2      2               3(!)            3(!)					2
// PARTSIZE*2+1    3               3               3					3

///////////////////////////////////////////////////////////////////////////////////////////////
// CFileIdentifierBase
CFileIdentifierBase::CFileIdentifierBase()
	: m_abyMD4Hash()
	, m_bHasValidAICHHash()
{
}

CFileIdentifierBase::CFileIdentifierBase(const CFileIdentifierBase &rFileIdentifier)
{
	*this = rFileIdentifier;
}

CFileIdentifierBase& CFileIdentifierBase::operator=(const CFileIdentifierBase &rFileIdentifier)
{
	md4cpy(m_abyMD4Hash, rFileIdentifier.m_abyMD4Hash);
	m_AICHFileHash = rFileIdentifier.m_AICHFileHash;
	m_bHasValidAICHHash = rFileIdentifier.m_bHasValidAICHHash;
	return *this;
}

// TODO: Remove me
EMFileSize CFileIdentifierBase::GetFileSize() const
{
	ASSERT(0);
	return 0ull;
}

void CFileIdentifierBase::SetMD4Hash(const uchar *pucFileHash)
{
	md4cpy(m_abyMD4Hash, pucFileHash);
}

void CFileIdentifierBase::SetMD4Hash(CFileDataIO *pFile)
{
	pFile->ReadHash16(m_abyMD4Hash);
}

void CFileIdentifierBase::SetAICHHash(const CAICHHash &Hash)
{
	m_AICHFileHash = Hash;
	m_bHasValidAICHHash = true;
}

bool CFileIdentifierBase::CompareRelaxed(const CFileIdentifierBase &rFileIdentifier) const
{
	ASSERT(!isnulmd4(m_abyMD4Hash));
	ASSERT(!isnulmd4(rFileIdentifier.m_abyMD4Hash));
	return md4equ(m_abyMD4Hash, rFileIdentifier.m_abyMD4Hash)
		&& (!(uint64)GetFileSize() || !(uint64)rFileIdentifier.GetFileSize() || GetFileSize() == rFileIdentifier.GetFileSize())
		&& (!m_bHasValidAICHHash || !rFileIdentifier.m_bHasValidAICHHash || m_AICHFileHash == rFileIdentifier.m_AICHFileHash);
}

bool CFileIdentifierBase::CompareStrict(const CFileIdentifierBase &rFileIdentifier) const
{
	ASSERT(!isnulmd4(m_abyMD4Hash));
	ASSERT(!isnulmd4(rFileIdentifier.m_abyMD4Hash));
	return md4equ(m_abyMD4Hash, rFileIdentifier.m_abyMD4Hash)
		&& GetFileSize() == rFileIdentifier.GetFileSize()
		&& !(m_bHasValidAICHHash ^ rFileIdentifier.m_bHasValidAICHHash)
		&& m_AICHFileHash == rFileIdentifier.m_AICHFileHash;
}

void CFileIdentifierBase::WriteIdentifier(CFileDataIO *pFile, bool bKadExcludeMD4) const
{
	ASSERT(!isnulmd4(m_abyMD4Hash));
	ASSERT(GetFileSize() != 0ull);
	const UINT uIncludesMD4 = static_cast<UINT>(!bKadExcludeMD4); // This is (currently) mandatory except for Kad
	const UINT uIncludesSize = static_cast<UINT>(GetFileSize() != 0ull);
	const UINT uIncludesAICH = static_cast<UINT>(HasAICHHash());
	const UINT uMandatoryOptions = 0; // RESERVED - Identifier invalid if we encountered an unknown options
	const UINT uOptions = 0; // RESERVED

	uint8 byIdentifierDesc = (uint8)
			((uOptions			<< 5) |
			(uMandatoryOptions	<< 3) |
			(uIncludesAICH		<< 2) |
			(uIncludesSize		<< 1) |
			(uIncludesMD4		<< 0));
//DebugLog(_T("Write IdentifierDesc: %u"), byIdentifierDesc);
	pFile->WriteUInt8(byIdentifierDesc);
	if (!bKadExcludeMD4)
		pFile->WriteHash16(m_abyMD4Hash);
	if (GetFileSize() != 0ull)
		pFile->WriteUInt64(GetFileSize());
	if (HasAICHHash())
		m_AICHFileHash.Write(pFile);
}

CString CFileIdentifierBase::DbgInfo() const
{
	//TODO fileident
	return CString();
}
///////////////////////////////////////////////////////////////////////////////////////////////
// CFileIdentifier
CFileIdentifier::CFileIdentifier(EMFileSize &rFileSize)
	: m_rFileSize(rFileSize)
{
}

CFileIdentifier::CFileIdentifier(const CFileIdentifier &rFileIdentifier, EMFileSize &rFileSize)
	: CFileIdentifierBase(rFileIdentifier)
	, m_rFileSize(rFileSize)
{
	for (INT_PTR i = 0; i < rFileIdentifier.m_aMD4HashSet.GetCount(); ++i) {
		uchar *pucHashSetPart = new uchar[MDX_DIGEST_SIZE];
		md4cpy(pucHashSetPart, rFileIdentifier.m_aMD4HashSet[i]);
		m_aMD4HashSet.Add(pucHashSetPart);
	}
	for (INT_PTR i = 0; i < rFileIdentifier.m_aAICHPartHashSet.GetCount(); ++i)
		m_aAICHPartHashSet.Add(rFileIdentifier.m_aAICHPartHashSet[i]);
}

CFileIdentifier::~CFileIdentifier()
{
	DeleteMD4Hashset();
}

bool CFileIdentifier::CalculateMD4HashByHashSet(bool bVerifyOnly, bool bDeleteOnVerifyFail)
{
	if (m_aMD4HashSet.GetCount() <= 1) {
		ASSERT(0);
		return false;
	}
	uchar *buffer = new uchar[m_aMD4HashSet.GetCount() * MDX_DIGEST_SIZE];
	for (INT_PTR i = m_aMD4HashSet.GetCount(); --i >= 0;)
		md4cpy(buffer + (i * MDX_DIGEST_SIZE), m_aMD4HashSet[i]);
	uchar aucResult[MDX_DIGEST_SIZE];
	CKnownFile::CreateHash(buffer, (uint32)m_aMD4HashSet.GetCount() * MDX_DIGEST_SIZE, aucResult);
	delete[] buffer;
	if (bVerifyOnly) {
		if (!md4equ(aucResult, m_abyMD4Hash)) {
			if (bDeleteOnVerifyFail)
				DeleteMD4Hashset();
			return false;
		}
	} else
		md4cpy(m_abyMD4Hash, aucResult);
	return true;
}

bool CFileIdentifier::LoadMD4HashsetFromFile(CFileDataIO *file, bool bVerifyExistingHash)
{
	uchar checkid[MDX_DIGEST_SIZE];
	file->ReadHash16(checkid);
	//TRACE("File size: %u (%u full parts + %u bytes)\n", GetFileSize(), GetFileSize()/PARTSIZE, GetFileSize()%PARTSIZE);
	//TRACE("File hash: %s\n", (LPCTSTR)md4str(checkid));
	ASSERT(m_aMD4HashSet.IsEmpty());
	ASSERT(!isnulmd4(m_abyMD4Hash) || !bVerifyExistingHash);
	DeleteMD4Hashset();

	uint16 parts = file->ReadUInt16();
	//TRACE("Nr. hashes: %u\n", (UINT)parts);
	if (bVerifyExistingHash && (!md4equ(m_abyMD4Hash, checkid) || parts != GetTheoreticalMD4PartHashCount()))
		return false;
	for (uint16 i = 0; i < parts; ++i) {
		uchar *cur_hash = new uchar[MDX_DIGEST_SIZE];
		file->ReadHash16(cur_hash);
		//TRACE("Hash[%3u]: %s\n", i, (LPCTSTR)md4str(cur_hash));
		m_aMD4HashSet.Add(cur_hash);
	}

	if (!bVerifyExistingHash)
		md4cpy(m_abyMD4Hash, checkid);

	// Calculate hash out of hashset and compare to existing file hash
	return m_aMD4HashSet.IsEmpty() || CalculateMD4HashByHashSet(true, true);
}

bool CFileIdentifier::SetMD4HashSet(const CArray<uchar*, uchar*> &aHashset)
{
	// delete hashset
	DeleteMD4Hashset();

	// set new hash
	for (int i = 0; i < aHashset.GetCount(); ++i) {
		uchar *pucHash = new uchar[MDX_DIGEST_SIZE];
		md4cpy(pucHash, aHashset[i]);
		m_aMD4HashSet.Add(pucHash);
	}

	// verify new hash
	return m_aMD4HashSet.IsEmpty() || CalculateMD4HashByHashSet(true, true);
}

uchar* CFileIdentifier::GetMD4PartHash(UINT part) const
{
	if (part >= (UINT)m_aMD4HashSet.GetCount())
		return NULL;
	return m_aMD4HashSet[part];
}

// nr. of part hashes according the file size wrt ED2K protocol
// nr. of parts to be used with OP_HASHSETANSWER
uint16 CFileIdentifier::GetTheoreticalMD4PartHashCount() const
{
	if (!(uint64)m_rFileSize) {
		ASSERT(0);
		return 0;
	}
	uint16 uResult = (uint16)((uint64)m_rFileSize / PARTSIZE);
	return uResult + static_cast<uint16>(uResult > 0);
}

void CFileIdentifier::WriteMD4HashsetToFile(CFileDataIO *pFile) const
{
	ASSERT(!isnulmd4(m_abyMD4Hash));
	pFile->WriteHash16(m_abyMD4Hash);
	UINT uParts = (UINT)m_aMD4HashSet.GetCount();
	pFile->WriteUInt16((uint16)uParts);
	for (UINT i = 0; i < uParts; ++i)
		pFile->WriteHash16(m_aMD4HashSet[i]);
}

void CFileIdentifier::WriteHashSetsToPacket(CFileDataIO *pFile, bool bMD4, bool bAICH) const
{
	// 6 Options - RESERVED
	// 1 AICH HashSet
	// 1 MD4 HashSet
	uint8 byOptions = 0;
	if (bMD4) {
		if (GetTheoreticalMD4PartHashCount() == 0) {
			bMD4 = false;
			DebugLogWarning(_T("CFileIdentifier::WriteHashSetsToPacket - requested zero sized MD4 HashSet"));
		} else if (HasExpectedMD4HashCount())
			byOptions |= 0x01;
		else {
			bMD4 = false;
			DebugLogError(_T("CFileIdentifier::WriteHashSetsToPacket - unable to write MD4 HashSet"));
		}
	}
	if (bAICH) {
		if (GetTheoreticalAICHPartHashCount() == 0) {
			bAICH = false;
			DebugLogWarning(_T("CFileIdentifier::WriteHashSetsToPacket - requested zero-sized AICH HashSet"));
		} else if (HasExpectedAICHHashCount() && HasAICHHash())
			byOptions |= 0x02;
		else {
			bAICH = false;
			DEBUG_ONLY(DebugLog(_T("CFileIdentifier::WriteHashSetsToPacket - unable to write AICH HashSet")));
		}
	}
	pFile->WriteUInt8(byOptions);
	if (bMD4)
		WriteMD4HashsetToFile(pFile);
	if (bAICH)
		WriteAICHHashsetToFile(pFile);
}

bool CFileIdentifier::ReadHashSetsFromPacket(CFileDataIO *pFile, bool &rbMD4, bool &rbAICH)
{
	ASSERT(rbMD4 || rbAICH);
	uint8 byOptions = pFile->ReadUInt8();
	bool bMD4Present = (byOptions & 0x01) > 0;
	bool bAICHPresent = (byOptions & 0x02) > 0;
	// We don't abort on unknown option, because even if there is another unknown hashset, there is no data
	// afterwards we try to read on the only occasion this function is used. So we might be able to add
	// an optional flags in the future without having to adjust the protocol any further
	// (new additional data/hashes should not be appended without adjustments, however)
	if ((byOptions >> 2) > 0)
		DebugLogWarning(_T("Unknown Options/HashSets set in CFileIdentifier::ReadHashSetsFromPacket"));

	if (bMD4Present && !rbMD4) {
		DebugLogWarning(_T("CFileIdentifier::ReadHashSetsFromPacket: MD4 HashSet present but unrequested"));
		// Even if we don't want it, we still have to read the file to skip it
		uchar tmpHash[MDX_DIGEST_SIZE];
		pFile->ReadHash16(tmpHash);
		for (int i = pFile->ReadUInt16(); --i >= 0;)
			pFile->ReadHash16(tmpHash);
	} else if (!bMD4Present)
		rbMD4 = false;
	else if (/*bMD4Present && */rbMD4) {
		if (!LoadMD4HashsetFromFile(pFile, true)) {	// corrupt
			rbMD4 = false;
			rbAICH = false;
			return false;
		}
	}

	if (bAICHPresent && !rbAICH) {
		DebugLogWarning(_T("CFileIdentifier::ReadHashSetsFromPacket: AICH HashSet present but unrequested"));
		// Even if we don't want it, we still have to read the file to skip it
		CAICHHash tmpHash(pFile);
		for (int i = pFile->ReadUInt16(); --i >= 0;)
			tmpHash.Read(pFile);
	} else if (!bAICHPresent || !HasAICHHash()) {
		ASSERT(!bAICHPresent);
		rbAICH = false;
	} else if (/*bAICHPresent && */rbAICH) {
		if (!LoadAICHHashsetFromFile(pFile, true)) {	// corrupt
			if (rbMD4) {
				DeleteMD4Hashset();
				rbMD4 = false;
			}
			rbAICH = false;
			return false;
		}
	}
	return true;
}

void CFileIdentifier::DeleteMD4Hashset()
{
	for (int i = (int)m_aMD4HashSet.GetCount(); --i >= 0;)
		delete[] m_aMD4HashSet[i];
	m_aMD4HashSet.RemoveAll();
}

uint16 CFileIdentifier::GetTheoreticalAICHPartHashCount() const
{
	return (m_rFileSize <= PARTSIZE) ? 0 : (uint16)(((uint64)m_rFileSize + PARTSIZE - 1) / PARTSIZE);
}

bool CFileIdentifier::SetAICHHashSet(const CAICHRecoveryHashSet &sourceHashSet)
{
	ASSERT(m_bHasValidAICHHash);
	if (sourceHashSet.GetStatus() != AICH_HASHSETCOMPLETE || sourceHashSet.GetMasterHash() != m_AICHFileHash) {
		ASSERT(0);
		DebugLogError(_T("Unexpected error SetAICHHashSet(), AICHPartHashSet not loaded"));
		return false;
	}
	return sourceHashSet.GetPartHashes(m_aAICHPartHashSet) && HasExpectedAICHHashCount();
}

bool CFileIdentifier::SetAICHHashSet(const CFileIdentifier &rSourceHashSet)
{
	if (!rSourceHashSet.HasAICHHash() || !rSourceHashSet.HasExpectedAICHHashCount()) {
		ASSERT(0);
		return false;
	}
	m_aAICHPartHashSet.RemoveAll();
	for (int i = 0; i < rSourceHashSet.m_aAICHPartHashSet.GetCount(); ++i)
		m_aAICHPartHashSet.Add(rSourceHashSet.m_aAICHPartHashSet[i]);
	ASSERT(HasExpectedAICHHashCount());
	return HasExpectedAICHHashCount();
}

bool CFileIdentifier::LoadAICHHashsetFromFile(CFileDataIO *pFile, bool bVerify)
{
	ASSERT(m_aAICHPartHashSet.IsEmpty());
	m_aAICHPartHashSet.RemoveAll();
	CAICHHash masterHash(pFile);
	if (HasAICHHash() && masterHash != m_AICHFileHash) {
		ASSERT(0);
		DebugLogError(_T("Loading AICH Part Hashset error: HashSet Masterhash doesn't matches with existing masterhash - hashset not loaded"));
		return false;
	}
	uint16 nCount = pFile->ReadUInt16();
	for (int i = 0; i < nCount; ++i)
		m_aAICHPartHashSet.Add(CAICHHash(pFile));
	if (bVerify)
		return VerifyAICHHashSet();
	return true;
}

void CFileIdentifier::WriteAICHHashsetToFile(CFileDataIO *pFile) const
{
	ASSERT(HasAICHHash());
	ASSERT(HasExpectedAICHHashCount());
	m_AICHFileHash.Write(pFile);
	UINT uParts = (UINT)m_aAICHPartHashSet.GetCount();
	pFile->WriteUInt16((uint16)uParts);
	for (UINT i = 0; i < uParts; ++i)
		m_aAICHPartHashSet[i].Write(pFile);
}

bool CFileIdentifier::VerifyAICHHashSet()
{
	if (m_rFileSize == 0ull || !m_bHasValidAICHHash) {
		ASSERT(0);
		return false;
	}
	if (!HasExpectedAICHHashCount())
		return false;
	CAICHRecoveryHashSet tmpAICHHashSet(NULL, m_rFileSize);
	tmpAICHHashSet.SetMasterHash(m_AICHFileHash, AICH_HASHSETCOMPLETE);

	uint32 uPartCount = (uint16)(((uint64)m_rFileSize + (PARTSIZE - 1)) / PARTSIZE);
	if (uPartCount <= 1)
		return true; // No AICH Part Hashes
	for (uint32 nPart = 0; nPart < uPartCount; ++nPart) {
		uint64 nPartStartPos = (uint64)nPart * PARTSIZE;
		uint32 nPartSize = (uint32)min(PARTSIZE, (uint64)GetFileSize() - nPartStartPos);
		CAICHHashTree *pPartHashTree = tmpAICHHashSet.m_pHashTree.FindHash(nPartStartPos, nPartSize);
		if (pPartHashTree != NULL) {
			pPartHashTree->m_Hash = m_aAICHPartHashSet[nPart];
			pPartHashTree->m_bHashValid = true;
		} else {
			ASSERT(0);
			return false;
		}
	}
	if (!tmpAICHHashSet.VerifyHashTree(false)) {
		m_aAICHPartHashSet.RemoveAll();
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// CFileIdentifierSA

CFileIdentifierSA::CFileIdentifierSA()
	: m_nFileSize(0ull)
{
}

CFileIdentifierSA::CFileIdentifierSA(const uchar *pucFileHash, EMFileSize nFileSize, const CAICHHash &rHash, bool bAICHHashValid)
	: m_nFileSize(nFileSize)
{
	SetMD4Hash(pucFileHash);
	if (bAICHHashValid)
		SetAICHHash(rHash);
}

bool CFileIdentifierSA::ReadIdentifier(CFileDataIO *pFile, bool bKadValidWithoutMd4)
{
	uint8 byIdentifierDesc = pFile->ReadUInt8();
	//DebugLog(_T("Read IdentifierDesc: %u"), byIdentifierDesc);
	bool bMD4	 = ((byIdentifierDesc >> 0) & 0x01) > 0;
	bool bSize	 = ((byIdentifierDesc >> 1) & 0x01) > 0;
	bool bAICH	 = ((byIdentifierDesc >> 2) & 0x01) > 0;
	uint8 byMOpt = ((byIdentifierDesc >> 3) & 0x03);
	uint8 byOpts = ((byIdentifierDesc >> 5) & 0x07);
	if (byMOpt > 0) {
		DebugLogError(_T("Unknown mandatory options (%u) set on reading file identifier, aborting"), byMOpt);
		return false;
	}
	if (byOpts > 0)
		DebugLogWarning(_T("Unknown options (%u) set on reading file identifier"), byOpts);
	if (!bMD4 && !bKadValidWithoutMd4) {
		DebugLogError(_T("Mandatory MD4 hash not included on reading file identifier, aborting"));
		return false;
	}
	if (!bSize)
		DebugLogWarning(_T("Size not included on reading file identifier"));

	if (bMD4)
		pFile->ReadHash16(m_abyMD4Hash);
	if (bSize)
		m_nFileSize = pFile->ReadUInt64();
	if (bAICH) {
		m_AICHFileHash.Read(pFile);
		m_bHasValidAICHHash = true;
	}
	return true;
}