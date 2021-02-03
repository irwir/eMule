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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once
#include "ShaHashset.h"

class CFileDataIO;

///////////////////////////////////////////////////////////////////////////////////////////////
// CFileIdentifierBase
class CFileIdentifierBase
{
public:
	virtual	~CFileIdentifierBase() = default;

	virtual EMFileSize GetFileSize() const;

	void			WriteIdentifier(CFileDataIO *pFile, bool bKadExcludeMD4 = false) const;
	bool			CompareRelaxed(const CFileIdentifierBase &rFileIdentifier) const;
	bool			CompareStrict(const CFileIdentifierBase &rFileIdentifier) const;

	CString			DbgInfo() const;

	//******************** MD4 Related
	void			SetMD4Hash(const uchar *pucFileHash);
	void			SetMD4Hash(CFileDataIO *pFile);
	const uchar*	GetMD4Hash() const						{ return m_abyMD4Hash; }

	//******************** AICH Related
	const CAICHHash& GetAICHHash() const					{ ASSERT(m_bHasValidAICHHash); return m_AICHFileHash; }
	void			SetAICHHash(const CAICHHash &Hash);
	bool			HasAICHHash() const						{ return m_bHasValidAICHHash; }
	void			ClearAICHHash()							{ m_bHasValidAICHHash = false; }

protected:
	CFileIdentifierBase();
	CFileIdentifierBase(const CFileIdentifierBase &rFileIdentifier);
	CFileIdentifierBase& operator=(const CFileIdentifierBase &rFileIdentifier);

	uchar			m_abyMD4Hash[16];
	CAICHHash		m_AICHFileHash;
	bool			m_bHasValidAICHHash;
};

///////////////////////////////////////////////////////////////////////////////////////////////
// CFileIdentifier
// Member of all CAbstractFiles, including hashsets
class CFileIdentifier : public CFileIdentifierBase
{
public:
	explicit CFileIdentifier(EMFileSize &rFileSize);
	CFileIdentifier(const CFileIdentifier &rFileIdentifier, EMFileSize &rFileSize);
	virtual	~CFileIdentifier();

	//******************** Common
	void	WriteHashSetsToPacket(CFileDataIO *pFile, bool bMD4, bool bAICH) const; // not compatible with old single md4 hashset
	bool	ReadHashSetsFromPacket(CFileDataIO *pFile, bool &rbMD4, bool &rbAICH);  // not compatible with old single md4 hashset
	virtual EMFileSize GetFileSize() const					{ return m_rFileSize; }

	//******************** MD4 Related
	bool	CalculateMD4HashByHashSet(bool bVerifyOnly, bool bDeleteOnVerifyFail = true);
	bool	LoadMD4HashsetFromFile(CFileDataIO *file, bool bVerifyExistingHash);
	void	WriteMD4HashsetToFile(CFileDataIO *pFile) const;

	bool	SetMD4HashSet(const CArray<uchar*, uchar*> &aHashset);
	uchar*	GetMD4PartHash(UINT part) const;
	void	DeleteMD4Hashset();

	// nr. of part hashes according the file size wrt ED2K protocol
	uint16	GetTheoreticalMD4PartHashCount() const;														 /* prev: GetED2KPartHashCount()*/
	uint16	GetAvailableMD4PartHashCount() const			{ return (uint16)m_aMD4HashSet.GetCount(); } /* prev: GetHashCount() */
	bool	HasExpectedMD4HashCount() const					{ return GetTheoreticalMD4PartHashCount() == GetAvailableMD4PartHashCount(); }

	CArray<uchar*, uchar*>&	GetRawMD4HashSet()				{ return m_aMD4HashSet; }

	//******************** AICH Related
	bool		LoadAICHHashsetFromFile(CFileDataIO *pFile, bool bVerify = true); // bVerify=false means you must call VerifyAICHHashSet immediately after this method
	void		WriteAICHHashsetToFile(CFileDataIO *pFile) const;

	bool		SetAICHHashSet(const CAICHRecoveryHashSet &sourceHashSet);
	bool		SetAICHHashSet(const CFileIdentifier &rSourceHashSet);

	bool		VerifyAICHHashSet();
	uint16		GetTheoreticalAICHPartHashCount() const;
	uint16		GetAvailableAICHPartHashCount() const		{ return (uint16)m_aAICHPartHashSet.GetCount(); }
	bool		HasExpectedAICHHashCount() const			{ return GetTheoreticalAICHPartHashCount() == GetAvailableAICHPartHashCount(); }

	const CArray<CAICHHash>&	GetRawAICHHashSet()	const	{ return m_aAICHPartHashSet; }
private:
	EMFileSize				&m_rFileSize;
	CArray<uchar*, uchar*>	m_aMD4HashSet;
	CArray<CAICHHash>		m_aAICHPartHashSet;
};

///////////////////////////////////////////////////////////////////////////////////////////////
// CFileIdentifierSA
// Stand alone file identifier, for creating out of protocol packages, comparing and stand-alone storing, excluding hashsets
class CFileIdentifierSA : public CFileIdentifierBase
{
public:
	CFileIdentifierSA(const uchar *pucFileHash, EMFileSize nFileSize, const CAICHHash &rHash, bool bAICHHashValid);
	CFileIdentifierSA();

	virtual	~CFileIdentifierSA() = default;
	virtual EMFileSize GetFileSize() const					{ return m_nFileSize; }

	bool	ReadIdentifier(CFileDataIO *pFile, bool bKadValidWithoutMd4 = false);

private:
	EMFileSize	m_nFileSize;
};

// Compare Helper for lists
inline int CompareAICHHash(const CFileIdentifier &ident1, const CFileIdentifier &ident2, bool bSortAscending)
{
	if (ident1.HasAICHHash()) {
		if (ident2.HasAICHHash())
			return memcmp(ident1.GetAICHHash().GetRawHashC(), ident2.GetAICHHash().GetRawHashC(), CAICHHash::GetHashSize());
		return bSortAscending ? 1 : -1;
	}
	if (ident2.HasAICHHash())
		return bSortAscending ? -1 : 1;
	return 0;
}