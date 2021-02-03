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

struct Requested_Block_Struct;
class CUpDownClient;
typedef CTypedPtrList<CPtrList, CUpDownClient*> CUpDownClientPtrList;

struct UploadingToClient_Struct
{
	UploadingToClient_Struct()
		: m_pClient()
		, m_bIOError()
		, m_bDisableCompression()
	{
	}
	~UploadingToClient_Struct();

	CUpDownClient										*m_pClient;
	CTypedPtrList<CPtrList, Requested_Block_Struct*>	m_BlockRequests_queue;
	CTypedPtrList<CPtrList, Requested_Block_Struct*>	m_DoneBlocks_list;
	CCriticalSection									m_csBlockListsLock; // don't acquire other locks while having this one in any thread other than UploadDiskIOThread or make sure deadlocks are impossible
	bool												m_bIOError;
	bool												m_bDisableCompression;
};
typedef CTypedPtrList<CPtrList, UploadingToClient_Struct*> CUploadingPtrList;

class CUploadQueue
{

public:
	CUploadQueue();
	~CUploadQueue();

	void	Process();
	void	AddClientToQueue(CUpDownClient *client, bool bIgnoreTimelimit = false);
	bool	RemoveFromUploadQueue(CUpDownClient *client, LPCTSTR pszReason = NULL, bool updatewindow = true, bool earlyabort = false);
	bool	RemoveFromWaitingQueue(CUpDownClient *client, bool updatewindow = true);
	bool	IsOnUploadQueue(CUpDownClient *client)	const	{ return (waitinglist.Find(client) != 0); }
	bool	IsDownloading(const CUpDownClient *client)	const { return (GetUploadingClientStructByClient(client) != NULL); }

	void	UpdateDatarates();
	uint32	GetDatarate() const								{ return datarate; }
	uint32  GetToNetworkDatarate() const;

	bool	CheckForTimeOver(const CUpDownClient *client);
	INT_PTR	GetWaitingUserCount() const						{ return waitinglist.GetCount(); }
	INT_PTR	GetUploadQueueLength() const					{ return uploadinglist.GetCount(); }
	INT_PTR	GetActiveUploadsCount()	const					{ return m_MaxActiveClientsShortTime; }
	uint32	GetWaitingUserForFileCount(const CSimpleArray<CObject*> &raFiles, bool bOnlyIfChanged);
	uint32	GetDatarateForFile(const CSimpleArray<CObject*> &raFiles) const;
	uint32	GetTargetClientDataRate(bool bMinDatarate) const;

	POSITION GetFirstFromUploadList() const					{ return uploadinglist.GetHeadPosition(); }
	CUpDownClient* GetNextFromUploadList(POSITION &curpos) const { return static_cast<UploadingToClient_Struct*>(uploadinglist.GetNext(curpos))->m_pClient; }
	CUpDownClient* GetQueueClientAt(POSITION &curpos) const	{ return static_cast<UploadingToClient_Struct*>(uploadinglist.GetAt(curpos))->m_pClient; }

	POSITION GetFirstFromWaitingList() const				{ return waitinglist.GetHeadPosition(); }
	CUpDownClient* GetNextFromWaitingList(POSITION &curpos) const { return waitinglist.GetNext(curpos); }
	CUpDownClient* GetWaitClientAt(POSITION &curpos) const	{ return waitinglist.GetAt(curpos); }

	CUpDownClient* GetWaitingClientByIP_UDP(uint32 dwIP, uint16 nUDPPort, bool bIgnorePortOnUniqueIP, bool *pbMultipleIPs = NULL);
	CUpDownClient* GetWaitingClientByIP(uint32 dwIP) const;
	CUpDownClient* GetNextClient(const CUpDownClient *lastclient) const;

	UploadingToClient_Struct* GetUploadingClientStructByClient(const CUpDownClient *pClient) const;

	const CUploadingPtrList& GetUploadListTS(CCriticalSection **outUploadListReadLock);


	void	DeleteAll();
	UINT	GetWaitingPosition(CUpDownClient *client);

	uint32	GetSuccessfullUpCount() const					{ return successfullupcount; }
	uint32	GetFailedUpCount() const						{ return failedupcount; }
	uint32	GetAverageUpTime() const;

	CUpDownClient* FindBestClientInQueue();
	void ReSortUploadSlots(bool force = false);

	CUpDownClientPtrList waitinglist;

protected:
	void		RemoveFromWaitingQueue(POSITION pos, bool updatewindow);
	bool		AcceptNewClient(bool addOnNextConnect = false) const;
	bool		AcceptNewClient(INT_PTR curUploadSlots) const;
	bool		ForceNewClient(bool allowEmptyWaitingQueue = false);
	bool		AddUpNextClient(LPCTSTR pszReason, CUpDownClient *directadd = NULL);

	static VOID CALLBACK UploadTimer(HWND hWnd, UINT nMsg, UINT_PTR nId, DWORD dwTime) noexcept;

private:
	void	UpdateMaxClientScore();
	uint32	GetMaxClientScore() const						{ return m_imaxscore; }
	void	UpdateActiveClientsInfo(DWORD curTick);

	void InsertInUploadingList(CUpDownClient *newclient, bool bNoLocking = false);
	void InsertInUploadingList(UploadingToClient_Struct *pNewClientUploadStruct, bool bNoLocking = false);
	float GetAverageCombinedFilePrioAndCredit();


	// By BadWolf - Accurate Speed Measurement
	typedef struct
	{
		uint32	datalen;
		DWORD	timestamp;
	} TransferredData;

	CUploadingPtrList	uploadinglist;
	// this lock ensures that only the main thread writes the uploading list, other threads need to fetch the lock if they want to read (but are not allowed to write)
	CCriticalSection	m_csUploadListMainThrdWriteOtherThrdsRead; // don't acquire other locks while having this one in any thread other than UploadDiskIOThread or make sure deadlocks are impossible

	CList<uint64> average_dr_list;
	CList<uint64> average_friend_dr_list;
	CList<DWORD, DWORD> average_tick_list;
	CList<int, int> activeClients_list;
	CList<DWORD, DWORD> activeClients_tick_list;
	uint32	datarate;   //data rate sent to network (including friends)
	uint32  friendDatarate; // data rate of sent to friends (included in above total)
	// By BadWolf - Accurate Speed Measurement

	UINT_PTR h_timer;
	uint32	successfullupcount;
	uint32	failedupcount;
	uint32	totaluploadtime;
	uint32	m_nLastStartUpload;
	uint32	m_dwRemovedClientByScore;

	uint32	m_imaxscore;

	DWORD	m_dwLastCalculatedAverageCombinedFilePrioAndCredit;
	float	m_fAverageCombinedFilePrioAndCredit;
	INT_PTR	m_iHighestNumberOfFullyActivatedSlotsSinceLastCall;
	INT_PTR	m_MaxActiveClients;
	INT_PTR	m_MaxActiveClientsShortTime;

	DWORD	m_lastCalculatedDataRateTick;
	uint64	m_average_dr_sum;

	DWORD	m_dwLastResortedUploadSlots;
	bool	m_bStatisticsWaitingListDirty;
};