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
#pragma once
#include "BarShader.h"
#include "StatisticFile.h"
#include "ShareableFile.h"
#include <list>

class CxImage;
class CUpDownClient;
class Packet;
class CFileDataIO;
class CAICHHashTree;
class CAICHRecoveryHashSet;
class CCollection;
class CAICHHashAlgo;

typedef CTypedPtrList<CPtrList, CUpDownClient*> CUpDownClientPtrList;

class CKnownFile : public CShareableFile
{
	DECLARE_DYNAMIC(CKnownFile)

public:
	CKnownFile();
	virtual	~CKnownFile();

	virtual void SetFileName(LPCTSTR pszFileName, bool bReplaceInvalidFileSystemChars = false, bool bRemoveControlChars = false); // 'bReplaceInvalidFileSystemChars' is set to 'false' for backward compatibility!

	bool	CreateFromFile(LPCTSTR directory, LPCTSTR filename, LPVOID pvProgressParam); // create date, hashset and tags from a file
	bool	LoadFromFile(CFileDataIO *file);	//load date, hashset and tags from a .met file
	bool	WriteToFile(CFileDataIO *file);
	bool	CreateAICHHashSetOnly();

	// last file modification time in (DST corrected, if NTFS) real UTC format
	// NOTE: this value can *not* be compared with NT's version of the UTC time
	CTime	GetUtcCFileDate() const						{ return CTime(m_tUtcLastModified); }
	time_t	GetUtcFileDate() const						{ return m_tUtcLastModified; }

	// Did we not see this file for a long time so that some information should be purged?
	bool	ShouldPartiallyPurgeFile() const;
	void	SetLastSeen()								{ m_timeLastSeen = time(NULL); }

	virtual void	SetFileSize(EMFileSize nFileSize);

	// nr. of 9MB parts (file data)
	inline uint16 GetPartCount() const					{ return m_iPartCount; }

	// nr. of 9MB parts according the file size wrt ED2K protocol (OP_FILESTATUS)
	inline uint16 GetED2KPartCount() const				{ return m_iED2KPartCount; }

	// file upload priority
	uint8	GetUpPriority() const						{ return m_iUpPriority; }
	void	SetUpPriority(uint8 iNewUpPriority, bool bSave = true);
	bool	IsAutoUpPriority() const					{ return m_bAutoUpPriority; }
	void	SetAutoUpPriority(bool NewAutoUpPriority)	{ m_bAutoUpPriority = NewAutoUpPriority; }
	void	UpdateAutoUpPriority();

	// This has lost its meaning here. This is the total clients we know that want this file.
	// Right now this number is used for auto priorities.
	// This may be replaced with total complete source known in the network.
	INT_PTR	GetQueuedCount()							{ return m_ClientUploadList.GetCount(); }

	void	AddUploadingClient(CUpDownClient *client);
	void	RemoveUploadingClient(CUpDownClient *client);
	virtual void	UpdatePartsInfo();
	virtual	void	DrawShareStatusBar(CDC *dc, LPCRECT rect, bool onlygreyrect, bool bFlat) const;

	// comment
	void	SetFileComment(LPCTSTR pszComment);

	void	SetFileRating(UINT uRating);

	bool	GetPublishedED2K() const					{ return m_PublishedED2K; }
	void	SetPublishedED2K(bool val);

	uint32	GetKadFileSearchID() const					{ return kadFileSearchID; }
	void	SetKadFileSearchID(uint32 id)				{ kadFileSearchID = id; } //Don't use this unless you know what your are DOING!! (Hopefully I do. :)

	const Kademlia::WordList &GetKadKeywords() const	{ return wordlist; }

	time_t	GetLastPublishTimeKadSrc() const			{ return m_lastPublishTimeKadSrc; }
	void	SetLastPublishTimeKadSrc(time_t time, uint32 buddyip)	{ m_lastPublishTimeKadSrc = time; m_lastBuddyIP = buddyip; }
	uint32	GetLastPublishBuddy() const					{ return m_lastBuddyIP; }
	void	SetLastPublishTimeKadNotes(time_t time)		{ m_lastPublishTimeKadNotes = time; }
	time_t	GetLastPublishTimeKadNotes() const			{ return m_lastPublishTimeKadNotes; }

	bool	PublishSrc();
	bool	PublishNotes();

	// file sharing
	virtual Packet* CreateSrcInfoPacket(const CUpDownClient *forClient, uint8 byRequestedVersion, uint16 nRequestedOptions) const;
	UINT	GetMetaDataVer() const						{ return m_uMetaDataVer; }
	void	UpdateMetaDataTags();
	void	RemoveMetaDataTags(UINT uTagType = 0);
	void	RemoveBrokenUnicodeMetaDataTags();

	// preview
	bool	IsMovie() const;
	virtual bool GrabImage(uint8 nFramesToGrab, double dStartTime, bool bReduceColor, uint16 nMaxWidth, void *pSender);
	virtual void GrabbingFinished(CxImage **imgResults, uint8 nFramesGrabbed, void *pSender);

	bool	ImportParts();

	// Display / Info / Strings
	virtual CString	GetInfoSummary(bool bNoFormatCommands = false) const;
	CString	GetUpPriorityDisplayString() const;
	virtual void	UpdateFileRatingCommentAvail(bool bForceUpdate = false);

	//aich
	void	SetAICHRecoverHashSetAvailable(bool bVal)	{ m_bAICHRecoverHashSetAvailable = bVal; }
	bool	IsAICHRecoverHashSetAvailable() const		{ return m_bAICHRecoverHashSetAvailable; }

	static bool	CreateHash(const uchar *pucData, uint32 uSize, uchar *pucHash, CAICHHashTree *pShaHashOut = NULL);


	// last file modification time in (DST corrected, if NTFS) real UTC format
	// NOTE: this value can *not* be compared with NT's version of the UTC time
	time_t	m_tUtcLastModified;

	CStatisticFile statistic;
	time_t	m_nCompleteSourcesTime;
	uint16	m_nCompleteSourcesCount;
	uint16	m_nCompleteSourcesCountLo;
	uint16	m_nCompleteSourcesCountHi;
	CUpDownClientPtrList m_ClientUploadList;
	CArray<uint16, uint16> m_AvailPartFrequency;
	CCollection *m_pCollection;

#ifdef _DEBUG
	// Diagnostic Support
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext &dc) const;
#endif

protected:
	//preview
	bool	GrabImage(const CString &strFileName, uint8 nFramesToGrab, double dStartTime, bool bReduceColor, uint16 nMaxWidth, void *pSender);
	bool	LoadTagsFromFile(CFileDataIO *file);
	bool	LoadDateFromFile(CFileDataIO *file);
	static void	CreateHash(CFile *pFile, uint64 Length, uchar *pucHash, CAICHHashTree *pShaHashOut = NULL);
	static bool	CreateHash(FILE *fp, uint64 uSize, uchar *pucHash, CAICHHashTree *pShaHashOut = NULL);

private:
	static CBarShader s_ShareStatusBar;
	Kademlia::WordList wordlist;
	time_t	m_timeLastSeen; // we only "see" files when they are in a shared directory
	time_t	m_lastPublishTimeKadSrc;
	time_t	m_lastPublishTimeKadNotes;
	uint32	kadFileSearchID;
	uint32	m_lastBuddyIP;
	UINT	m_uMetaDataVer;
	uint16	m_iPartCount;
	uint16	m_iED2KPartCount;
	uint8	m_iUpPriority;
	bool	m_bAutoUpPriority;
	bool	m_PublishedED2K;
	bool	m_bAICHRecoverHashSetAvailable;
};