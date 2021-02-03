// parts of this file are based on work from pan One (http://home-3.tiscali.nl/~meost/pms/)
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
#include "AbstractFile.h"
#include "Kademlia/Kademlia/Entry.h"
#include "ini2.h"
#include "Preferences.h"
#include "opcodes.h"
#include "Packets.h"
#include "StringConversion.h"
#ifdef _DEBUG
#include "DebugHelpers.h"
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

IMPLEMENT_DYNAMIC(CAbstractFile, CObject)

CAbstractFile::CAbstractFile()
	: m_nFileSize(0ull)
	, m_FileIdentifier(m_nFileSize)
	, m_uRating()
	, m_bCommentLoaded()
	, m_uUserRating()
	, m_bHasComment()
	, m_bKadCommentSearchRunning()
{
}

CAbstractFile::CAbstractFile(const CAbstractFile *pAbstractFile)
	: m_nFileSize(pAbstractFile->m_nFileSize)
	, m_FileIdentifier(pAbstractFile->m_FileIdentifier, m_nFileSize)
	, m_strFileName(pAbstractFile->m_strFileName)
	, m_strComment(pAbstractFile->m_strComment)
	, m_uRating(pAbstractFile->m_uRating)
	, m_bCommentLoaded(pAbstractFile->m_bCommentLoaded)
	, m_uUserRating(pAbstractFile->m_uUserRating)
	, m_bHasComment(pAbstractFile->m_bHasComment)
	, m_bKadCommentSearchRunning(pAbstractFile->m_bKadCommentSearchRunning)
	, m_strFileType(pAbstractFile->m_strFileType)
{

	const CTypedPtrList<CPtrList, Kademlia::CEntry*> &list = pAbstractFile->getNotes();
	for (POSITION pos = list.GetHeadPosition(); pos != NULL;)
		m_kadNotes.AddTail(list.GetNext(pos)->Copy());

	CopyTags(pAbstractFile->GetTags());
}

CAbstractFile::~CAbstractFile()
{
	ClearTags();
	while (!m_kadNotes.IsEmpty())
		delete m_kadNotes.RemoveHead();
}

#ifdef _DEBUG
void CAbstractFile::AssertValid() const
{
	CObject::AssertValid();
	(void)m_strFileName;
	(void)m_FileIdentifier;
	(void)m_nFileSize;
	(void)m_strComment;
	(void)m_uRating;
	(void)m_strFileType;
	(void)m_uUserRating;
	CHECK_BOOL(m_bHasComment);
	CHECK_BOOL(m_bCommentLoaded);
	m_taglist.AssertValid();
}

void CAbstractFile::Dump(CDumpContext &dc) const
{
	CObject::Dump(dc);
}
#endif

bool CAbstractFile::AddNote(Kademlia::CEntry *pEntry)
{
	for (POSITION pos = m_kadNotes.GetHeadPosition(); pos != NULL; ) {
		const Kademlia::CEntry *entry = m_kadNotes.GetNext(pos);
		if (entry->m_uSourceID == pEntry->m_uSourceID) {
			ASSERT(entry != pEntry);
			return false;
		}
	}
	m_kadNotes.AddHead(pEntry);
	UpdateFileRatingCommentAvail();
	return true;
}

UINT CAbstractFile::GetFileRating()
{
	if (!m_bCommentLoaded)
		LoadComment();
	return m_uRating;
}

const CString& CAbstractFile::GetFileComment()
{
	if (!m_bCommentLoaded)
		LoadComment();
	return m_strComment;
}

void CAbstractFile::LoadComment()
{
	CIni ini(thePrefs.GetFileCommentsFilePath(), md4str(GetFileHash()));
	m_strComment = ini.GetStringUTF8(_T("Comment")).Left(MAXFILECOMMENTLEN);
	m_uRating = ini.GetInt(_T("Rate"), 0);
	m_bCommentLoaded = true;
}

void CAbstractFile::CopyTags(const CArray<CTag*, CTag*> &tags)
{
	for (INT_PTR i = 0; i < tags.GetCount(); ++i)
		m_taglist.Add(new CTag(*tags[i]));
}

void CAbstractFile::ClearTags()
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;)
		delete m_taglist[i];
	m_taglist.RemoveAll();
}

void CAbstractFile::AddTagUnique(CTag *pTag)
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		const CTag *pCurTag = m_taglist[i];
		if ((	(pCurTag->GetNameID() != 0 && pCurTag->GetNameID() == pTag->GetNameID())
			 ||	(pCurTag->GetName()[0] && pTag->GetName()[0] && CmpED2KTagName(pCurTag->GetName(), pTag->GetName()) == 0)
			)
			&& pCurTag->GetType() == pTag->GetType())
		{
			delete pCurTag;
			m_taglist.SetAt(i, pTag);
			return;
		}
	}
	m_taglist.Add(pTag);
}

void CAbstractFile::SetFileName(LPCTSTR pszFileName, bool bReplaceInvalidFileSystemChars, bool bAutoSetFileType, bool bRemoveControlChars)
{
	m_strFileName = pszFileName;
	if (bReplaceInvalidFileSystemChars)
		for (LPCTSTR p = sBadFileNameChar; *p; ++p)
			m_strFileName.Replace(*p, _T('-'));
	if (bAutoSetFileType)
		SetFileType(GetFileTypeByName(m_strFileName));
	if (bRemoveControlChars)
		for (int i = m_strFileName.GetLength(); --i >= 0;)
			if (m_strFileName[i] < _T(' ')) //space
				m_strFileName.Delete(i, 1);
}

void CAbstractFile::SetFileType(LPCTSTR pszFileType)
{
	m_strFileType = pszFileType;
}

CString CAbstractFile::GetFileTypeDisplayStr() const
{
	CString strFileTypeDisplayStr(GetFileTypeDisplayStrFromED2KFileType(GetFileType()));
	if (strFileTypeDisplayStr.IsEmpty())
		strFileTypeDisplayStr = GetFileType();
	return strFileTypeDisplayStr;
}

bool CAbstractFile::HasNullHash() const
{
	return isnulmd4(m_FileIdentifier.GetMD4Hash());
}

uint32 CAbstractFile::GetIntTagValue(uint8 tagname) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		const CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname && pTag->IsInt())
			return pTag->GetInt();
	}
	return 0;
}

bool CAbstractFile::GetIntTagValue(uint8 tagname, uint32 &ruValue) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		const CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname && pTag->IsInt()) {
			ruValue = pTag->GetInt();
			return true;
		}
	}
	return false;
}

uint64 CAbstractFile::GetInt64TagValue(LPCSTR tagname) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		const CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == 0 && pTag->IsInt64(true) && CmpED2KTagName(pTag->GetName(), tagname) == 0)
			return pTag->GetInt64();
	}
	return 0;
}

uint64 CAbstractFile::GetInt64TagValue(uint8 tagname) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		const CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname && pTag->IsInt64(true))
			return pTag->GetInt64();
	}
	return 0;
}

bool CAbstractFile::GetInt64TagValue(uint8 tagname, uint64 &ruValue) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		const CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname && pTag->IsInt64(true)) {
			ruValue = pTag->GetInt64();
			return true;
		}
	}
	return false;
}

uint32 CAbstractFile::GetIntTagValue(LPCSTR tagname) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		const CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == 0 && pTag->IsInt() && CmpED2KTagName(pTag->GetName(), tagname) == 0)
			return pTag->GetInt();
	}
	return 0;
}

void CAbstractFile::SetIntTagValue(uint8 tagname, uint32 uValue)
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname && pTag->IsInt()) {
			pTag->SetInt(uValue);
			return;
		}
	}
	m_taglist.Add(new CTag(tagname, uValue));
}

void CAbstractFile::SetInt64TagValue(uint8 tagname, uint64 uValue)
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname && pTag->IsInt64(true)) {
			pTag->SetInt64(uValue);
			return;
		}
	}
	m_taglist.Add(new CTag(tagname, uValue));
}

static const CString s_strEmpty;

const CString& CAbstractFile::GetStrTagValue(uint8 tagname) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		const CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname && pTag->IsStr())
			return pTag->GetStr();
	}
	return s_strEmpty;
}

const CString& CAbstractFile::GetStrTagValue(LPCSTR tagname) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		const CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == 0 && pTag->IsStr() && CmpED2KTagName(pTag->GetName(), tagname) == 0)
			return pTag->GetStr();
	}
	return s_strEmpty;
}

void CAbstractFile::SetStrTagValue(uint8 tagname, LPCTSTR pszValue)
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname && pTag->IsStr()) {
			pTag->SetStr(pszValue);
			return;
		}
	}
	m_taglist.Add(new CTag(tagname, pszValue));
}

CTag* CAbstractFile::GetTag(uint8 tagname, uint8 tagtype) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname && pTag->GetType() == tagtype)
			return pTag;
	}
	return NULL;
}

CTag* CAbstractFile::GetTag(LPCSTR tagname, uint8 tagtype) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == 0 && pTag->GetType() == tagtype && CmpED2KTagName(pTag->GetName(), tagname) == 0)
			return pTag;
	}
	return NULL;
}

CTag* CAbstractFile::GetTag(uint8 tagname) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname)
			return pTag;
	}
	return NULL;
}

CTag* CAbstractFile::GetTag(LPCSTR tagname) const
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == 0 && CmpED2KTagName(pTag->GetName(), tagname) == 0)
			return pTag;
	}
	return NULL;
}

void CAbstractFile::DeleteTag(CTag *pTag)
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;)
		if (m_taglist[i] == pTag) {
			m_taglist.RemoveAt(i);
			delete pTag;
			return;
		}
}

void CAbstractFile::DeleteTag(uint8 tagname)
{
	for (INT_PTR i = m_taglist.GetCount(); --i >= 0;) {
		CTag *pTag = m_taglist[i];
		if (pTag->GetNameID() == tagname) {
			m_taglist.RemoveAt(i);
			delete pTag;
			return;
		}
	}
}

void CAbstractFile::SetKadCommentSearchRunning(bool bVal)
{
	if (bVal != m_bKadCommentSearchRunning) {
		m_bKadCommentSearchRunning = bVal;
		UpdateFileRatingCommentAvail(true);
	}
}

void CAbstractFile::RefilterKadNotes(bool bUpdate)
{
	const CString &cfilter(thePrefs.GetCommentFilter());
	// check all available comments against our filter again
	if (cfilter.IsEmpty())
		return;

	for (POSITION pos = m_kadNotes.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		const Kademlia::CEntry *entry = m_kadNotes.GetNext(pos);
		if (!entry->GetStrTagValue(Kademlia::CKadTagNameString(TAG_DESCRIPTION)).IsEmpty()) {
			CString strCommentLower(entry->GetStrTagValue(Kademlia::CKadTagNameString(TAG_DESCRIPTION)));
			// Verified Locale Dependency: Locale dependent string conversion (OK)
			strCommentLower.MakeLower();

			for (int iPos = 0; iPos >= 0;) {
				const CString &strFilter(cfilter.Tokenize(_T("|"), iPos));
				// comment filters are already in lower case, compare with temp. lower cased received comment
				if (!strFilter.IsEmpty() && strCommentLower.Find(strFilter) >= 0) {
					m_kadNotes.RemoveAt(pos2);
					delete entry;
					break;
				}
			}
		}
	}
	if (bUpdate) // until updated, rating and m_bHasComment might be wrong
		UpdateFileRatingCommentAvail();
}

CString CAbstractFile::GetED2kLink(bool bHashset, bool bHTML, bool bHostname, bool bSource, uint32 dwSourceIP) const
{
	CString strLink;
	strLink.Format(_T("ed2k://|file|%s|%I64u|%s|")
		, (LPCTSTR)EncodeUrlUtf8(StripInvalidFilenameChars(GetFileName()))
		, (uint64)GetFileSize()
		, (LPCTSTR)EncodeBase16(GetFileHash(), 16));

	if (bHTML)
		strLink = _T("<a href=\"") + strLink;
	if (bHashset && GetFileIdentifierC().GetAvailableMD4PartHashCount() > 0 && GetFileIdentifierC().HasExpectedMD4HashCount()) {
		strLink += _T("p=");
		for (UINT j = 0; j < GetFileIdentifierC().GetAvailableMD4PartHashCount(); ++j) {
			if (j > 0)
				strLink += _T(':');
			strLink += EncodeBase16(GetFileIdentifierC().GetMD4PartHash(j), 16);
		}
		strLink += _T('|');
	}

	if (GetFileIdentifierC().HasAICHHash())
		strLink.AppendFormat(_T("h=%s|"), (LPCTSTR)GetFileIdentifierC().GetAICHHash().GetString());

	strLink += _T('/');
	if (bHostname && thePrefs.GetYourHostname().Find(_T('.')) >= 0)
		strLink.AppendFormat(_T("|sources,%s:%i|/"), (LPCTSTR)thePrefs.GetYourHostname(), thePrefs.GetPort());
	else if (bSource && dwSourceIP != 0)
		strLink.AppendFormat(_T("|sources,%i.%i.%i.%i:%i|/"), (uint8)dwSourceIP, (uint8)(dwSourceIP >> 8), (uint8)(dwSourceIP >> 16), (uint8)(dwSourceIP >> 24), thePrefs.GetPort());

	if (bHTML)
		strLink.AppendFormat(_T("\">%s</a>"), (LPCTSTR)StripInvalidFilenameChars(GetFileName()));

	return strLink;
}