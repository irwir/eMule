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
#include <list>

// CStatistics
#define AVG_SESSION 0
#define AVG_TIME 1
#define AVG_TOTAL 2

enum TBPSTATES
{
	STATE_DOWNLOADING = 0x01,
	STATE_ERROROUS	  = 0x10
};

class CStatistics
{
public:
	CStatistics();   // standard constructor

	static void	Init();
	void	RecordRate();
	float	GetAvgDownloadRate(int averageType);
	float	GetAvgUploadRate(int averageType);

	// -khaos--+++> (2-11-03)
	static uint32	GetTransferTime()							{ return timeTransfers + time_thisTransfer; }
	static uint32	GetUploadTime()								{ return timeUploads + time_thisUpload; }
	static uint32	GetDownloadTime()							{ return timeDownloads + time_thisDownload; }
	static uint32	GetServerDuration()							{ return timeServerDuration + time_thisServerDuration; }
	static void	Add2TotalServerDuration()						{ timeServerDuration += time_thisServerDuration;
																  time_thisServerDuration = 0; }
	void	UpdateConnectionStats(float uploadrate, float downloadrate);


	///////////////////////////////////////////////////////////////////////////
	// Down Overhead
	//
	void	CompDownDatarateOverhead();
	void	ResetDownDatarateOverhead();
	static void	AddDownDataOverheadSourceExchange(uint32 data)	{ m_nDownDataRateMSOverhead += data;
																  m_nDownDataOverheadSourceExchange += data;
																  ++m_nDownDataOverheadSourceExchangePackets; }
	static void	AddDownDataOverheadFileRequest(uint32 data)		{ m_nDownDataRateMSOverhead += data;
																  m_nDownDataOverheadFileRequest += data;
																  ++m_nDownDataOverheadFileRequestPackets; }
	static void	AddDownDataOverheadServer(uint32 data)			{ m_nDownDataRateMSOverhead += data;
																  m_nDownDataOverheadServer += data;
																  ++m_nDownDataOverheadServerPackets; }
	static void	AddDownDataOverheadOther(uint32 data)			{ m_nDownDataRateMSOverhead += data;
																  m_nDownDataOverheadOther += data;
																  ++m_nDownDataOverheadOtherPackets; }
	static void	AddDownDataOverheadKad(uint32 data)				{ m_nDownDataRateMSOverhead += data;
																  m_nDownDataOverheadKad += data;
																  ++m_nDownDataOverheadKadPackets; }
	static void	AddDownDataOverheadCrypt(uint32 /*data*/)		{}
	static uint32	GetDownDatarateOverhead()					{ return m_nDownDatarateOverhead; }
	static uint64	GetDownDataOverheadSourceExchange()			{ return m_nDownDataOverheadSourceExchange; }
	static uint64	GetDownDataOverheadFileRequest()			{ return m_nDownDataOverheadFileRequest; }
	static uint64	GetDownDataOverheadServer()					{ return m_nDownDataOverheadServer; }
	static uint64	GetDownDataOverheadKad()					{ return m_nDownDataOverheadKad; }
	static uint64	GetDownDataOverheadOther()					{ return m_nDownDataOverheadOther; }
	static uint64	GetDownDataOverheadSourceExchangePackets()	{ return m_nDownDataOverheadSourceExchangePackets; }
	static uint64	GetDownDataOverheadFileRequestPackets()		{ return m_nDownDataOverheadFileRequestPackets; }
	static uint64	GetDownDataOverheadServerPackets()			{ return m_nDownDataOverheadServerPackets; }
	static uint64	GetDownDataOverheadKadPackets()				{ return m_nDownDataOverheadKadPackets; }
	static uint64	GetDownDataOverheadOtherPackets()			{ return m_nDownDataOverheadOtherPackets; }


	///////////////////////////////////////////////////////////////////////////
	// Up Overhead
	//
	void	CompUpDatarateOverhead();
	void	ResetUpDatarateOverhead();
	static void	AddUpDataOverheadSourceExchange(uint32 data)	{ m_nUpDataRateMSOverhead += data;
																  m_nUpDataOverheadSourceExchange += data;
																  ++m_nUpDataOverheadSourceExchangePackets; }
	static void	AddUpDataOverheadFileRequest(uint32 data)		{ m_nUpDataRateMSOverhead += data;
																  m_nUpDataOverheadFileRequest += data;
																  ++m_nUpDataOverheadFileRequestPackets; }
	static void	AddUpDataOverheadServer(uint32 data)			{ m_nUpDataRateMSOverhead += data;
																  m_nUpDataOverheadServer += data;
																  ++m_nUpDataOverheadServerPackets; }
	static void	AddUpDataOverheadKad(uint32 data)				{ m_nUpDataRateMSOverhead += data;
																  m_nUpDataOverheadKad += data;
																  ++m_nUpDataOverheadKadPackets; }
	static void	AddUpDataOverheadOther(uint32 data)				{ m_nUpDataRateMSOverhead += data;
																  m_nUpDataOverheadOther += data;
																  ++m_nUpDataOverheadOtherPackets; }
	static void	AddUpDataOverheadCrypt(uint32 /*data*/)			{}

	static uint32	GetUpDatarateOverhead()						{ return m_nUpDatarateOverhead; }
	static uint64	GetUpDataOverheadSourceExchange()			{ return m_nUpDataOverheadSourceExchange; }
	static uint64	GetUpDataOverheadFileRequest()				{ return m_nUpDataOverheadFileRequest; }
	static uint64	GetUpDataOverheadServer()					{ return m_nUpDataOverheadServer; }
	static uint64	GetUpDataOverheadKad()						{ return m_nUpDataOverheadKad; }
	static uint64	GetUpDataOverheadOther()					{ return m_nUpDataOverheadOther; }
	static uint64	GetUpDataOverheadSourceExchangePackets()	{ return m_nUpDataOverheadSourceExchangePackets; }
	static uint64	GetUpDataOverheadFileRequestPackets()		{ return m_nUpDataOverheadFileRequestPackets; }
	static uint64	GetUpDataOverheadServerPackets()			{ return m_nUpDataOverheadServerPackets; }
	static uint64	GetUpDataOverheadKadPackets()				{ return m_nUpDataOverheadKadPackets; }
	static uint64	GetUpDataOverheadOtherPackets()				{ return m_nUpDataOverheadOtherPackets; }

public:
	//	Cumulative Stats
	static float	maxDown;
	static float	maxDownavg;
	static float	cumDownavg;
	static float	maxcumDownavg;
	static float	maxcumDown;
	static float	cumUpavg;
	static float	maxcumUpavg;
	static float	maxcumUp;
	static float	maxUp;
	static float	maxUpavg;
	static float	rateDown;
	static float	rateUp;
	static uint32	timeTransfers;
	static uint32	timeDownloads;
	static uint32	timeUploads;
	static uint32	start_timeTransfers;
	static uint32	start_timeDownloads;
	static uint32	start_timeUploads;
	static uint32	time_thisTransfer;
	static uint32	time_thisDownload;
	static uint32	time_thisUpload;
	static uint32	timeServerDuration;
	static uint32	time_thisServerDuration;
	static DWORD	m_dwOverallStatus;
	static float	m_fGlobalDone;
	static float	m_fGlobalSize;

	static uint64	sessionReceivedBytes;
	static uint64	sessionSentBytes;
    static uint64	sessionSentBytesToFriend;
	static uint16	reconnects;
	static DWORD	transferStarttime;
	static DWORD	serverConnectTime;
	static uint32	filteredclients;
	static DWORD	starttime;

private:
	typedef struct {
		uint32	datalen;
		DWORD	timestamp;
	} TransferredData;
	std::list<TransferredData> uprateHistory;
	std::list<TransferredData> downrateHistory;

	static uint32	m_nDownDatarateOverhead;
	static uint32	m_nDownDataRateMSOverhead;
	static uint64	m_nDownDataOverheadSourceExchange;
	static uint64	m_nDownDataOverheadSourceExchangePackets;
	static uint64	m_nDownDataOverheadFileRequest;
	static uint64	m_nDownDataOverheadFileRequestPackets;
	static uint64	m_nDownDataOverheadServer;
	static uint64	m_nDownDataOverheadServerPackets;
	static uint64	m_nDownDataOverheadKad;
	static uint64	m_nDownDataOverheadKadPackets;
	static uint64	m_nDownDataOverheadOther;
	static uint64	m_nDownDataOverheadOtherPackets;

	static uint32	m_nUpDatarateOverhead;
	static uint32	m_nUpDataRateMSOverhead;
	static uint64	m_nUpDataOverheadSourceExchange;
	static uint64	m_nUpDataOverheadSourceExchangePackets;
	static uint64	m_nUpDataOverheadFileRequest;
	static uint64	m_nUpDataOverheadFileRequestPackets;
	static uint64	m_nUpDataOverheadServer;
	static uint64	m_nUpDataOverheadServerPackets;
	static uint64	m_nUpDataOverheadKad;
	static uint64	m_nUpDataOverheadKadPackets;
	static uint64	m_nUpDataOverheadOther;
	static uint64	m_nUpDataOverheadOtherPackets;

	static uint32	m_sumavgDDRO;
	static uint32	m_sumavgUDRO;
	CList<TransferredData> m_AverageDDRO_list;
	CList<TransferredData> m_AverageUDRO_list;
};

extern CStatistics theStats;

#if !defined(_DEBUG) && !defined(_AFXDLL) && _MFC_VER==0x0710
//#define USE_MEM_STATS
#define	ALLOC_SLOTS	20
extern ULONGLONG g_aAllocStats[ALLOC_SLOTS];
#endif