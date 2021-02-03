//this file is part of eMule
//Copyright (C)2002-2005 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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
#include "collection.h"
#include "KnownFile.h"
#include "CollectionFile.h"
#include "SafeFile.h"
#include "Packets.h"
#include "Preferences.h"
#include "SharedFilelist.h"
#include "emule.h"
#include "Log.h"
#include "md5sum.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define COLLECTION_FILE_VERSION1_INITIAL		0x01
#define COLLECTION_FILE_VERSION2_LARGEFILES		0x02

CCollection::CCollection()
	: m_bTextFormat()
	, m_pabyCollectionAuthorKey()
	, m_nKeySize()
{
	m_CollectionFilesMap.InitHashTable(1031);
	m_sCollectionName.Format(_T("New Collection-%u"), ::GetTickCount());
}

CCollection::CCollection(const CCollection *pCollection)
	: m_sCollectionName(pCollection->m_sCollectionName)
	, m_bTextFormat(pCollection->m_bTextFormat)
{
	if (pCollection->m_pabyCollectionAuthorKey != NULL) {
		m_nKeySize = pCollection->m_nKeySize;
		m_pabyCollectionAuthorKey = new BYTE[m_nKeySize];
		memcpy(m_pabyCollectionAuthorKey, pCollection->m_pabyCollectionAuthorKey, m_nKeySize);
		m_sCollectionAuthorName = pCollection->m_sCollectionAuthorName;
	} else {
		m_nKeySize = 0;
		m_pabyCollectionAuthorKey = NULL;
	}

	m_CollectionFilesMap.InitHashTable(1031);
	CSKey key;
	for (POSITION pos = pCollection->m_CollectionFilesMap.GetStartPosition(); pos != NULL;) {
		CCollectionFile *pCollectionFile;
		pCollection->m_CollectionFilesMap.GetNextAssoc(pos, key, pCollectionFile);
		AddFileToCollection(pCollectionFile, true);
	}
}

CCollection::~CCollection()
{
	delete[] m_pabyCollectionAuthorKey;
	CSKey key;
	for (POSITION pos = m_CollectionFilesMap.GetStartPosition(); pos != NULL;) {
		CCollectionFile *pCollectionFile;
		m_CollectionFilesMap.GetNextAssoc(pos, key, pCollectionFile);
		delete pCollectionFile;
	}
	m_CollectionFilesMap.RemoveAll();
}

CCollectionFile* CCollection::AddFileToCollection(CAbstractFile *pAbstractFile, bool bCreateClone)
{
	CSKey key(pAbstractFile->GetFileHash());
	CCollectionFile *pCollectionFile;
	if (m_CollectionFilesMap.Lookup(key, pCollectionFile)) {
		ASSERT(0);
		return pCollectionFile;
	}

	pCollectionFile = NULL;

	if (bCreateClone)
		pCollectionFile = new CCollectionFile(pAbstractFile);
	else if (pAbstractFile->IsKindOf(RUNTIME_CLASS(CCollectionFile)))
		pCollectionFile = static_cast<CCollectionFile*>(pAbstractFile);

	if (pCollectionFile)
		m_CollectionFilesMap.SetAt(key, pCollectionFile);

	return pCollectionFile;
}

void CCollection::RemoveFileFromCollection(CAbstractFile *pAbstractFile)
{
	CSKey key(pAbstractFile->GetFileHash());
	CCollectionFile *pCollectionFile;
	if (m_CollectionFilesMap.Lookup(key, pCollectionFile)) {
		m_CollectionFilesMap.RemoveKey(key);
		delete pCollectionFile;
	} else
		ASSERT(0);
}

void CCollection::SetCollectionAuthorKey(const byte *abyCollectionAuthorKey, uint32 nSize)
{
	delete[] m_pabyCollectionAuthorKey;
	m_pabyCollectionAuthorKey = NULL;
	m_nKeySize = 0;
	if (abyCollectionAuthorKey != NULL) {
		m_pabyCollectionAuthorKey = new BYTE[nSize];
		memcpy(m_pabyCollectionAuthorKey, abyCollectionAuthorKey, nSize);
		m_nKeySize = nSize;
	}
}

bool CCollection::InitCollectionFromFile(const CString &sFilePath, const CString &sFileName)
{
	bool bCollectionLoaded = false;

	CSafeFile data;
	if (data.Open(sFilePath, CFile::modeRead | CFile::shareDenyWrite | CFile::typeBinary)) {
		try {
			uint32 nVersion = data.ReadUInt32();
			if (nVersion == COLLECTION_FILE_VERSION1_INITIAL || nVersion == COLLECTION_FILE_VERSION2_LARGEFILES) {
				for (uint32 headerTagCount = data.ReadUInt32(); headerTagCount > 0; --headerTagCount) {
					CTag tag(&data, true);
					switch (tag.GetNameID()) {
					case FT_FILENAME:
						if (tag.IsStr())
							m_sCollectionName = tag.GetStr();
						break;
					case FT_COLLECTIONAUTHOR:
						if (tag.IsStr())
							m_sCollectionAuthorName = tag.GetStr();
						break;
					case FT_COLLECTIONAUTHORKEY:
						if (tag.IsBlob())
							SetCollectionAuthorKey(tag.GetBlob(), tag.GetBlobSize());
					}
				}
				for (uint32 fileCount = data.ReadUInt32(); fileCount > 0; --fileCount)
					try {
						CCollectionFile *pCollectionFile = new CCollectionFile(&data);
						AddFileToCollection(pCollectionFile, false);
					} catch (...) {
					}

				bCollectionLoaded = true;
			}
			if (m_pabyCollectionAuthorKey != NULL) {
				bool bResult = false;
				if (data.GetLength() > data.GetPosition()) {
					using namespace CryptoPP;

					uint32 nPos = (uint32)data.GetPosition();
					data.SeekToBegin();
					BYTE *pMessage = new BYTE[nPos];
					VERIFY(data.Read(pMessage, nPos) == nPos);

					StringSource ss_Pubkey(m_pabyCollectionAuthorKey, m_nKeySize, true, 0);
					RSASSA_PKCS1v15_SHA_Verifier pubkey(ss_Pubkey);

					UINT nSignLen = (UINT)(data.GetLength() - data.GetPosition());
					BYTE *pSignature = new BYTE[nSignLen];
					VERIFY(data.Read(pSignature, nSignLen) == nSignLen);

					bResult = pubkey.VerifyMessage(pMessage, nPos, pSignature, nSignLen);

					delete[] pMessage;
					delete[] pSignature;
				}
				if (!bResult) {
					DebugLogWarning(_T("Collection %s: Verification of public key failed!"), (LPCTSTR)m_sCollectionName);
					delete[] m_pabyCollectionAuthorKey;
					m_pabyCollectionAuthorKey = NULL;
					m_nKeySize = 0;
					m_sCollectionAuthorName.Empty();
				} else
					DebugLog(_T("Collection %s: Public key verified"), (LPCTSTR)m_sCollectionName);

			} else
				m_sCollectionAuthorName.Empty();
			data.Close();
		} catch (CFileException *error) {
			error->Delete();
			return false;
		} catch (...) {
			ASSERT(0);
			data.Close();
			return false;
		}
	} else
		return false;

	if (!bCollectionLoaded) {
		CStdioFile data1;
		if (data1.Open(sFilePath, CFile::modeRead | CFile::shareDenyWrite | CFile::typeText)) {
			try {
				CString sLink;
				while (data1.ReadString(sLink)) {
					//Ignore all lines that start with #.
					//These lines can be used for future features.
					if (sLink.Find(_T('#')) != 0) {
						try {
							CCollectionFile *pCollectionFile = new CCollectionFile();
							if (pCollectionFile->InitFromLink(sLink))
								AddFileToCollection(pCollectionFile, false);
							else
								delete pCollectionFile;
						} catch (...) {
							ASSERT(0);
							data1.Close();
							return false;
						}
					}
				}
				data1.Close();
				m_sCollectionName = sFileName;
				DEBUG_ONLY(m_sCollectionName.Replace(COLLECTION_FILEEXTENSION, _T("")));
				bCollectionLoaded = true;
				m_bTextFormat = true;
			} catch (CFileException *error) {
				error->Delete();
				return false;
			} catch (...) {
				ASSERT(0);
				data1.Close();
				return false;
			}
		}
	}

	return bCollectionLoaded;
}

void CCollection::WriteToFileAddShared(CryptoPP::RSASSA_PKCS1v15_SHA_Signer *pSignKey)
{
	using namespace CryptoPP;

	CString sFilePath;
	sFilePath.Format(_T("%s\\%s%s"), (LPCTSTR)thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR), (LPCTSTR)m_sCollectionName, COLLECTION_FILEEXTENSION);

	if (m_bTextFormat) {
		CStdioFile data;
		if (data.Open(sFilePath, CFile::modeCreate | CFile::modeWrite | CFile::shareDenyWrite | CFile::typeText)) {
			try {
				CSKey key;
				for (POSITION pos = m_CollectionFilesMap.GetStartPosition(); pos != NULL;) {
					CCollectionFile *pCollectionFile;
					m_CollectionFilesMap.GetNextAssoc(pos, key, pCollectionFile);
					if (pCollectionFile)
						data.WriteString(pCollectionFile->GetED2kLink() + _T('\n'));
				}
				data.Close();
			} catch (CFileException *error) {
				error->Delete();
				return;
			} catch (...) {
				ASSERT(0);
				data.Close();
				return;
			}
		}
	} else {
		CSafeFile data;
		if (data.Open(sFilePath, CFile::modeCreate | CFile::modeReadWrite | CFile::shareDenyWrite | CFile::typeBinary)) {
			try {
				//Version
				// check first if we have any large files in the map - write use lowest version possible
				uint32 dwVersion = COLLECTION_FILE_VERSION1_INITIAL;
				for (POSITION pos = m_CollectionFilesMap.GetStartPosition(); pos != NULL;) {
					CCollectionFile *pCollectionFile;
					CSKey key;
					m_CollectionFilesMap.GetNextAssoc(pos, key, pCollectionFile);
					if (pCollectionFile->IsLargeFile()) {
						dwVersion = COLLECTION_FILE_VERSION2_LARGEFILES;
						break;
					}
				}
				data.WriteUInt32(dwVersion);

				uint32 uTagCount = 1;

				//NumberHeaderTags
				if (m_pabyCollectionAuthorKey != NULL)
					uTagCount += 2;


				data.WriteUInt32(uTagCount);

				CTag collectionName(FT_FILENAME, m_sCollectionName);
				collectionName.WriteTagToFile(&data, UTF8strRaw);

				if (m_pabyCollectionAuthorKey != NULL) {
					CTag collectionAuthor(FT_COLLECTIONAUTHOR, m_sCollectionAuthorName);
					collectionAuthor.WriteTagToFile(&data, UTF8strRaw);

					CTag collectionAuthorKey(FT_COLLECTIONAUTHORKEY, m_nKeySize, m_pabyCollectionAuthorKey);
					collectionAuthorKey.WriteTagToFile(&data, UTF8strRaw);
				}

				//Total Files
				data.WriteUInt32((uint32)m_CollectionFilesMap.GetCount());

				CSKey key;
				for (POSITION pos = m_CollectionFilesMap.GetStartPosition(); pos != NULL;) {
					CCollectionFile *pCollectionFile;
					m_CollectionFilesMap.GetNextAssoc(pos, key, pCollectionFile);
					pCollectionFile->WriteCollectionInfo(&data);
				}

				if (pSignKey != NULL) {
					uint32 nPos = (uint32)data.GetPosition();
					data.SeekToBegin();
					BYTE *pBuffer = new BYTE[nPos];
					VERIFY(data.Read(pBuffer, nPos) == nPos);

					SecByteBlock sbbSignature(pSignKey->SignatureLength());
					AutoSeededRandomPool rng;
					pSignKey->SignMessage(rng, pBuffer, nPos, sbbSignature.begin());
					BYTE abyBuffer2[500];
					ArraySink asink(abyBuffer2, sizeof abyBuffer2);
					asink.Put(sbbSignature.begin(), sbbSignature.size());
					data.Write(abyBuffer2, (UINT)asink.TotalPutLength());

					delete[] pBuffer;
				}
				data.Close();
			} catch (CFileException *error) {
				error->Delete();
				return;
			} catch (...) {
				ASSERT(0);
				data.Close();
				return;
			}
		}
	}

	theApp.sharedfiles->AddFileFromNewlyCreatedCollection(sFilePath);
}

bool CCollection::HasCollectionExtention(const CString &sFileName)
{
	return sFileName.Find(COLLECTION_FILEEXTENSION) >= 0;
}

CString CCollection::GetCollectionAuthorKeyString()
{
	if (m_pabyCollectionAuthorKey == NULL)
		return CString();
	return EncodeBase16(m_pabyCollectionAuthorKey, m_nKeySize);
}

CString CCollection::GetAuthorKeyHashString() const
{
	if (m_pabyCollectionAuthorKey == NULL)
		return CString();
	MD5Sum md5(m_pabyCollectionAuthorKey, m_nKeySize);
	return md5.GetHashString().MakeUpper();
}