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
#include "BarShader.h"
#include "ClientStateDefs.h"
#include "opcodes.h"
#include "OtherFunctions.h"

class CClientReqSocket;
class CPeerCacheDownSocket;
class CPeerCacheUpSocket;
class CFriend;
class CPartFile;
class CClientCredits;
class CAbstractFile;
class CKnownFile;
class Packet;
class CxImage;
struct Requested_Block_Struct;
class CSafeMemFile;
class CEMSocket;
class CAICHHash;
enum EUTF8str : uint8;

struct Pending_Block_Struct
{
	Pending_Block_Struct()
		: block()
		, zStream()
		, totalUnzipped()
		, fZStreamError()
		, fRecovered()
		, fQueued()
	{
	}
	Requested_Block_Struct	*block;
	struct z_stream_s		*zStream;		// Barry - Used to unzip packets
	UINT					totalUnzipped;	// Barry - This holds the total unzipped bytes for all packets so far
	UINT					fZStreamError : 1,
							fRecovered	  : 1,
							fQueued		  : 3;
};

#pragma pack(push, 1)
struct Requested_File_Struct
{
	uchar	  fileid[16];
	uint32	  lastasked;
	uint8	  badrequests;
};
#pragma pack(pop)

struct PartFileStamp
{
	CPartFile	*file;
	DWORD		timestamp;
};

#define	MAKE_CLIENT_VERSION(mjr, min, upd) \
	((UINT)(mjr)*100U*10U*100U + (UINT)(min)*100U*10U + (UINT)(upd)*100U)

class CUpDownClient : public CObject
{
	DECLARE_DYNAMIC(CUpDownClient)
	friend class CUploadQueue;
	void	Init();

public:
//	void PrintUploadStatus();

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Base
	explicit CUpDownClient(CClientReqSocket *sender = NULL);
	CUpDownClient(CPartFile *in_reqfile, uint16 in_port, uint32 in_userid, uint32 in_serverip, uint16 in_serverport, bool ed2kID = false);
	virtual	~CUpDownClient();

	void			StartDownload();
	virtual void	CheckDownloadTimeout();
	virtual void	SendCancelTransfer();
	virtual bool	IsEd2kClient() const							{ return true; }
	virtual bool	Disconnected(LPCTSTR pszReason, bool bFromSocket = false);
	virtual bool	TryToConnect(bool bIgnoreMaxCon = false, bool bNoCallbacks = false, CRuntimeClass *pClassSocket = NULL);
	virtual void	Connect();
	virtual void	ConnectionEstablished();
	virtual void	OnSocketConnected(int nErrorCode);
	bool			CheckHandshakeFinished() const;
	void			CheckFailedFileIdReqs(const uchar *aucFileHash);
	uint32			GetUserIDHybrid() const							{ return m_nUserIDHybrid; }
	void			SetUserIDHybrid(uint32 val)						{ m_nUserIDHybrid = val; }
	LPCTSTR			GetUserName() const								{ return m_pszUsername; }
	void			SetUserName(LPCTSTR pszNewName);
	uint32			GetIP() const									{ return m_dwUserIP; }
	//Only use this when you know the real IP or when your clearing it.
	void			SetIP(uint32 val)								{ m_dwUserIP = val; m_nConnectIP = val; }

	inline bool		HasLowID() const								{ return (m_nUserIDHybrid < 16777216u); } //0x01000000u
	uint32			GetConnectIP() const							{ return m_nConnectIP; }
	void			SetConnectIP(uint32 val)						{ m_nConnectIP = val; }
	uint16			GetUserPort() const								{ return m_nUserPort; }
	void			SetUserPort(uint16 val)							{ m_nUserPort = val; }
	UINT			GetTransferredUp() const						{ return m_nTransferredUp; }
	UINT			GetTransferredDown() const						{ return m_nTransferredDown; }
	uint32			GetServerIP() const								{ return m_dwServerIP; }
	void			SetServerIP(uint32 nIP)							{ m_dwServerIP = nIP; }
	uint16			GetServerPort() const							{ return m_nServerPort; }
	void			SetServerPort(uint16 nPort)						{ m_nServerPort = nPort; }
	const uchar*	GetUserHash() const								{ return (uchar*)m_achUserHash; }
	void			SetUserHash(const uchar *pucUserHash);
	bool			HasValidHash() const							{ return !isnulmd4(m_achUserHash); }
	int				GetHashType() const;
	const uchar*	GetBuddyID() const								{ return (uchar*)m_achBuddyID; }
	void			SetBuddyID(const uchar *pucBuddyID);
	bool			HasValidBuddyID() const							{ return m_bBuddyIDValid; }
	void			SetBuddyIP(uint32 val)							{ m_nBuddyIP = val; }
	uint32			GetBuddyIP() const								{ return m_nBuddyIP; }
	void			SetBuddyPort(uint16 val)						{ m_nBuddyPort = val; }
	uint16			GetBuddyPort() const							{ return m_nBuddyPort; }
	EClientSoftware	GetClientSoft() const							{ return m_clientSoft; }
	const CString&	GetClientSoftVer() const						{ return m_strClientSoftware; }
	const CString&	GetClientModVer() const							{ return m_strModVersion; }
	void			InitClientSoftwareVersion();
	UINT			GetVersion() const								{ return m_nClientVersion; }
	uint8			GetMuleVersion() const							{ return m_byEmuleVersion; }
	bool			ExtProtocolAvailable() const					{ return m_bEmuleProtocol; }
	bool			SupportMultiPacket() const						{ return m_bMultiPacket; }
	bool			SupportExtMultiPacket() const					{ return m_fExtMultiPacket; }
	bool			SupportPeerCache() const						{ return m_fPeerCache; }
	bool			SupportsLargeFiles() const						{ return m_fSupportsLargeFiles; }
	bool			SupportsFileIdentifiers() const					{ return m_fSupportsFileIdent; }
	bool			IsEmuleClient() const							{ return m_byEmuleVersion!=0; }
	uint8			GetSourceExchange1Version() const				{ return m_bySourceExchange1Ver; }
	bool			SupportsSourceExchange2() const					{ return m_fSupportsSourceEx2; }
	CClientCredits*	Credits() const									{ return credits; }
	bool			IsBanned() const;
	const CString&	GetClientFilename() const						{ return m_strClientFilename; }
	void			SetClientFilename(const CString &fileName)		{ m_strClientFilename = fileName; }
	uint16			GetUDPPort() const								{ return m_nUDPPort; }
	void			SetUDPPort(uint16 nPort)						{ m_nUDPPort = nPort; }
	uint8			GetUDPVersion() const							{ return m_byUDPVer; }
	bool			SupportsUDP() const								{ return GetUDPVersion() != 0 && m_nUDPPort != 0; }
	uint16			GetKadPort() const								{ return m_nKadPort; }
	void			SetKadPort(uint16 nPort)						{ m_nKadPort = nPort; }
	uint8			GetExtendedRequestsVersion() const				{ return m_byExtendedRequestsVer; }
	void			RequestSharedFileList();
	void			ProcessSharedFileList(const uchar *pachPacket, uint32 nSize, LPCTSTR pszDirectory = NULL);
	EConnectingState GetConnectingState() const						{ return m_eConnectingState; }

	void			ClearHelloProperties();
	bool			ProcessHelloAnswer(const uchar *pachPacket, uint32 nSize);
	bool			ProcessHelloPacket(const uchar *pachPacket, uint32 nSize);
	void			SendHelloAnswer();
	virtual void	SendHelloPacket();
	void			SendMuleInfoPacket(bool bAnswer);
	void			ProcessMuleInfoPacket(const uchar *pachPacket, uint32 nSize);
	void			ProcessMuleCommentPacket(const uchar *pachPacket, uint32 nSize);
	void			ProcessEmuleQueueRank(const uchar *packet, UINT size);
	void			ProcessEdonkeyQueueRank(const uchar *packet, UINT size);
	void			CheckQueueRankFlood();
	bool			Compare(const CUpDownClient *tocomp, bool bIgnoreUserhash = false) const;
	void			ResetFileStatusInfo();
	uint32			GetLastSrcReqTime() const						{ return m_dwLastSourceRequest; }
	void			SetLastSrcReqTime()								{ m_dwLastSourceRequest = ::GetTickCount(); }
	uint32			GetLastSrcAnswerTime() const					{ return m_dwLastSourceAnswer; }
	void			SetLastSrcAnswerTime()							{ m_dwLastSourceAnswer = ::GetTickCount(); }
	uint32			GetLastAskedForSources() const					{ return m_dwLastAskedForSources; }
	void			SetLastAskedForSources()						{ m_dwLastAskedForSources = ::GetTickCount(); }
	bool			GetFriendSlot() const;
	void			SetFriendSlot(bool bNV)							{ m_bFriendSlot = bNV; }
	bool			IsFriend() const								{ return m_Friend != NULL; }
	CFriend*		GetFriend() const;
	void			SetCommentDirty(bool bDirty = true)				{ m_bCommentDirty = bDirty; }
	bool			GetSentCancelTransfer() const					{ return m_fSentCancelTransfer; }
	void			SetSentCancelTransfer(bool bVal)				{ m_fSentCancelTransfer = bVal; }
	void			ProcessPublicIPAnswer(const BYTE *pbyData, UINT uSize);
	void			SendPublicIPRequest();
	uint8			GetKadVersion()	const							{ return m_byKadVersion; }
	bool			SendBuddyPingPong()								{ return ::GetTickCount() >= m_dwLastBuddyPingPongTime; }
	bool			AllowIncomeingBuddyPingPong()					{ return ::GetTickCount() >= m_dwLastBuddyPingPongTime + MIN2MS(3); }
	void			SetLastBuddyPingPongTime()						{ m_dwLastBuddyPingPongTime = ::GetTickCount() + MIN2MS(10); }
	void			ProcessFirewallCheckUDPRequest(CSafeMemFile *data);
	void			SendSharedDirectories();

	// secure ident
	void			SendPublicKeyPacket();
	void			SendSignaturePacket();
	void			ProcessPublicKeyPacket(const uchar *pachPacket, uint32 nSize);
	void			ProcessSignaturePacket(const uchar *pachPacket, uint32 nSize);
	uint8			GetSecureIdentState() const						{ return (uint8)m_SecureIdentState; }
	void			SendSecIdentStatePacket();
	void			ProcessSecIdentStatePacket(const uchar *pachPacket, uint32 nSize);
	uint8			GetInfoPacketsReceived() const					{ return m_byInfopacketsReceived; }
	void			InfoPacketsReceived();
	bool			HasPassedSecureIdent(bool bPassIfUnavailable) const;
	// preview
	void			SendPreviewRequest(const CAbstractFile *pForFile);
	void			SendPreviewAnswer(const CKnownFile *pForFile, CxImage **imgFrames, uint8 nCount);
	void			ProcessPreviewReq(const uchar *pachPacket, uint32 nSize);
	void			ProcessPreviewAnswer(const uchar *pachPacket, uint32 nSize);
	bool			GetPreviewSupport() const						{ return m_fSupportsPreview && GetViewSharedFilesSupport(); }
	bool			GetViewSharedFilesSupport() const				{ return m_fNoViewSharedFiles==0; }
	bool			SafeConnectAndSendPacket(Packet *packet);
	bool			SendPacket(Packet *packet, bool bVerifyConnection = false);
	void			CheckForGPLEvilDoer();
	// Encryption / Obfuscation / Connect options
	bool			SupportsCryptLayer() const						{ return m_fSupportsCryptLayer; }
	bool			RequestsCryptLayer() const						{ return SupportsCryptLayer() && m_fRequestsCryptLayer; }
	bool			RequiresCryptLayer() const						{ return RequestsCryptLayer() && m_fRequiresCryptLayer; }
	bool			SupportsDirectUDPCallback() const				{ return m_fDirectUDPCallback != 0 && HasValidHash() && GetKadPort() != 0; }
	void			SetCryptLayerSupport(bool bVal)					{ m_fSupportsCryptLayer = static_cast<UINT>(bVal); }
	void			SetCryptLayerRequest(bool bVal)					{ m_fRequestsCryptLayer = static_cast<UINT>(bVal); }
	void			SetCryptLayerRequires(bool bVal)				{ m_fRequiresCryptLayer = static_cast<UINT>(bVal); }
	void			SetDirectUDPCallbackSupport(bool bVal)			{ m_fDirectUDPCallback = static_cast<UINT>(bVal); }
	void			SetConnectOptions(uint8 byOptions, bool bEncryption = true, bool bCallback = true); // shortcut, sets crypt, callback etc based from the tag value we receive
	bool			IsObfuscatedConnectionEstablished() const;
	bool			ShouldReceiveCryptUDPPackets() const;

	void			GetDisplayImage(int &iImage, UINT &uOverlayImage) const;
	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Upload
	EUploadState	GetUploadState() const							{ return m_eUploadState; }
	void			SetUploadState(EUploadState eNewState);
	uint32			GetWaitStartTime() const;
	void			SetWaitStartTime();
	void			ClearWaitStartTime();
	uint32			GetWaitTime() const								{ return m_dwUploadTime - GetWaitStartTime(); }
	bool			IsDownloading() const							{ return (m_eUploadState == US_UPLOADING); }
	UINT			GetDatarate() const								{ return m_nUpDatarate; }
	UINT			GetScore(bool sysvalue, bool isdownloading = false, bool onlybasevalue = false) const;
	void			AddReqBlock(Requested_Block_Struct *reqblock, bool bSignalIOThread);
	DWORD			GetUpStartTime() const							{ return m_dwUploadTime; }
	DWORD			GetUpStartTimeDelay() const						{ return ::GetTickCount() - m_dwUploadTime; }
	void			SetUpStartTime()								{ m_dwUploadTime = ::GetTickCount(); }
	void			SendHashsetPacket(const uchar *pData, uint32 nSize, bool bFileIdentifiers);
	const uchar*	GetUploadFileID() const							{ return requpfileid; }
	void			SetUploadFileID(CKnownFile *newreqfile);
	uint32			UpdateUploadingStatisticsData();
	void			SendRankingInfo();
	void			SendCommentInfo(/*const */CKnownFile *file);
	void			AddRequestCount(const uchar *fileid);
	void			UnBan();
	void			Ban(LPCTSTR pszReason = NULL);
	UINT			GetAskedCount() const							{ return m_cAsked; }
	void			AddAskedCount()									{ ++m_cAsked; }
	void			SetAskedCount(UINT m_cInAsked)					{ m_cAsked = m_cInAsked; }
	void			FlushSendBlocks(); // call this when you stop upload, or the socket might be not able to send
	uint32			GetLastUpRequest() const						{ return m_dwLastUpRequest; }
	void			SetLastUpRequest()								{ m_dwLastUpRequest = ::GetTickCount(); }
	void			SetCollectionUploadSlot(bool bValue);
	bool			HasCollectionUploadSlot() const					{ return m_bCollectionUploadSlot; }

	UINT			GetSessionUp() const							{ return m_nTransferredUp - m_nCurSessionUp; }
	void			ResetSessionUp() {
						m_nCurSessionUp = m_nTransferredUp;
						m_addedPayloadQueueSession = 0;
						m_nCurQueueSessionPayloadUp = 0;
					}

	UINT			GetSessionDown() const							{ return m_nTransferredDown - m_nCurSessionDown; }
	UINT			GetSessionPayloadDown() const					{ return m_nCurSessionPayloadDown; }
	void			ResetSessionDown()								{ m_nCurSessionDown = m_nTransferredDown; m_nCurSessionPayloadDown = 0; }
	UINT			GetQueueSessionPayloadUp() const				{ return m_nCurQueueSessionPayloadUp; } // Data uploaded/transmitted
	UINT			GetQueueSessionUploadAdded() const				{ return m_addedPayloadQueueSession; } // Data put into upload buffers
	UINT			GetPayloadInBuffer() const						{ return m_addedPayloadQueueSession - m_nCurQueueSessionPayloadUp; }
	void			SetQueueSessionUploadAdded(UINT uVal)			{ m_addedPayloadQueueSession = uVal; }

	bool			ProcessExtendedInfo(CSafeMemFile *data, CKnownFile *tempreqfile);
	uint16			GetUpPartCount() const							{ return m_nUpPartCount; }
	void			DrawUpStatusBar(CDC *dc, const CRect &rect, bool onlygreyrect, bool  bFlat) const;
	bool			IsUpPartAvailable(UINT iPart) const {
						return (iPart >= m_nUpPartCount || !m_abyUpPartStatus) ? false : m_abyUpPartStatus[iPart] != 0;
					}
	uint8*			GetUpPartStatus() const							{ return m_abyUpPartStatus; }
	float			GetCombinedFilePrioAndCredit();
	uint8			GetDataCompressionVersion() const				{ return m_byDataCompVer; }

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Download
	UINT			GetAskedCountDown() const						{ return m_cDownAsked; }
	void			AddAskedCountDown()								{ ++m_cDownAsked; }
	void			SetAskedCountDown(UINT cInDownAsked)			{ m_cDownAsked = cInDownAsked; }
	EDownloadState	GetDownloadState() const						{ return m_eDownloadState; }
	void			SetDownloadState(EDownloadState nNewState, LPCTSTR pszReason = _T("Unspecified"));
	DWORD			GetLastAskedTime(const CPartFile *partFile = NULL) const;
	void			SetLastAskedTime()								{ m_fileReaskTimes.SetAt(m_reqfile, ::GetTickCount()); }
	bool			IsPartAvailable(UINT iPart) const				{ return m_abyPartStatus && iPart < m_nPartCount && m_abyPartStatus[iPart] != 0; }
	uint8*			GetPartStatus() const							{ return m_abyPartStatus; }
	uint16			GetPartCount() const							{ return m_nPartCount; }
	UINT			GetDownloadDatarate() const						{ return m_nDownDatarate; }
	UINT			GetRemoteQueueRank() const						{ return m_nRemoteQueueRank; }
	void			SetRemoteQueueRank(UINT nr, bool bUpdateDisplay = false);
	bool			IsRemoteQueueFull() const						{ return m_bRemoteQueueFull; }
	void			SetRemoteQueueFull(bool flag)					{ m_bRemoteQueueFull = flag; }
	void			DrawStatusBar(CDC *dc, const CRect &rect, bool onlygreyrect, bool  bFlat) const;
	bool			AskForDownload();
	virtual void	SendFileRequest();
	void			SendStartupLoadReq();
	void			ProcessFileInfo(CSafeMemFile *data, CPartFile *file);
	void			ProcessFileStatus(bool bUdpPacket, CSafeMemFile *data, CPartFile *file);
	void			ProcessHashSet(const uchar *packet, uint32 size, bool bFileIdentifiers);
	void			ProcessAcceptUpload();
	bool			AddRequestForAnotherFile(CPartFile *file);
	void			CreateBlockRequests(int iMinBlocks, int iMaxBlocks);
	virtual void	SendBlockRequests();
	virtual bool	SendHttpBlockRequests();
	virtual void	ProcessBlockPacket(const uchar *packet, uint32 size, bool packed, bool bI64Offsets);
	virtual void	ProcessHttpBlockPacket(const BYTE *pucData, UINT uSize);
	void			ClearDownloadBlockRequests();
	void			SendOutOfPartReqsAndAddToWaitingQueue();
	UINT			CalculateDownloadRate();
	uint16			GetAvailablePartCount() const;
	bool			SwapToAnotherFile(LPCTSTR reason, bool bIgnoreNoNeeded, bool ignoreSuspensions, bool bRemoveCompletely, CPartFile *toFile = NULL, bool allowSame = true, bool isAboutToAsk = false, bool debug = false); // ZZ:DownloadManager
	void			DontSwapTo(/*const*/ CPartFile *file);
	bool			IsSwapSuspended(const CPartFile *file, const bool allowShortReaskTime = false, const bool fileIsNNP = false) /*const*/; // ZZ:DownloadManager
	uint32			GetTimeUntilReask() const;
	uint32			GetTimeUntilReask(const CPartFile *file) const;
	uint32			GetTimeUntilReask(const CPartFile *file, const bool allowShortReaskTime, const bool useGivenNNP = false, const bool givenNNP = false) const;
	void			UDPReaskACK(uint16 nNewQR);
	void			UDPReaskFNF();
	void			UDPReaskForDownload();
	bool			UDPPacketPending() const						{ return m_bUDPPending; }
	bool			IsSourceRequestAllowed() const;
	bool			IsSourceRequestAllowed(CPartFile *partfile, bool sourceExchangeCheck = false) const; // ZZ:DownloadManager

	bool			IsValidSource() const;
	ESourceFrom		GetSourceFrom() const							{ return m_eSourceFrom; }
	void			SetSourceFrom(const ESourceFrom val)			{ m_eSourceFrom = val; }

	void			SetDownStartTime()								{ m_dwDownStartTime = ::GetTickCount(); }
	DWORD			GetDownTimeDifference(boolean clear = true)
					{
						DWORD myTime = m_dwDownStartTime;
						if (clear)
							m_dwDownStartTime = 0;
						return ::GetTickCount() - myTime;
					}
	bool			GetTransferredDownMini() const					{ return m_bTransferredDownMini; }
	void			SetTransferredDownMini()						{ m_bTransferredDownMini = true; }
	void			InitTransferredDownMini()						{ m_bTransferredDownMini = false; }
	UINT			GetA4AFCount() const							{ return static_cast<UINT>(m_OtherRequests_list.GetCount()); }

	uint16			GetUpCompleteSourcesCount() const				{ return m_nUpCompleteSourcesCount; }
	void			SetUpCompleteSourcesCount(uint16 n)				{ m_nUpCompleteSourcesCount = n; }

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Chat
	EChatState		GetChatState() const							{ return m_eChatstate; }
	void			SetChatState(const EChatState nNewS)			{ m_eChatstate = nNewS; }
	EChatCaptchaState GetChatCaptchaState() const					{ return m_eChatCaptchaState; }
	void			SetChatCaptchaState(const EChatCaptchaState nNewS)	{ m_eChatCaptchaState = nNewS; }
	void			ProcessChatMessage(CSafeMemFile *data, uint32 nLength);
	void			SendChatMessage(const CString &strMessage);
	void			ProcessCaptchaRequest(CSafeMemFile *data);
	void			ProcessCaptchaReqRes(uint8 nStatus);
	// message filtering
	uint8			GetMessagesReceived() const						{ return m_cMessagesReceived; }
	void			SetMessagesReceived(uint8 nCount)				{ m_cMessagesReceived = nCount; }
	void			IncMessagesReceived()							{ m_cMessagesReceived < 255 ? ++m_cMessagesReceived : 255; }
	uint8			GetMessagesSent() const							{ return m_cMessagesSent; }
	void			SetMessagesSent(uint8 nCount)					{ m_cMessagesSent = nCount; }
	void			IncMessagesSent()								{ m_cMessagesSent < 255 ? ++m_cMessagesSent : 255; }
	bool			IsSpammer() const								{ return m_fIsSpammer; }
	void			SetSpammer(bool bVal);
	bool			GetMessageFiltered() const						{ return m_fMessageFiltered; }
	void			SetMessageFiltered(bool bVal);


	//KadIPCheck
	EKadState		GetKadState() const								{ return m_eKadState; }
	void			SetKadState(const EKadState nNewS)				{ m_eKadState = nNewS; }

	//File Comment
	bool			HasFileComment() const							{ return !m_strFileComment.IsEmpty(); }
	const CString&	GetFileComment() const							{ return m_strFileComment; }
	void			SetFileComment(LPCTSTR pszComment)				{ m_strFileComment = pszComment; }

	bool			HasFileRating() const							{ return m_uFileRating > 0; }
	uint8			GetFileRating() const							{ return m_uFileRating; }
	void			SetFileRating(uint8 uRating)					{ m_uFileRating = uRating; }

	// Barry - Process zip file as it arrives, don't need to wait until end of block
	int				unzip(Pending_Block_Struct *block, const BYTE *zipped, uint32 lenZipped, BYTE **unzipped, uint32 *lenUnzipped, int iRecursion = 0);
	void			UpdateDisplayedInfo(bool force = false);
	int				GetFileListRequested() const					{ return m_iFileListRequested; }
	void			SetFileListRequested(int iFileListRequested)	{ m_iFileListRequested = iFileListRequested; }

	virtual void	SetRequestFile(CPartFile *pReqFile);
	CPartFile*		GetRequestFile() const							{ return m_reqfile; }

	// AICH Stuff
	void			SetReqFileAICHHash(CAICHHash *val);
	CAICHHash*		GetReqFileAICHHash() const						{ return m_pReqFileAICHHash; }
	bool			IsSupportingAICH() const						{ return m_fSupportsAICH & 0x01; }
	void			SendAICHRequest(CPartFile *pForFile, uint16 nPart);
	bool			IsAICHReqPending() const						{ return m_fAICHRequested; }
	void			ProcessAICHAnswer(const uchar *packet, UINT size);
	void			ProcessAICHRequest(const uchar *packet, UINT size);
	void			ProcessAICHFileHash(CSafeMemFile *data, CPartFile *file, const CAICHHash *pAICHHash);

	EUTF8str		GetUnicodeSupport() const;

	CString			GetDownloadStateDisplayString() const;
	CString			GetUploadStateDisplayString() const;

	LPCTSTR			DbgGetDownloadState() const;
	LPCTSTR			DbgGetUploadState() const;
	LPCTSTR			DbgGetKadState() const;
	CString			DbgGetClientInfo(bool bFormatIP = false) const;
	CString			DbgGetFullClientSoftVer() const;
	const CString&	DbgGetHelloInfo() const							{ return m_strHelloInfo; }
	const CString&	DbgGetMuleInfo() const							{ return m_strMuleInfo; }

// ZZ:DownloadManager -->
	const bool		IsInNoNeededList(const CPartFile *fileToCheck) const;
	const bool		SwapToRightFile(CPartFile *SwapTo, CPartFile *cur_file, bool ignoreSuspensions, bool SwapToIsNNPFile, bool curFileisNNPFile, bool &wasSkippedDueToSourceExchange, bool doAgressiveSwapping = false, bool debug = false);
	const DWORD		getLastTriedToConnectTime() const				{ return m_dwLastTriedToConnect; }
	void			setLastTriedToConnectTime()						{ m_dwLastTriedToConnect = ::GetTickCount(); }
// <-- ZZ:DownloadManager

#ifdef _DEBUG
	// Diagnostic Support
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext &dc) const;
#endif

	CClientReqSocket *socket;
	CClientCredits	*credits;
	CFriend			*m_Friend;
	uint8			*m_abyUpPartStatus;
	CTypedPtrList<CPtrList, CPartFile*> m_OtherRequests_list;
	CTypedPtrList<CPtrList, CPartFile*> m_OtherNoNeeded_list;
	uint16			m_lastPartAsked;
	bool			m_bAddNextConnect;

	void			SetSlotNumber(UINT newValue)					{ m_slotNumber = newValue; }
	UINT			GetSlotNumber() const							{ return m_slotNumber; }
	CEMSocket*		GetFileUploadSocket(bool bLog = false);

	///////////////////////////////////////////////////////////////////////////
	// PeerCache client
	//
	bool IsDownloadingFromPeerCache() const;
	bool IsUploadingToPeerCache() const;
	void SetPeerCacheDownState(EPeerCacheDownState eState);
	void SetPeerCacheUpState(EPeerCacheUpState eState);

	int  GetHttpSendState() const									{ return m_iHttpSendState; }
	void SetHttpSendState(int iState)								{ m_iHttpSendState = iState; }

	bool SendPeerCacheFileRequest();
	bool ProcessPeerCacheQuery(const uchar *packet, UINT size);
	bool ProcessPeerCacheAnswer(const uchar *packet, UINT size);
	bool ProcessPeerCacheAcknowledge(const uchar *packet, UINT size);
	void OnPeerCacheDownSocketClosed(int nErrorCode);
	bool OnPeerCacheDownSocketTimeout();

	bool ProcessPeerCacheDownHttpResponse(const CStringAArray &astrHeaders);
	bool ProcessPeerCacheDownHttpResponseBody(const BYTE *pucData, UINT uSize);
	void ProcessPeerCacheUpHttpResponse(const CStringAArray &astrHeaders);
	UINT ProcessPeerCacheUpHttpRequest(const CStringAArray &astrHeaders);

	virtual bool ProcessHttpDownResponse(const CStringAArray &astrHeaders);
	virtual bool ProcessHttpDownResponseBody(const BYTE *pucData, UINT uSize);

	CPeerCacheDownSocket *m_pPCDownSocket;
	CPeerCacheUpSocket *m_pPCUpSocket;

protected:
	int		m_iHttpSendState;
	uint32	m_uPeerCacheDownloadPushId;
	uint32	m_uPeerCacheUploadPushId;
	uint32	m_uPeerCacheRemoteIP;
	bool	m_bPeerCacheDownHit;
	bool	m_bPeerCacheUpHit;
	EPeerCacheDownState m_ePeerCacheDownState;
	EPeerCacheUpState m_ePeerCacheUpState;

	// base
	bool	ProcessHelloTypePacket(CSafeMemFile *data);
	void	SendHelloTypePacket(CSafeMemFile *data);
	void	SendFirewallCheckUDPRequest();
	void	SendHashSetRequest();

	bool	DoSwap(CPartFile *SwapTo, bool bRemoveCompletely, LPCTSTR reason); // ZZ:DownloadManager
	bool	RecentlySwappedForSourceExchange()		{ return ::GetTickCount() < lastSwapForSourceExchangeTick + SEC2MS(30); } // ZZ:DownloadManager
	void	SetSwapForSourceExchangeTick()			{ lastSwapForSourceExchangeTick = ::GetTickCount(); } // ZZ:DownloadManager

	uint32	m_nConnectIP;	// holds the supposed IP or (after we had a connection) the real IP
	uint32	m_dwUserIP;		// holds 0 (real IP not yet available) or the real IP (after we had a connection)
	uint32	m_dwServerIP;
	uint32	m_nUserIDHybrid;
	uint16	m_nUserPort;
	uint16	m_nServerPort;
	UINT	m_nClientVersion;
	//--group aligned to int32
	uint8	m_byEmuleVersion;
	uint8	m_byDataCompVer;
	bool	m_bEmuleProtocol;
	bool	m_bIsHybrid;
	//--group aligned to int32
	TCHAR	*m_pszUsername;
	uchar	m_achUserHash[MDX_DIGEST_SIZE];
	uint16	m_nUDPPort;
	uint16	m_nKadPort;
	//--group aligned to int32
	uint8	m_byUDPVer;
	uint8	m_bySourceExchange1Ver;
	uint8	m_byAcceptCommentVer;
	uint8	m_byExtendedRequestsVer;
	//--group aligned to int32
	uint8	m_byCompatibleClient;
	bool	m_bFriendSlot;
	bool	m_bCommentDirty;
	bool	m_bIsML;
	//--group aligned to int32
	bool	m_bGPLEvildoer;
	bool	m_bHelloAnswerPending;
	uint8	m_byInfopacketsReceived; // have we received the edonkeyprot and emuleprot packet already (see InfoPacketsReceived() )
	uint8	m_bySupportSecIdent;
	//--group aligned to int32
	uint32	m_dwLastSignatureIP;
	CString m_strClientSoftware;
	CString m_strModVersion;
	uint32	m_dwLastSourceRequest;
	uint32	m_dwLastSourceAnswer;
	uint32	m_dwLastAskedForSources;
	int		m_iFileListRequested;
	CString	m_strFileComment;
	//--group aligned to int32
	uint8	m_uFileRating;
	uint8	m_cMessagesReceived;	// count of chatmessages he sent to me
	uint8	m_cMessagesSent;		// count of chatmessages I sent to him
	bool	m_bMultiPacket;
	//--group aligned to int32
	bool	m_bUnicodeSupport;
	bool	m_bBuddyIDValid;
	uint16	m_nBuddyPort;
	//--group aligned to int32
	uint8	m_byKadVersion;
	uint8	m_cCaptchasSent;

	uint32	m_nBuddyIP;
	uint32	m_dwLastBuddyPingPongTime;
	uchar	m_achBuddyID[MDX_DIGEST_SIZE];
	CString m_strHelloInfo;
	CString m_strMuleInfo;
	CString m_strCaptchaChallenge;
	CString m_strCaptchaPendingMsg;

	// States
	EClientSoftware		m_clientSoft;
	EChatState			m_eChatstate;
	EKadState			m_eKadState;
	ESecureIdentState	m_SecureIdentState;
	EUploadState		m_eUploadState;
	EDownloadState		m_eDownloadState;
	ESourceFrom			m_eSourceFrom;
	EChatCaptchaState	m_eChatCaptchaState;
	EConnectingState	m_eConnectingState;

	CTypedPtrList<CPtrList, Packet*> m_WaitingPackets_list;
	CList<PartFileStamp> m_DontSwap_list;

	////////////////////////////////////////////////////////////////////////
	// Upload
	//
	int GetFilePrioAsNumber() const;

	UINT		m_nTransferredUp;
	DWORD		m_dwUploadTime;
	UINT		m_cAsked;
	uint32		m_dwLastUpRequest;
	UINT		m_nCurSessionUp;
	UINT		m_nCurSessionDown;
	UINT		m_nCurQueueSessionPayloadUp;
	UINT		m_addedPayloadQueueSession;
	uint16		m_nUpPartCount;
	uint16		m_nUpCompleteSourcesCount;
	uchar		requpfileid[16];
	UINT		m_slotNumber;
	bool		m_bCollectionUploadSlot;

	typedef struct
	{
		UINT	datalen;
		DWORD	timestamp;
	} TransferredData;
	CTypedPtrList<CPtrList, Requested_File_Struct*>	 m_RequestedFiles_list;

	//////////////////////////////////////////////////////////
	// Download
	//
	CPartFile	*m_reqfile;
	CAICHHash	*m_pReqFileAICHHash;
	uint8		*m_abyPartStatus;
	CString		m_strClientFilename;
	UINT		m_cDownAsked;
	UINT		m_nTransferredDown;
	UINT		m_nCurSessionPayloadDown;
	DWORD		m_dwDownStartTime;
	uint64		m_nLastBlockOffset;
	uint32		m_dwLastBlockReceived;
	UINT		m_nTotalUDPPackets;
	UINT		m_nFailedUDPPackets;
	UINT		m_nRemoteQueueRank;
	//--group aligned to int32
	bool		m_bRemoteQueueFull;
	bool		m_bCompleteSource;
	uint16		m_nPartCount;
	//--group aligned to int32
	uint16		m_cShowDR;
	bool		m_bReaskPending;
	bool		m_bUDPPending;
	bool		m_bTransferredDownMini;
//	bool		m_bHasMatchingAICHHash;

	// Download from URL
	CStringA	m_strUrlPath;
	uint64		m_uReqStart;
	uint64		m_uReqEnd;
	uint64		m_nUrlStartPos;

	//////////////////////////////////////////////////////////
	// Upload data rate computation
	//
	UINT		m_nUpDatarate;
	UINT		m_nSumForAvgUpDataRate;
	CList<TransferredData> m_AverageUDR_list;

	//////////////////////////////////////////////////////////
	// Download data rate computation
	//
	UINT		m_nDownDatarate;
	UINT		m_nDownDataRateMS;
	UINT		m_nSumForAvgDownDataRate;
	CList<TransferredData> m_AverageDDR_list;

	//////////////////////////////////////////////////////////
	// GUI helpers
	//
	static CBarShader s_StatusBar;
	static CBarShader s_UpStatusBar;
	CTypedPtrList<CPtrList, Pending_Block_Struct*> m_PendingBlocks_list;
	CMap<CPartFile*, CPartFile*, DWORD, DWORD> m_fileReaskTimes; // ZZ:DownloadManager (one re-ask timestamp for each file)
	DWORD		m_lastRefreshedDLDisplay;
	DWORD		m_lastRefreshedULDisplay;
	uint32		m_random_update_wait;

	// using bit fields for less important flags, to save some bytes
	UINT m_fHashsetRequestingMD4 : 1, // we have sent a hashset request to this client in the current connection
		 m_fSharedDirectories : 1, // client supports OP_ASKSHAREDIRS opcodes
		 m_fSentCancelTransfer: 1, // we have sent an OP_CANCELTRANSFER in the current connection
		 m_fNoViewSharedFiles : 1, // client has disabled the 'View Shared Files' feature, if this flag is not set, we just know that we don't know for sure if it is enabled
		 m_fSupportsPreview   : 1,
		 m_fPreviewReqPending : 1,
		 m_fPreviewAnsPending : 1,
		 m_fIsSpammer		  : 1,
		 m_fMessageFiltered   : 1,
		 m_fPeerCache		  : 1,
		 m_fQueueRankPending  : 1,
		 m_fUnaskQueueRankRecv: 2,
		 m_fFailedFileIdReqs  : 4, // nr. of failed file-id related requests per connection
		 m_fNeedOurPublicIP	  : 1, // we requested our IP from this client
		 m_fSupportsAICH	  : 3,
		 m_fAICHRequested	  : 1,
		 m_fSentOutOfPartReqs : 1,
		 m_fSupportsLargeFiles: 1,
		 m_fExtMultiPacket	  : 1,
		 m_fRequestsCryptLayer: 1,
		 m_fSupportsCryptLayer: 1,
		 m_fRequiresCryptLayer: 1,
		 m_fSupportsSourceEx2 : 1,
		 m_fSupportsCaptcha	  : 1,
		 m_fDirectUDPCallback : 1,
		 m_fSupportsFileIdent : 1; // 0 bits left
	UINT m_fHashsetRequestingAICH : 1; // 31 bits left

	DWORD   lastSwapForSourceExchangeTick; // ZZ:DownloadManaager
	DWORD   m_dwLastTriedToConnect; // ZZ:DownloadManager (one re-ask timestamp for each file)
	bool	m_bSourceExchangeSwapped; // ZZ:DownloadManager
};