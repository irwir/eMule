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
#include <io.h>
#include "emule.h"
#include "DownloadQueue.h"
#include "UpDownClient.h"
#include "PartFile.h"
#include "ed2kLink.h"
#include "SearchFile.h"
#include "ClientList.h"
#include "Statistics.h"
#include "SharedFileList.h"
#include "OtherFunctions.h"
#include "SafeFile.h"
#include "ServerConnect.h"
#include "ServerList.h"
#include "Server.h"
#include "Packets.h"
#include "Kademlia/Kademlia/Kademlia.h"
#include "kademlia/utils/uint128.h"
#include "ipfilter.h"
#include "emuledlg.h"
#include "TransferDlg.h"
#include "TaskbarNotifier.h"
#include "MenuCmds.h"
#include "Log.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CDownloadQueue::CDownloadQueue()
	: cur_udpserver()
	, m_datarateMS()
	, lastfile()
	, m_dwLastA4AFtime()
	, lastcheckdiskspacetime()
	, lastudpsearchtime()
	, lastudpstattime()
	, udcounter()
	, m_cRequestsSentToServer()
	, m_dwNextTCPSrcReq()
	, m_iSearchedServers()
	, m_nUDPFileReasks()
	, m_nFailedUDPFileReasks()
	, datarate()
	, filesrdy()
{
	SetLastKademliaFileRequest();
}

void CDownloadQueue::AddPartFilesToShare()
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);
		if (cur_file->GetStatus(true) == PS_READY)
			theApp.sharedfiles->SafeAddKFile(cur_file, true);
	}
}

void CDownloadQueue::Init()
{
	// find all part files, read & hash them if needed and store into a list
	CFileFind ff;
	int count = 0;

	for (INT_PTR i = 0; i < thePrefs.GetTempDirCount(); ++i) {
		CString searchPath;
		searchPath.Format(_T("%s\\*.part.met"), (LPCTSTR)thePrefs.GetTempDir(i));

		//check all part.met files
		bool end = !ff.FindFile(searchPath, 0);
		while (!end) {
			end = !ff.FindNextFile();
			if (ff.IsDirectory())
				continue;
			CPartFile *toadd = new CPartFile();
			EPartFileLoadResult eResult = toadd->LoadPartFile(thePrefs.GetTempDir(i), ff.GetFileName());
			if (eResult == PLR_FAILED_METFILE_CORRUPT) {
				// .met file is corrupted, try to load the latest backup of this file
				delete toadd;
				toadd = new CPartFile();
				eResult = toadd->LoadPartFile(thePrefs.GetTempDir(i), ff.GetFileName() + PARTMET_BAK_EXT);
				if (eResult == PLR_LOADSUCCESS) {
					toadd->SavePartFile(true); // don't override our just used .bak file yet
					AddLogLine(false, GetResString(IDS_RECOVERED_PARTMET), (LPCTSTR)toadd->GetFileName());
				}
			}

			if (eResult == PLR_LOADSUCCESS) {
				++count;
				filelist.AddTail(toadd); // to download queue
				if (toadd->GetStatus(true) == PS_READY)
					theApp.sharedfiles->SafeAddKFile(toadd); // part files are always shared files
				theApp.emuledlg->transferwnd->GetDownloadList()->AddFile(toadd); // show in download window
			} else
				delete toadd;
		}
		ff.Close();

		//try recovering any part.met files
		searchPath += _T(".backup");
		end = !ff.FindFile(searchPath, 0);
		while (!end) {
			end = !ff.FindNextFile();
			if (ff.IsDirectory())
				continue;
			CPartFile *toadd = new CPartFile();
			if (toadd->LoadPartFile(thePrefs.GetTempDir(i), ff.GetFileName()) == PLR_LOADSUCCESS) {
				toadd->SavePartFile(true); // re-save backup, don't overwrite existing bak files yet
				++count;
				filelist.AddTail(toadd);			// to download queue
				if (toadd->GetStatus(true) == PS_READY)
					theApp.sharedfiles->SafeAddKFile(toadd); // part files are always shared files
				theApp.emuledlg->transferwnd->GetDownloadList()->AddFile(toadd);// show in downloads window

				AddLogLine(false, GetResString(IDS_RECOVERED_PARTMET), (LPCTSTR)toadd->GetFileName());
			} else
				delete toadd;
		}
		ff.Close();
	}
	if (count == 0)
		AddLogLine(false, GetResString(IDS_NOPARTSFOUND));
	else {
		AddLogLine(false, GetResString(IDS_FOUNDPARTS), count);
		SortByPriority();
		CheckDiskspace();
	}
	VERIFY(m_srcwnd.CreateEx(0, AfxRegisterWndClass(0), _T("eMule Async DNS Resolve Socket Wnd #2"), WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL));

	ExportPartMetFilesOverview();
}

CDownloadQueue::~CDownloadQueue()
{
	while (!filelist.IsEmpty())
		delete filelist.RemoveHead();
	m_srcwnd.DestroyWindow(); // just to avoid a MFC warning
}

void CDownloadQueue::AddSearchToDownload(CSearchFile *toadd, uint8 paused, int cat)
{
	if (!(uint64)toadd->GetFileSize() || IsFileExisting(toadd->GetFileHash()))
		return;

	if (toadd->GetFileSize() > OLD_MAX_EMULE_FILE_SIZE && !thePrefs.CanFSHandleLargeFiles(cat)) {
		LogError(LOG_STATUSBAR, GetResString(IDS_ERR_FSCANTHANDLEFILE));
		return;
	}

	CPartFile *newfile = new CPartFile(toadd, cat);
	if (newfile->GetStatus() == PS_ERROR) {
		delete newfile;
		return;
	}

	if (paused == 2)
		paused = (uint8)thePrefs.AddNewFilesPaused();
	AddDownload(newfile, (paused == 1));

	// If the search result is from OP_GLOBSEARCHRES there may also be a source
	if (toadd->GetClientID() && toadd->GetClientPort()) {
		CSafeMemFile sources(1 + 4 + 2);
		try {
			sources.WriteUInt8(1);
			sources.WriteUInt32(toadd->GetClientID());
			sources.WriteUInt16(toadd->GetClientPort());
			sources.SeekToBegin();
			newfile->AddSources(&sources, toadd->GetClientServerIP(), toadd->GetClientServerPort(), false);
		} catch (CFileException *error) {
			ASSERT(0);
			error->Delete();
		}
	}

	// Add more sources which were found via global UDP search
	const CSimpleArray<CSearchFile::SClient> &aClients = toadd->GetClients();
	for (int i = 0; i < aClients.GetSize(); ++i) {
		CSafeMemFile sources(1 + 4 + 2);
		try {
			sources.WriteUInt8(1);
			sources.WriteUInt32(aClients[i].m_nIP);
			sources.WriteUInt16(aClients[i].m_nPort);
			sources.SeekToBegin();
			newfile->AddSources(&sources, aClients[i].m_nServerIP, aClients[i].m_nServerPort, false);
		} catch (CFileException *error) {
			ASSERT(0);
			error->Delete();
			break;
		}
	}
}

void CDownloadQueue::AddSearchToDownload(const CString &link, uint8 paused, int cat)
{
	CPartFile *newfile = new CPartFile(link, cat);
	if (newfile->GetStatus() == PS_ERROR) {
		delete newfile;
		return;
	}

	if (paused == 2)
		paused = (uint8)thePrefs.AddNewFilesPaused();
	AddDownload(newfile, (paused == 1));
}

void CDownloadQueue::StartNextFileIfPrefs(int cat)
{
	int i = thePrefs.StartNextFile();
	if (i)
		StartNextFile((i > 1 ? cat : -1), (i != 3));
}

void CDownloadQueue::StartNextFile(int cat, bool force)
{
	CPartFile *pfile = NULL;

	if (cat != -1) {
		// try to find in specified category
		for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
			CPartFile *cur_file = filelist.GetNext(pos);
			if (cur_file->GetStatus() == PS_PAUSED
				&& (cur_file->GetCategory() == (UINT)cat
					|| (!cat && !thePrefs.GetCategory(0)->filter && cur_file->GetCategory() > 0)
				   )
				&& CPartFile::RightFileHasHigherPrio(pfile, cur_file)
			   )
			{
				pfile = cur_file;
			}
		}
		if (pfile == NULL && !force)
			return;
	}

	if (cat == -1 || (pfile == NULL && force))
		for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
			CPartFile *cur_file = filelist.GetNext(pos);
			if (cur_file->GetStatus() == PS_PAUSED && CPartFile::RightFileHasHigherPrio(pfile, cur_file))
				// pick first found matching file, since they are sorted in prio order with most important file first.
				pfile = cur_file;
		}

	if (pfile)
		pfile->ResumeFile();
}

void CDownloadQueue::AddFileLinkToDownload(CED2KFileLink *pLink, int cat)
{
	CPartFile *newfile = new CPartFile(pLink, cat);
	if (newfile->GetStatus() == PS_ERROR) {
		delete newfile;
		newfile = NULL;
	} else
		AddDownload(newfile, thePrefs.AddNewFilesPaused());

	CPartFile *partfile = newfile;
	if (partfile == NULL)
		partfile = GetFileByID(pLink->GetHashKey());
	if (partfile) {
		// match the file identifier and only if they are the same add possible sources
		CFileIdentifierSA tmpFileIdent(pLink->GetHashKey(), pLink->GetSize(), pLink->GetAICHHash(), pLink->HasValidAICHHash());
		CFileIdentifier &fileid = partfile->GetFileIdentifier();
		if (fileid.CompareRelaxed(tmpFileIdent)) {
			if (pLink->HasValidSources())
				partfile->AddClientSources(pLink->SourcesList, 1, false);
			if (!fileid.HasAICHHash() && tmpFileIdent.HasAICHHash()) {
				fileid.SetAICHHash(tmpFileIdent.GetAICHHash());
				partfile->GetAICHRecoveryHashSet()->SetMasterHash(tmpFileIdent.GetAICHHash(), AICH_VERIFIED);
				partfile->GetAICHRecoveryHashSet()->FreeHashSet();

			}
		} else
			DebugLogWarning(_T("FileIdentifier mismatch when trying to add ed2k link to existing download - AICH Hash or Size might differ, no sources added. File: %s"),
			(LPCTSTR)partfile->GetFileName());
	}

	if (pLink->HasHostnameSources()) {
		for (POSITION pos = pLink->m_HostnameSourcesList.GetHeadPosition(); pos != NULL;) {
			const SUnresolvedHostname *pUnresHost = pLink->m_HostnameSourcesList.GetNext(pos);
			m_srcwnd.AddToResolve(pLink->GetHashKey(), pUnresHost->strHostname, pUnresHost->nPort, pUnresHost->strURL);
		}
	}
}

void CDownloadQueue::AddToResolved(CPartFile *pFile, SUnresolvedHostname *pUH)
{
	if (pFile && pUH)
		m_srcwnd.AddToResolve(pFile->GetFileHash(), pUH->strHostname, pUH->nPort, pUH->strURL);
}

void CDownloadQueue::AddDownload(CPartFile *newfile, bool paused)
{
	// Barry - Add in paused mode if required
	if (paused)
		newfile->PauseFile();

	SetAutoCat(newfile);// HoaX_69 / Slugfiller: AutoCat

	filelist.AddTail(newfile);
	SortByPriority();
	CheckDiskspace();
	theApp.emuledlg->transferwnd->GetDownloadList()->AddFile(newfile);
	AddLogLine(true, GetResString(IDS_NEWDOWNLOAD), (LPCTSTR)newfile->GetFileName());
	CString msgTemp;
	msgTemp.Format(GetResString(IDS_NEWDOWNLOAD) + _T('\n'), (LPCTSTR)newfile->GetFileName());
	theApp.emuledlg->ShowNotifier(msgTemp, TBN_DOWNLOADADDED);
	ExportPartMetFilesOverview();
}

bool CDownloadQueue::IsFileExisting(const uchar *fileid, bool bLogWarnings) const
{
	const CKnownFile *file = theApp.sharedfiles->GetFileByID(fileid);
	if (file) {
		if (bLogWarnings) {
			if (file->IsPartFile())
				LogWarning(LOG_STATUSBAR, GetResString(IDS_ERR_ALREADY_DOWNLOADING), (LPCTSTR)file->GetFileName());
			else
				LogWarning(LOG_STATUSBAR, GetResString(IDS_ERR_ALREADY_DOWNLOADED), (LPCTSTR)file->GetFileName());
		}
		return true;
	}
	file = GetFileByID(fileid);
	if (!file)
		return false;
	if (bLogWarnings)
		LogWarning(LOG_STATUSBAR, GetResString(IDS_ERR_ALREADY_DOWNLOADING), (LPCTSTR)file->GetFileName());
	return true;
}

void CDownloadQueue::Process()
{
	ProcessLocalRequests(); // send src requests to local server

	uint32 downspeed;
	uint64 maxDownload = thePrefs.GetMaxDownloadInBytesPerSec(true);
	if (maxDownload != UNLIMITED * 1024ull && datarate > 1500) {
		downspeed = (uint32)((maxDownload * 100) / (datarate + 1));
		if (downspeed < 50)
			downspeed = 50;
		else if (downspeed > 200)
			downspeed = 200;
	} else
		downspeed = 0;

	while (!average_dr_list.IsEmpty() && ::GetTickCount() >= average_dr_list.GetHead().timestamp + SEC2MS(10))
		m_datarateMS -= average_dr_list.RemoveHead().datalen;

	if (average_dr_list.GetCount() > 1)
		datarate = (uint32)(m_datarateMS / average_dr_list.GetCount());
	else
		datarate = 0;

	uint32 datarateX = 0;
	++udcounter;

	theStats.m_fGlobalDone = 0;
	theStats.m_fGlobalSize = 0;
	theStats.m_dwOverallStatus = 0;
	//file list is already sorted by prio, therefore I removed all the extra loops.
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);

		// maintain global download stats
		theStats.m_fGlobalDone += (uint64)cur_file->GetCompletedSize();
		theStats.m_fGlobalSize += (uint64)cur_file->GetFileSize();

		if (cur_file->GetTransferringSrcCount() > 0)
			theStats.m_dwOverallStatus |= STATE_DOWNLOADING;
		if (cur_file->GetStatus() == PS_ERROR)
			theStats.m_dwOverallStatus |= STATE_ERROROUS;


		if (cur_file->GetStatus() == PS_READY || cur_file->GetStatus() == PS_EMPTY)
			datarateX += cur_file->Process(downspeed, udcounter);
		else
			//This will make sure we don't keep old sources to paused and stopped files.
			cur_file->StopPausedFile();
	}

	average_dr_list.AddTail(TransferredData{datarateX, ::GetTickCount()});
	m_datarateMS += datarateX;

	if (udcounter == 5
		&& theApp.serverconnect->IsUDPSocketAvailable()
		&& (!lastudpstattime || ::GetTickCount() >= lastudpstattime + UDPSERVERSTATTIME))
	{
		lastudpstattime = ::GetTickCount();
		theApp.serverlist->ServerStats();
	}

	if (udcounter >= 10) {
		udcounter = 0;
		if (theApp.serverconnect->IsUDPSocketAvailable())
			if (!lastudpsearchtime || ::GetTickCount() >= lastudpsearchtime + UDPSERVERREASKTIME)
				SendNextUDPPacket();
	}

	CheckDiskspaceTimed();

	// ZZ:DownloadManager -->
	if (!m_dwLastA4AFtime || ::GetTickCount() >= m_dwLastA4AFtime + MIN2MS(8)) {
		theApp.clientlist->ProcessA4AFClients();
		m_dwLastA4AFtime = ::GetTickCount();
	}
	// <-- ZZ:DownloadManager
}

CPartFile* CDownloadQueue::GetFileNext(POSITION &pos) const
{
	if (!pos)
		pos = filelist.GetHeadPosition();
	return pos ? filelist.GetNext(pos) : NULL;
}

CPartFile* CDownloadQueue::GetFileByID(const uchar *filehash) const
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);
		if (md4equ(filehash, cur_file->GetFileHash()))
			return cur_file;
	}
	return NULL;
}

CPartFile* CDownloadQueue::GetFileByKadFileSearchID(uint32 id) const
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);
		if (id == cur_file->GetKadFileSearchID())
			return cur_file;
	}
	return NULL;
}

bool CDownloadQueue::IsPartFile(const CKnownFile *file) const
{
	return filelist.Find((void*)file) != NULL;
}

bool CDownloadQueue::CheckAndAddSource(CPartFile *sender, CUpDownClient *source)
{
	if (sender->IsStopped()) {
		delete source;
		return false;
	}

	if (source->HasValidHash()) {
		if (md4equ(source->GetUserHash(), thePrefs.GetUserHash())) {
			if (thePrefs.GetVerbose())
				AddDebugLogLine(false, _T("Tried to add source with matching hash to your own."));
			delete source;
			return false;
		}
	}
	// filter sources which are known to be temporarily dead/useless
	if (theApp.clientlist->m_globDeadSourceList.IsDeadSource(source) || sender->m_DeadSourceList.IsDeadSource(source)) {
		//if (thePrefs.GetLogFilteredIPs())
		//	AddDebugLogLine(DLP_DEFAULT, false, _T("Rejected source because it was found on the DeadSourcesList (%s) for file %s : %s")
		//	,sender->m_DeadSourceList.IsDeadSource(source)? _T("Local") : _T("Global"), (LPCTSTR)sender->GetFileName(), (LPCTSTR)source->DbgGetClientInfo() );
		delete source;
		return false;
	}

	// filter sources which are incompatible with our encryption setting (one requires it, and the other one doesn't supports it)
	if ((source->RequiresCryptLayer() && (!thePrefs.IsClientCryptLayerSupported() || !source->HasValidHash())) || (thePrefs.IsClientCryptLayerRequired() && (!source->SupportsCryptLayer() || !source->HasValidHash()))) {
#if defined(_DEBUG) || defined(_BETA) || defined(_DEVBUILD)
		//if (thePrefs.GetDebugSourceExchange()) // TODO: Uncomment after testing
		AddDebugLogLine(DLP_DEFAULT, false, _T("Rejected source because CryptLayer-Setting (Obfuscation) was incompatible for file %s : %s"), (LPCTSTR)sender->GetFileName(), (LPCTSTR)source->DbgGetClientInfo());
#endif
		delete source;
		return false;
	}

	// "Filter LAN IPs" and/or "IPfilter" is not required here, because it was already done in parent functions

	// uses this only for temp. clients
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		const CPartFile *cur_file = filelist.GetNext(pos);
		for (POSITION pos2 = cur_file->srclist.GetHeadPosition(); pos2 != NULL;) {
			CUpDownClient *cur_client = cur_file->srclist.GetNext(pos2);
			if (cur_client->Compare(source, true) || cur_client->Compare(source, false)) {
				// if this file has not this source already, set request for this source
				if (cur_file != sender && cur_client->AddRequestForAnotherFile(sender)) {
					theApp.emuledlg->transferwnd->GetDownloadList()->AddSource(sender, cur_client, true);
					if (cur_client->GetDownloadState() != DS_CONNECTED)
						cur_client->SwapToAnotherFile(_T("New A4AF source found. CDownloadQueue::CheckAndAddSource()"), false, false, false, NULL, true, false); // ZZ:DownloadManager
				}
				delete source;
				return false;
			}
		}
	}
	//our new source is real new but maybe it is already uploading to us?
	//if yes the known client will be attached to the var "source"
	//and the old source client will be deleted
	if (theApp.clientlist->AttachToAlreadyKnown(&source, NULL)) {
#ifdef _DEBUG
		const CPartFile *srcfile = source->GetRequestFile();
		if (thePrefs.GetVerbose() && srcfile) {
			// if a client sent us wrong sources (sources for some other file for which we asked but which we are also
			// downloading) we may get a little in trouble here when "moving" this source to some other partfile without
			// further checks and updates.
			if (!md4equ(srcfile->GetFileHash(), sender->GetFileHash()))
				AddDebugLogLine(false, _T("*** CDownloadQueue::CheckAndAddSource -- added potential wrong source (%u)(diff. filehash) to file \"%s\""), source->GetUserIDHybrid(), (LPCTSTR)sender->GetFileName());
			if (srcfile->GetPartCount() != 0 && srcfile->GetPartCount() != sender->GetPartCount())
				AddDebugLogLine(false, _T("*** CDownloadQueue::CheckAndAddSource -- added potential wrong source (%u)(diff. partcount) to file \"%s\""), source->GetUserIDHybrid(), (LPCTSTR)sender->GetFileName());
		}
#endif
		source->SetRequestFile(sender);
	} else {
		// here we know that the client instance 'source' is a new created client instance (see callers)
		// which is therefore not already in the client list, we can avoid the check for duplicate
		// client list entries when adding this client
		theApp.clientlist->AddClient(source, true);
	}

#ifdef _DEBUG
	if (thePrefs.GetVerbose() && source->GetPartCount() != 0 && source->GetPartCount() != sender->GetPartCount())
		DEBUG_ONLY(AddDebugLogLine(false, _T("*** CDownloadQueue::CheckAndAddSource -- New added source (%u, %s) had still value in partcount"), source->GetUserIDHybrid(), (LPCTSTR)sender->GetFileName()));
#endif

	sender->srclist.AddTail(source);
	theApp.emuledlg->transferwnd->GetDownloadList()->AddSource(sender, source, false);
	return true;
}

bool CDownloadQueue::CheckAndAddKnownSource(CPartFile *sender, CUpDownClient *source, bool bIgnoreGlobDeadList)
{
	if (sender->IsStopped())
		return false;

	// filter sources which are known to be temporarily dead/useless
	if ((theApp.clientlist->m_globDeadSourceList.IsDeadSource(source) && !bIgnoreGlobDeadList) || sender->m_DeadSourceList.IsDeadSource(source)) {
		//if (thePrefs.GetLogFilteredIPs())
		//	AddDebugLogLine(DLP_DEFAULT, false, _T("Rejected source because it was found on the DeadSourcesList (%s) for file %s : %s")
		//	,sender->m_DeadSourceList.IsDeadSource(source)? _T("Local") : _T("Global"), (LPCTSTR)sender->GetFileName(), (LPCTSTR)source->DbgGetClientInfo());
		return false;
	}

	// filter sources which are incompatible with our encryption setting (one requires it, and the other one doesn't supports it)
	if ((source->RequiresCryptLayer() && (!thePrefs.IsClientCryptLayerSupported() || !source->HasValidHash())) || (thePrefs.IsClientCryptLayerRequired() && (!source->SupportsCryptLayer() || !source->HasValidHash()))) {
#if defined(_DEBUG) || defined(_BETA) || defined(_DEVBUILD)
		//if (thePrefs.GetDebugSourceExchange()) // TODO: Uncomment after testing
		AddDebugLogLine(DLP_DEFAULT, false, _T("Rejected source because CryptLayer-Setting (Obfuscation) was incompatible for file %s : %s"), (LPCTSTR)sender->GetFileName(), (LPCTSTR)source->DbgGetClientInfo());
#endif
		return false;
	}

	// "Filter LAN IPs" -- this may be needed here in case we are connected to the internet and are also connected
	// to a LAN and some client from within the LAN connected to us. Though this situation may be supported in future
	// by adding that client to the source list and filtering that client's LAN IP when sending sources to
	// a client within the internet.
	//
	// "IPfilter" is not needed here, because that "known" client was already IPfiltered when receiving OP_HELLO.
	if (!source->HasLowID()) {
		uint32 nClientIP = htonl(source->GetUserIDHybrid());
		if (!IsGoodIP(nClientIP)) { // check for 0-IP, localhost and LAN addresses
			//if (thePrefs.GetLogFilteredIPs())
			//	AddDebugLogLine(false, _T("Ignored already known source with IP=%s"), (LPCTSTR)ipstr(nClientIP));
			return false;
		}
	}

	// use this for client which are already know (downloading for example)
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		const CPartFile *cur_file = filelist.GetNext(pos);
		if (cur_file->srclist.Find(source)) {
			if (cur_file == sender)
				return false;
			if (source->AddRequestForAnotherFile(sender))
				theApp.emuledlg->transferwnd->GetDownloadList()->AddSource(sender, source, true);
			if (source->GetDownloadState() != DS_CONNECTED)
				source->SwapToAnotherFile(_T("New A4AF source found. CDownloadQueue::CheckAndAddKnownSource()"), false, false, false, NULL, true, false); // ZZ:DownloadManager

			return false;
		}
	}
#ifdef _DEBUG
	const CPartFile *srcfile = source->GetRequestFile();
	if (thePrefs.GetVerbose() && srcfile) {
		// if a client sent us wrong sources (sources for some other file for which we asked but which we are also
		// downloading) we may get a little in trouble here when "moving" this source to some other partfile without
		// further checks and updates.
		if (!md4equ(srcfile->GetFileHash(), sender->GetFileHash()))
			AddDebugLogLine(false, _T("*** CDownloadQueue::CheckAndAddKnownSource -- added potential wrong source (%u)(diff. filehash) to file \"%s\""), source->GetUserIDHybrid(), (LPCTSTR)sender->GetFileName());
		if (srcfile->GetPartCount() != 0 && srcfile->GetPartCount() != sender->GetPartCount())
			AddDebugLogLine(false, _T("*** CDownloadQueue::CheckAndAddKnownSource -- added potential wrong source (%u)(diff. partcount) to file \"%s\""), source->GetUserIDHybrid(), (LPCTSTR)sender->GetFileName());
	}
#endif
	source->SetRequestFile(sender);
	sender->srclist.AddTail(source);
	source->SetSourceFrom(SF_PASSIVE);
	if (thePrefs.GetDebugSourceExchange())
		AddDebugLogLine(false, _T("SXRecv: Passively added source; %s, File=\"%s\""), (LPCTSTR)source->DbgGetClientInfo(), (LPCTSTR)sender->GetFileName());
#ifdef _DEBUG
	if (thePrefs.GetVerbose() && source->GetPartCount() != 0 && source->GetPartCount() != sender->GetPartCount())
		DEBUG_ONLY(AddDebugLogLine(false, _T("*** CDownloadQueue::CheckAndAddKnownSource -- New added source (%u, %s) had still value in partcount"), source->GetUserIDHybrid(), (LPCTSTR)sender->GetFileName()));
#endif

	theApp.emuledlg->transferwnd->GetDownloadList()->AddSource(sender, source, false);
	//UpdateDisplayedInfo();
	return true;
}

bool CDownloadQueue::RemoveSource(CUpDownClient *toremove, bool bDoStatsUpdate)
{
	bool bRemovedSrcFromPartFile = false;
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);
		POSITION pos2 = cur_file->srclist.Find(toremove);
		if (pos2) {
			cur_file->srclist.RemoveAt(pos2);
			bRemovedSrcFromPartFile = true;
			if (bDoStatsUpdate) {
				cur_file->RemoveDownloadingSource(toremove);
				cur_file->UpdatePartsInfo();
			}
		}
		if (bDoStatsUpdate)
			cur_file->UpdateAvailablePartsCount();
	}

	// remove this source on all files in the download queue who link this source
	// pretty slow but no way around, maybe using a Map is better, but that's slower on other parts
	for (POSITION pos = toremove->m_OtherRequests_list.GetHeadPosition(); pos != NULL;) {
		const POSITION pos1 = pos;
		CPartFile *pfile = toremove->m_OtherRequests_list.GetNext(pos);
		POSITION pos2 = pfile->A4AFsrclist.Find(toremove);
		if (pos2) {
			pfile->A4AFsrclist.RemoveAt(pos2);
			theApp.emuledlg->transferwnd->GetDownloadList()->RemoveSource(toremove, pfile);
			toremove->m_OtherRequests_list.RemoveAt(pos1);
		}
	}
	for (POSITION pos = toremove->m_OtherNoNeeded_list.GetHeadPosition(); pos != NULL;) {
		const POSITION pos1 = pos;
		CPartFile *pfile = toremove->m_OtherNoNeeded_list.GetNext(pos);
		POSITION pos2 = pfile->A4AFsrclist.Find(toremove);
		if (pos2) {
			pfile->A4AFsrclist.RemoveAt(pos2);
			theApp.emuledlg->transferwnd->GetDownloadList()->RemoveSource(toremove, pfile);
			toremove->m_OtherNoNeeded_list.RemoveAt(pos1);
		}
	}

	if (bRemovedSrcFromPartFile && (toremove->HasFileRating() || !toremove->GetFileComment().IsEmpty())) {
		CPartFile *pFile = toremove->GetRequestFile();
		if (pFile)
			pFile->UpdateFileRatingCommentAvail();
	}

	toremove->SetDownloadState(DS_NONE);
	theApp.emuledlg->transferwnd->GetDownloadList()->RemoveSource(toremove, 0);
	toremove->SetRequestFile(NULL);
	return bRemovedSrcFromPartFile;
}

void CDownloadQueue::RemoveFile(CPartFile *toremove)
{
	RemoveLocalServerRequest(toremove);

	POSITION pos = filelist.Find(toremove);
	if (pos != NULL)
		filelist.RemoveAt(pos);
	SortByPriority();
	CheckDiskspace();
	ExportPartMetFilesOverview();
}

void CDownloadQueue::DeleteAll()
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);
		cur_file->srclist.RemoveAll();
		// Barry - Should also remove all requested blocks
		// Don't worry about deleting the blocks, that gets handled
		// when CUpDownClient is deleted in CClientList::DeleteAll()
		cur_file->RemoveAllRequestedBlocks();
	}
}

// Max. file IDs per UDP packet
// ----------------------------
// 576 - 30 bytes of header (28 for UDP, 2 for "E3 9A" edonkey proto) = 546 bytes
// 546 / 16 = 34
#define MAX_UDP_PACKET_DATA				510
#define BYTES_PER_FILE_G1				16
#define BYTES_PER_FILE_G2				20
#define ADDITIONAL_BYTES_PER_LARGEFILE	8

#define MAX_REQUESTS_PER_SERVER		35

bool CDownloadQueue::IsMaxFilesPerUDPServerPacketReached(uint32 nFiles, uint32 nIncludedLargeFiles) const
{
	if (cur_udpserver && cur_udpserver->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES) {

		const int nBytesPerNormalFile = ((cur_udpserver->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES2) > 0) ? BYTES_PER_FILE_G2 : BYTES_PER_FILE_G1;
		const int nUsedBytes = nFiles * nBytesPerNormalFile + nIncludedLargeFiles * ADDITIONAL_BYTES_PER_LARGEFILE;
		if (nIncludedLargeFiles > 0) {
			ASSERT(cur_udpserver->SupportsLargeFilesUDP());
			ASSERT(cur_udpserver->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES2);
		}
		return (m_cRequestsSentToServer >= MAX_REQUESTS_PER_SERVER) || (nUsedBytes >= MAX_UDP_PACKET_DATA);
	}
	ASSERT(nIncludedLargeFiles == 0);
	return nFiles != 0;
}

bool CDownloadQueue::SendGlobGetSourcesUDPPacket(CSafeMemFile *data, bool bExt2Packet, uint32 nFiles, uint32 nIncludedLargeFiles)
{
	bool bSentPacket = false;

	if (cur_udpserver) {
#ifdef _DEBUG
		int iPacketSize = (int)data->GetLength();
#endif
		Packet packet(data);
		if (bExt2Packet) {
			ASSERT(iPacketSize > 0 && (uint32)iPacketSize == nFiles * 20 + nIncludedLargeFiles * 8);
			packet.opcode = OP_GLOBGETSOURCES2;
		} else {
			ASSERT(iPacketSize > 0 && (uint32)iPacketSize == nFiles * 16 && nIncludedLargeFiles == 0);
			packet.opcode = OP_GLOBGETSOURCES;
		}
		if (thePrefs.GetDebugServerUDPLevel() > 0)
			Debug(_T(">>> Sending %s to server %-21s (%3i of %3u); FileIDs=%u(%u large)\n")
				, (packet.opcode == OP_GLOBGETSOURCES2) ? _T("OP_GlobGetSources2") : _T("OP_GlobGetSources1")
				, (LPCTSTR)ipstr(cur_udpserver->GetAddress(), cur_udpserver->GetPort())
				, m_iSearchedServers + 1
				, (unsigned)theApp.serverlist->GetServerCount()
				, nFiles
				, nIncludedLargeFiles);

		theStats.AddUpDataOverheadServer(packet.size);
		theApp.serverconnect->SendUDPPacket(&packet, cur_udpserver, false);

		m_cRequestsSentToServer += nFiles;
		bSentPacket = true;
	}

	return bSentPacket;
}

bool CDownloadQueue::SendNextUDPPacket()
{
	if (filelist.IsEmpty()
		|| !theApp.serverconnect->IsUDPSocketAvailable()
		|| !theApp.serverconnect->IsConnected()
		|| thePrefs.IsClientCryptLayerRequired()) // we cannot use sources received without user hash, so don't ask
	{
		return false;
	}
	CServer *pConnectedServer = theApp.serverconnect->GetCurrentServer();
	if (pConnectedServer)
		pConnectedServer = theApp.serverlist->GetServerByAddress(pConnectedServer->GetAddress(), pConnectedServer->GetPort());

	if (!cur_udpserver) {
		m_cRequestsSentToServer = 0;
		do {
			cur_udpserver = theApp.serverlist->GetSuccServer(cur_udpserver);
			if (cur_udpserver == NULL) {
				StopUDPRequests();
				return false;
			}
		} while (cur_udpserver == pConnectedServer || cur_udpserver->GetFailedCount() >= thePrefs.GetDeadServerRetries());
	}

	bool bGetSources2Packet = (cur_udpserver->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES2) > 0;
	bool bServerSupportsLargeFiles = cur_udpserver->SupportsLargeFilesUDP();

	// loop until the packet is filled, or a packet was sent
	bool bSentPacket = false;
	CSafeMemFile dataGlobGetSources(20);
	int iFiles = 0;
	int iLargeFiles = 0;
	while (!IsMaxFilesPerUDPServerPacketReached(iFiles, iLargeFiles) && !bSentPacket) {
		// get the next file to search sources for
		CPartFile *nextfile = NULL;
		while (!bSentPacket && !(nextfile && (nextfile->GetStatus() == PS_READY || nextfile->GetStatus() == PS_EMPTY))) {
			if (lastfile == NULL) // we just started the global source searching or have switched the server
				// get the first file to search sources for
				nextfile = filelist.GetHead();
			else {
				POSITION pos = filelist.Find(lastfile);
				if (pos == NULL) // the last file is no longer in the DL-list (may have been finished or cancelled)
					// get the first file to search sources for
					nextfile = filelist.GetHead();
				else {
					filelist.GetNext(pos);
					if (pos == 0) { // finished asking the current server for all files
						// if there are pending requests for the current server, send them
						if (dataGlobGetSources.GetLength() > 0) {
							if (SendGlobGetSourcesUDPPacket(&dataGlobGetSources, bGetSources2Packet, iFiles, iLargeFiles))
								bSentPacket = true;
							dataGlobGetSources.SetLength(0);
							iFiles = 0;
							iLargeFiles = 0;
						}

						m_cRequestsSentToServer = 0;
						// get next server to ask
						do {
							cur_udpserver = theApp.serverlist->GetSuccServer(cur_udpserver);
							if (cur_udpserver == NULL) {
								// finished asking all servers for all files
								if (thePrefs.GetDebugServerUDPLevel() > 0 && thePrefs.GetDebugServerSourcesLevel() > 0)
									Debug(_T("Finished UDP search processing for all servers (%u)\n"), (unsigned)theApp.serverlist->GetServerCount());
								StopUDPRequests();
								return false; // finished (processed all file & all servers)
							}
						} while (cur_udpserver == pConnectedServer || cur_udpserver->GetFailedCount() >= thePrefs.GetDeadServerRetries());
						++m_iSearchedServers;

						// if we already sent a packet, switch to the next file at next function call
						if (bSentPacket) {
							lastfile = NULL;
							break;
						}

						bGetSources2Packet = (cur_udpserver->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES2) > 0;
						bServerSupportsLargeFiles = cur_udpserver->SupportsLargeFilesUDP();

						// have selected a new server; get the first file to search sources for
						nextfile = filelist.GetHead();
					} else
						nextfile = filelist.GetAt(pos);
				}
			}
			lastfile = nextfile;
		}

		if (!bSentPacket && nextfile && nextfile->GetSourceCount() < nextfile->GetMaxSourcePerFileUDP() && (bServerSupportsLargeFiles || !nextfile->IsLargeFile())) {
			// GETSOURCES Packet (<HASH_16> *)
			dataGlobGetSources.WriteHash16(nextfile->GetFileHash());
			if (bGetSources2Packet)
				if (nextfile->IsLargeFile()) {
					// GETSOURCES2 Packet Large File (<HASH_16><IND_4 = 0><SIZE_8> *)
					++iLargeFiles;
					dataGlobGetSources.WriteUInt32(0);
					dataGlobGetSources.WriteUInt64(nextfile->GetFileSize());
				} else {
					// GETSOURCES2 Packet (<HASH_16><SIZE_4> *)
					dataGlobGetSources.WriteUInt32((uint32)(uint64)nextfile->GetFileSize());
				}

			++iFiles;
			if (thePrefs.GetDebugServerUDPLevel() > 0 && thePrefs.GetDebugServerSourcesLevel() > 0)
				Debug(_T(">>> Queued  %s to server %-21s (%3i of %3u); Buff  %u(%u)=%s\n"), bGetSources2Packet ? _T("OP_GlobGetSources2") : _T("OP_GlobGetSources1"), (LPCTSTR)ipstr(cur_udpserver->GetAddress(), cur_udpserver->GetPort()), m_iSearchedServers + 1, (unsigned)theApp.serverlist->GetServerCount(), iFiles, iLargeFiles, (LPCTSTR)DbgGetFileInfo(nextfile->GetFileHash()));
		}
	}

	ASSERT(dataGlobGetSources.GetLength() == 0 || !bSentPacket);

	if (!bSentPacket && dataGlobGetSources.GetLength() > 0)
		SendGlobGetSourcesUDPPacket(&dataGlobGetSources, bGetSources2Packet, iFiles, iLargeFiles);

	// send max 35 UDP request to one server per interval
	// if we have more than 35 files, we rotate the list and use it as a queue
	if (m_cRequestsSentToServer >= MAX_REQUESTS_PER_SERVER) {
		if (thePrefs.GetDebugServerUDPLevel() > 0 && thePrefs.GetDebugServerSourcesLevel() > 0)
			Debug(_T("Rotating file list\n"));

		// move the last 35 files to the head
		if (filelist.GetCount() >= MAX_REQUESTS_PER_SERVER)
			for (int i = MAX_REQUESTS_PER_SERVER; --i >= 0;)
				filelist.AddHead(filelist.RemoveTail());

		m_cRequestsSentToServer = 0;
		// and next server
		do {
			cur_udpserver = theApp.serverlist->GetSuccServer(cur_udpserver);
			if (cur_udpserver == NULL) {
				if (thePrefs.GetDebugServerUDPLevel() > 0 && thePrefs.GetDebugServerSourcesLevel() > 0)
					Debug(_T("Finished UDP search processing for all servers (%u)\n"), (unsigned)theApp.serverlist->GetServerCount());
				StopUDPRequests();
				return false; // finished (processed all file & all servers)
			}
		} while (cur_udpserver == pConnectedServer || cur_udpserver->GetFailedCount() >= thePrefs.GetDeadServerRetries());
		++m_iSearchedServers;
		lastfile = NULL;
	}

	return true;
}

void CDownloadQueue::StopUDPRequests()
{
	cur_udpserver = NULL;
	lastudpsearchtime = ::GetTickCount();
	lastfile = NULL;
	m_iSearchedServers = 0;
}

bool CDownloadQueue::CompareParts(POSITION pos1, POSITION pos2)
{
	CPartFile *file1 = filelist.GetAt(pos1);
	CPartFile *file2 = filelist.GetAt(pos2);
	return CPartFile::RightFileHasHigherPrio(file1, file2);
}

void CDownloadQueue::SwapParts(POSITION pos1, POSITION pos2)
{
	CPartFile *file1 = filelist.GetAt(pos1);
	CPartFile *file2 = filelist.GetAt(pos2);
	filelist.SetAt(pos1, file2);
	filelist.SetAt(pos2, file1);
}

void CDownloadQueue::HeapSort(UINT first, UINT last)
{
	UINT r;
	POSITION pos1 = filelist.FindIndex(first);
	for (r = first; !(r & (UINT)INT_MIN) && (r << 1) < last; ) {
		UINT r2 = (r << 1) + 1;
		POSITION pos2 = filelist.FindIndex(r2);
		if (r2 != last) {
			POSITION pos3 = pos2;
			filelist.GetNext(pos3);
			if (!CompareParts(pos2, pos3)) {
				pos2 = pos3;
				++r2;
			}
		}
		if (CompareParts(pos1, pos2))
			break;
		SwapParts(pos1, pos2);
		r = r2;
		pos1 = pos2;
	}
}

void CDownloadQueue::SortByPriority()
{
	UINT n = (UINT)filelist.GetCount();
	if (!n)
		return;
	for (UINT i = n / 2; i--;)
		HeapSort(i, n - 1);
	for (UINT i = n; --i;) {
		SwapParts(filelist.FindIndex(0), filelist.FindIndex(i));
		HeapSort(0, i - 1);
	}
}

void CDownloadQueue::CheckDiskspaceTimed()
{
	if (!lastcheckdiskspacetime || ::GetTickCount() >= lastcheckdiskspacetime + DISKSPACERECHECKTIME)
		CheckDiskspace();
}

void CDownloadQueue::CheckDiskspace(bool bNotEnoughSpaceLeft)
{
	lastcheckdiskspacetime = ::GetTickCount();

	// sorting the list could be done here, but I prefer to "see" that function call in the calling functions.
	//SortByPriority();

	// If disabled, resume any previously paused files
	if (!thePrefs.IsCheckDiskspaceEnabled()) {
		if (!bNotEnoughSpaceLeft) // avoid worse case, if we already had 'disk full'
			for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
				CPartFile *cur_file = filelist.GetNext(pos);
				switch (cur_file->GetStatus()) {
				case PS_PAUSED:
				case PS_ERROR:
				case PS_COMPLETING:
				case PS_COMPLETE:
					continue;
				}
				cur_file->ResumeFileInsufficient();
			}
		return;
	}

	uint64 nTotalAvailableSpaceMain = bNotEnoughSpaceLeft ? 0 : GetFreeDiskSpaceX(thePrefs.GetTempDir());

	// 'bNotEnoughSpaceLeft' - avoid worse case of having already 'disk full'
	if (thePrefs.GetMinFreeDiskSpace() == 0) {
		for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
			CPartFile *cur_file = filelist.GetNext(pos);

			uint64 nTotalAvailableSpace = bNotEnoughSpaceLeft ? 0 :
				((thePrefs.GetTempDirCount() == 1) ? nTotalAvailableSpaceMain : GetFreeDiskSpaceX(cur_file->GetTempPath()));

			switch (cur_file->GetStatus()) {
			case PS_PAUSED:
			case PS_ERROR:
			case PS_COMPLETING:
			case PS_COMPLETE:
				continue;
			}

			// Pause the file only if it would grow in size and would exceed the currently available free space
			uint64 nSpaceToGo = cur_file->GetNeededSpace();
			if (nSpaceToGo <= nTotalAvailableSpace) {
				nTotalAvailableSpace -= nSpaceToGo;
				cur_file->ResumeFileInsufficient();
			} else
				cur_file->PauseFile(true);
		}
	} else {
		for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
			CPartFile *cur_file = filelist.GetNext(pos);
			switch (cur_file->GetStatus()) {
			case PS_PAUSED:
			case PS_ERROR:
			case PS_COMPLETING:
			case PS_COMPLETE:
				continue;
			}

			uint64 nTotalAvailableSpace = bNotEnoughSpaceLeft ? 0 :
				((thePrefs.GetTempDirCount() == 1) ? nTotalAvailableSpaceMain : GetFreeDiskSpaceX(cur_file->GetTempPath()));
			if (nTotalAvailableSpace < thePrefs.GetMinFreeDiskSpace()) {
				// Compressed/sparse files: always pause the file
				// Normal files: pause the file only if it would still grow
				if (!cur_file->IsNormalFile() || cur_file->GetNeededSpace() > 0)
					cur_file->PauseFile(true);
			} else {
				// doesn't work this way. resuming the file without checking if there is a chance to successfully
				// flush any available buffered file data will pause the file right after it was resumed and disturb
				// the StopPausedFile function.
				//cur_file->ResumeFileInsufficient();
			}
		}
	}
}

void CDownloadQueue::GetDownloadSourcesStats(SDownloadStats &results)
{
	memset(&results, 0, sizeof results);
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		const CPartFile *cur_file = filelist.GetNext(pos);

		results.a[0] += cur_file->GetSourceCount();
		results.a[1] += cur_file->GetTransferringSrcCount();
		results.a[2] += cur_file->GetSrcStatisticsValue(DS_ONQUEUE);
		results.a[3] += cur_file->GetSrcStatisticsValue(DS_REMOTEQUEUEFULL);
		results.a[4] += cur_file->GetSrcStatisticsValue(DS_NONEEDEDPARTS);
		results.a[5] += cur_file->GetSrcStatisticsValue(DS_CONNECTED);
		results.a[6] += cur_file->GetSrcStatisticsValue(DS_REQHASHSET);
		results.a[7] += cur_file->GetSrcStatisticsValue(DS_CONNECTING);
		results.a[8] += cur_file->GetSrcStatisticsValue(DS_WAITCALLBACK);
		results.a[8] += cur_file->GetSrcStatisticsValue(DS_WAITCALLBACKKAD);
		results.a[9] += cur_file->GetSrcStatisticsValue(DS_TOOMANYCONNS);
		results.a[9] += cur_file->GetSrcStatisticsValue(DS_TOOMANYCONNSKAD);
		results.a[10] += cur_file->GetSrcStatisticsValue(DS_LOWTOLOWIP);
		results.a[11] += cur_file->GetSrcStatisticsValue(DS_NONE);
		results.a[12] += cur_file->GetSrcStatisticsValue(DS_ERROR);
		results.a[13] += cur_file->GetSrcStatisticsValue(DS_BANNED);
		results.a[14] += cur_file->src_stats[3];
		results.a[15] += cur_file->GetSrcA4AFCount();
		results.a[16] += cur_file->src_stats[0];
		results.a[17] += cur_file->src_stats[1];
		results.a[18] += cur_file->src_stats[2];
		results.a[19] += cur_file->net_stats[0];
		results.a[20] += cur_file->net_stats[1];
		results.a[21] += cur_file->net_stats[2];
		results.a[22] += static_cast<unsigned>(cur_file->m_DeadSourceList.GetDeadSourcesCount());
	}
}

CUpDownClient* CDownloadQueue::GetDownloadClientByIP(uint32 dwIP)
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		const CPartFile *cur_file = filelist.GetNext(pos);
		for (POSITION pos2 = cur_file->srclist.GetHeadPosition(); pos2 != NULL;) {
			CUpDownClient *cur_client = cur_file->srclist.GetNext(pos2);
			if (dwIP == cur_client->GetIP())
				return cur_client;
		}
	}
	return NULL;
}

CUpDownClient* CDownloadQueue::GetDownloadClientByIP_UDP(uint32 dwIP, uint16 nUDPPort, bool bIgnorePortOnUniqueIP, bool *pbMultipleIPs)
{
	CUpDownClient *pMatchingIPClient = NULL;
	uint32 cMatches = 0;

	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		const CPartFile *cur_file = filelist.GetNext(pos);
		for (POSITION pos2 = cur_file->srclist.GetHeadPosition(); pos2 != NULL;) {
			CUpDownClient *cur_client = cur_file->srclist.GetNext(pos2);
			if (dwIP == cur_client->GetIP() && nUDPPort == cur_client->GetUDPPort())
				return cur_client;
			if (dwIP == cur_client->GetIP() && bIgnorePortOnUniqueIP && cur_client != pMatchingIPClient) {
				pMatchingIPClient = cur_client;
				++cMatches;
			}
		}
	}
	if (pbMultipleIPs != NULL)
		*pbMultipleIPs = cMatches > 1;

	if (pMatchingIPClient != NULL && cMatches == 1)
		return pMatchingIPClient;
	return NULL;
}

bool CDownloadQueue::IsInList(const CUpDownClient *client) const
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;)
		if (filelist.GetNext(pos)->srclist.Find(const_cast<CUpDownClient*>(client)))
			return true;
	return false;
}

void CDownloadQueue::ResetCatParts(UINT cat)
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);

		if (cur_file->GetCategory() == cat)
			cur_file->SetCategory(0);
		else if (cur_file->GetCategory() > cat)
			cur_file->SetCategory(cur_file->GetCategory() - 1);
	}
}

void CDownloadQueue::SetCatPrio(UINT cat, uint8 newprio)
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);
		if (cat == 0 || cur_file->GetCategory() == cat)
			if (newprio == PR_AUTO) {
				cur_file->SetAutoDownPriority(true);
				cur_file->SetDownPriority(PR_HIGH, false);
			} else {
				cur_file->SetAutoDownPriority(false);
				cur_file->SetDownPriority(newprio, false);
			}
	}

	theApp.downloadqueue->SortByPriority();
	theApp.downloadqueue->CheckDiskspaceTimed();
}

// ZZ:DownloadManager -->
void CDownloadQueue::RemoveAutoPrioInCat(UINT cat, uint8 newprio)
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);
		if (cur_file->IsAutoDownPriority() && (cat == 0 || cur_file->GetCategory() == cat)) {
			cur_file->SetAutoDownPriority(false);
			cur_file->SetDownPriority(newprio, false);
		}
	}

	theApp.downloadqueue->SortByPriority();
	theApp.downloadqueue->CheckDiskspaceTimed();
}
// <-- ZZ:DownloadManager

void CDownloadQueue::SetCatStatus(UINT cat, int newstatus)
{
	bool reset = false;
	bool resort = false;

	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);
		if (!cur_file)
			continue;

		if (cat == (UINT)-1
			|| (cat == (UINT)-2 && cur_file->GetCategory() == 0)
			|| (cat == 0 && cur_file->CheckShowItemInGivenCat(cat))
			|| (cat > 0 && cat == cur_file->GetCategory()))
		{
			switch (newstatus) {
			case MP_CANCEL:
				cur_file->DeletePartFile();
				reset = true;
				break;
			case MP_PAUSE:
				cur_file->PauseFile(false, false);
				resort = true;
				break;
			case MP_STOP:
				cur_file->StopFile(false, false);
				resort = true;
				break;
			case MP_RESUME:
				if (cur_file->CanResumeFile()) {
					if (cur_file->GetStatus() == PS_INSUFFICIENT)
						cur_file->ResumeFileInsufficient();
					else {
						cur_file->ResumeFile(false);
						resort = true;
					}
				}
			}
		}
		if (reset) {
			reset = false;
			pos = filelist.GetHeadPosition();
		}
	}

	if (resort) {
		theApp.downloadqueue->SortByPriority();
		theApp.downloadqueue->CheckDiskspace();
	}
}

void CDownloadQueue::MoveCat(UINT from, UINT to)
{
	to -= static_cast<UINT>(from < to);
	const UINT cmin = min(from, to);
	const UINT cmax = max(from, to);
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *cur_file = filelist.GetNext(pos);
		if (cur_file) {
			UINT mycat = cur_file->GetCategory();
			if (mycat >= cmin && mycat <= cmax)
				if (mycat == from)
					mycat = to;
				else
					mycat += (from < to ? -1 : 1);
			cur_file->SetCategory(mycat);
		}
	}
}

UINT CDownloadQueue::GetDownloadingFileCount() const
{
	UINT result = 0;
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		const EPartFileStatus uStatus = filelist.GetNext(pos)->GetStatus();
		if (uStatus == PS_READY || uStatus == PS_EMPTY)
			++result;
	}
	return result;
}

UINT CDownloadQueue::GetPausedFileCount() const
{
	UINT result = 0;
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;)
		if (filelist.GetNext(pos)->GetStatus() == PS_PAUSED)
			++result;
	return result;
}

void CDownloadQueue::SetAutoCat(CPartFile *newfile)
{
	if (thePrefs.GetCatCount() < 2 || newfile->GetCategory() > 0)
		return;

	bool bFound = false;
	for (int i = (int)thePrefs.GetCatCount(); --i > 0;) {
		CString catExt(thePrefs.GetCategory(i)->autocat);
		if (catExt.IsEmpty())
			continue;

		if (thePrefs.GetCategory(i)->ac_regexpeval)
			bFound = RegularExpressionMatch(catExt, newfile->GetFileName()); // regular expression evaluation
		else {
			CString fullname(newfile->GetFileName());
			fullname.MakeLower();
			catExt.MakeLower();
			for (int iPos = 0; iPos >= 0;) {
				const CString &cmpExt(catExt.Tokenize(_T("|"), iPos));
				if (!cmpExt.IsEmpty())
					break;
				// HoaX_69: Allow wildcards in autocat string
				// thanks to: bluecow, khaos and SlugFiller
				if ((cmpExt.FindOneOf(_T("*?")) && PathMatchSpec(fullname, cmpExt)) // Use wildcards
					|| fullname.Find(cmpExt) >= 0) //simple string comparison
				{
					bFound = true;
					break;
				}
			}
		}
		if (bFound) {
			newfile->SetCategory(i);
			return;
		}
	}
}

void CDownloadQueue::ResetLocalServerRequests()
{
	m_dwNextTCPSrcReq = 0;
	m_localServerReqQueue.RemoveAll();

	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *pFile = filelist.GetNext(pos);
		EPartFileStatus uState = pFile->GetStatus();
		if (uState == PS_READY || uState == PS_EMPTY)
			pFile->ResumeFile();
		pFile->m_bLocalSrcReqQueued = false;
	}
}

void CDownloadQueue::RemoveLocalServerRequest(CPartFile *pFile)
{
	POSITION pos = m_localServerReqQueue.Find(pFile);
	if (pos) {
		m_localServerReqQueue.RemoveAt(pos);
		pFile->m_bLocalSrcReqQueued = false;
	}
}

void CDownloadQueue::ProcessLocalRequests()
{
	if (!m_localServerReqQueue.IsEmpty() && ::GetTickCount() >= m_dwNextTCPSrcReq) {
		CSafeMemFile dataTcpFrame(22);
		const int iMaxFilesPerTcpFrame = 15;
		int iFiles = 0;
		while (!m_localServerReqQueue.IsEmpty() && iFiles < iMaxFilesPerTcpFrame) {
			// find the file with the longest waitingtime
			uint32 dwBestWaitTime = _UI32_MAX;
			POSITION posNextRequest = NULL;
			for (POSITION pos = m_localServerReqQueue.GetHeadPosition(); pos != NULL;) {
				POSITION pos2 = pos;
				CPartFile *cur_file = m_localServerReqQueue.GetNext(pos);
				if (cur_file->GetStatus() == PS_READY || cur_file->GetStatus() == PS_EMPTY) {
					uint8 nPriority = cur_file->GetDownPriority();
					if (nPriority > PR_HIGH) {
						ASSERT(0);
						nPriority = PR_HIGH;
					}

					if (cur_file->m_LastSearchTime + (PR_HIGH - nPriority) < dwBestWaitTime) {
						dwBestWaitTime = cur_file->m_LastSearchTime + (PR_HIGH - nPriority);
						posNextRequest = pos2;
					}
				} else {
					m_localServerReqQueue.RemoveAt(pos2);
					cur_file->m_bLocalSrcReqQueued = false;
					if (thePrefs.GetDebugSourceExchange())
						AddDebugLogLine(false, _T("SXSend: Local server source request for file \"%s\" not sent because of status '%s'"), (LPCTSTR)cur_file->GetFileName(), (LPCTSTR)cur_file->getPartfileStatus());
				}
			}

			if (posNextRequest != NULL) {
				CPartFile *cur_file = m_localServerReqQueue.GetAt(posNextRequest);
				cur_file->m_bLocalSrcReqQueued = false;
				cur_file->m_LastSearchTime = ::GetTickCount();
				m_localServerReqQueue.RemoveAt(posNextRequest);

				if (cur_file->IsLargeFile() && (theApp.serverconnect->GetCurrentServer() == NULL || !theApp.serverconnect->GetCurrentServer()->SupportsLargeFilesTCP())) {
					ASSERT(0);
					DebugLogError(_T("Large file (%s) on local requestqueue for server without support for large files"), (LPCTSTR)cur_file->GetFileName());
					continue;
				}

				++iFiles;

				// create request packet
				CSafeMemFile smPacket;
				smPacket.WriteHash16(cur_file->GetFileHash());
				if (!cur_file->IsLargeFile())
					smPacket.WriteUInt32((uint32)(uint64)cur_file->GetFileSize());
				else {
					smPacket.WriteUInt32(0); // indicates that this is a large file and a uint64 follows
					smPacket.WriteUInt64(cur_file->GetFileSize());
				}

				uint8 byOpcode;
				if (thePrefs.IsClientCryptLayerSupported() && theApp.serverconnect->GetCurrentServer() != NULL && theApp.serverconnect->GetCurrentServer()->SupportsGetSourcesObfuscation())
					byOpcode = OP_GETSOURCES_OBFU;
				else
					byOpcode = OP_GETSOURCES;

				Packet packet(&smPacket, OP_EDONKEYPROT, byOpcode);
				if (thePrefs.GetDebugServerTCPLevel() > 0)
					Debug(_T(">>> Sending OP_GetSources%s(%2u/%2u); %s\n"), (byOpcode == OP_GETSOURCES) ? _T("") : _T("_OBFU"), iFiles, iMaxFilesPerTcpFrame, (LPCTSTR)DbgGetFileInfo(cur_file->GetFileHash()));
				dataTcpFrame.Write(packet.GetPacket(), packet.GetRealPacketSize());

				if (thePrefs.GetDebugSourceExchange())
					AddDebugLogLine(false, _T("SXSend: Local server source request; File=\"%s\""), (LPCTSTR)cur_file->GetFileName());
			}
		}

		int iSize = (int)dataTcpFrame.GetLength();
		if (iSize > 0) {
			// create one 'packet' which contains all buffered OP_GETSOURCES eD2K packets to be sent with one TCP frame
			// server credits: 16*iMaxFilesPerTcpFrame+1 = 241
			Packet *packet = new Packet(new char[iSize], (uint32)dataTcpFrame.GetLength(), true, false);
			dataTcpFrame.Seek(0, CFile::begin);
			dataTcpFrame.Read(packet->GetPacket(), iSize);
			theStats.AddUpDataOverheadServer(packet->size);
			theApp.serverconnect->SendPacket(packet);
		}

		// next TCP frame with up to 15 source requests is allowed to be sent in
		m_dwNextTCPSrcReq = ::GetTickCount() + SEC2MS(iMaxFilesPerTcpFrame * (16 + 4));
	}
}

void CDownloadQueue::SendLocalSrcRequest(CPartFile *sender)
{
	ASSERT(!m_localServerReqQueue.Find(sender));
	m_localServerReqQueue.AddTail(sender);
}

int CDownloadQueue::GetDownloadFilesStats(uint64 &rui64TotalFileSize
	, uint64 &rui64TotalLeftToTransfer
	, uint64 &rui64TotalAdditionalNeededSpace)
{
	int iActiveFiles = 0;
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		const CPartFile *cur_file = filelist.GetNext(pos);
		EPartFileStatus uState = cur_file->GetStatus();
		if (uState == PS_READY || uState == PS_EMPTY) {
			uint64 ui64LeftToTransfer = 0;
			uint64 ui64AdditionalNeededSpace = 0;
			cur_file->GetLeftToTransferAndAdditionalNeededSpace(ui64LeftToTransfer, ui64AdditionalNeededSpace);
			rui64TotalFileSize += (uint64)cur_file->GetFileSize();
			rui64TotalLeftToTransfer += ui64LeftToTransfer;
			rui64TotalAdditionalNeededSpace += ui64AdditionalNeededSpace;
			++iActiveFiles;
		}
	}
	return iActiveFiles;
}

///////////////////////////////////////////////////////////////////////////////
// CSourceHostnameResolveWnd

#define WM_HOSTNAMERESOLVED		(WM_USER + 0x101)	// does not need to be placed in "UserMsgs.h"

BEGIN_MESSAGE_MAP(CSourceHostnameResolveWnd, CWnd)
	ON_MESSAGE(WM_HOSTNAMERESOLVED, OnHostnameResolved)
END_MESSAGE_MAP()

CSourceHostnameResolveWnd::CSourceHostnameResolveWnd()
	: m_aucHostnameBuffer()
{
}

CSourceHostnameResolveWnd::~CSourceHostnameResolveWnd()
{
	while (!m_toresolve.IsEmpty())
		delete m_toresolve.RemoveHead();
}

void CSourceHostnameResolveWnd::AddToResolve(const uchar *fileid, LPCSTR pszHostname, uint16 port, LPCTSTR pszURL)
{
	bool bResolving = !m_toresolve.IsEmpty();

	// double checking
	if (!theApp.downloadqueue->GetFileByID(fileid))
		return;

	Hostname_Entry *entry = new Hostname_Entry;
	md4cpy(entry->fileid, fileid);
	entry->strHostname = pszHostname;
	entry->port = port;
	entry->strURL = pszURL;
	m_toresolve.AddTail(entry);

	if (bResolving)
		return;

	memset(m_aucHostnameBuffer, 0, sizeof m_aucHostnameBuffer);
	if (!WSAAsyncGetHostByName(m_hWnd, WM_HOSTNAMERESOLVED, entry->strHostname, m_aucHostnameBuffer, sizeof m_aucHostnameBuffer)) {
		m_toresolve.RemoveHead();
		delete entry;
	}
}

LRESULT CSourceHostnameResolveWnd::OnHostnameResolved(WPARAM, LPARAM lParam)
{
	if (m_toresolve.IsEmpty())
		return TRUE;
	Hostname_Entry *resolved = m_toresolve.RemoveHead();
	if (WSAGETASYNCERROR(lParam) == 0) {
		unsigned iBufLen = WSAGETASYNCBUFLEN(lParam);
		if (iBufLen >= sizeof(HOSTENT)) {
			LPHOSTENT pHost = (LPHOSTENT)m_aucHostnameBuffer;
			if (pHost->h_length == 4 && pHost->h_addr_list && pHost->h_addr_list[0]) {
				uint32 nIP = ((LPIN_ADDR)(pHost->h_addr_list[0]))->s_addr;

				CPartFile *file = theApp.downloadqueue->GetFileByID(resolved->fileid);
				if (file)
					if (resolved->strURL.IsEmpty()) {
						CSafeMemFile sources(1 + 4 + 2);
						sources.WriteUInt8(1);
						sources.WriteUInt32(nIP);
						sources.WriteUInt16(resolved->port);
						sources.SeekToBegin();
						file->AddSources(&sources, 0, 0, false);
					} else
						file->AddSource(resolved->strURL, nIP);

			}
		}
	}
	delete resolved;

	while (!m_toresolve.IsEmpty()) {
		Hostname_Entry *entry = m_toresolve.GetHead();
		memset(m_aucHostnameBuffer, 0, sizeof m_aucHostnameBuffer);
		if (WSAAsyncGetHostByName(m_hWnd, WM_HOSTNAMERESOLVED, entry->strHostname, m_aucHostnameBuffer, sizeof m_aucHostnameBuffer) != 0)
			return TRUE;
		m_toresolve.RemoveHead();
		delete entry;
	}
	return TRUE;
}

bool CDownloadQueue::DoKademliaFileRequest() const
{
	return (::GetTickCount() >= lastkademliafilerequest + KADEMLIAASKTIME);
}

void CDownloadQueue::KademliaSearchFile(uint32 searchID, const Kademlia::CUInt128 *pcontactID, const Kademlia::CUInt128 *pbuddyID, uint8 type, uint32 ip, uint16 tcp, uint16 udp, uint32 dwBuddyIP, uint16 dwBuddyPort, uint8 byCryptOptions)
{
	//Safety measure to make sure we are looking for these sources
	CPartFile *temp = GetFileByKadFileSearchID(searchID);
	if (!temp)
		return;
	//Do we need more sources?
	if (temp->IsStopped() || temp->GetMaxSources() <= temp->GetSourceCount())
		return;

	uint32 ED2Kip = htonl(ip);
	if (theApp.ipfilter->IsFiltered(ED2Kip)) {
		if (thePrefs.GetLogFilteredIPs())
			AddDebugLogLine(false, _T("IPfiltered source IP=%s (%s) received from Kademlia"), (LPCTSTR)ipstr(ED2Kip), (LPCTSTR)theApp.ipfilter->GetLastHit());
		return;
	}
	if ((ip == Kademlia::CKademlia::GetIPAddress() || ED2Kip == theApp.serverconnect->GetClientID()) && tcp == thePrefs.GetPort())
		return;
	CUpDownClient *ctemp = NULL;
	//DEBUG_ONLY( DebugLog(_T("Kadsource received, type %u, IP %s"), type, (LPCTSTR)ipstr(ED2Kip)) );
	switch (type) {
	case 4:
	case 1:
		{
			//NonFirewalled users
			if (!tcp) {
				if (thePrefs.GetVerbose())
					AddDebugLogLine(false, _T("Ignored source (IP=%s) received from Kademlia, no TCP port received"), (LPCTSTR)ipstr(ip));
				return;
			}
			ctemp = new CUpDownClient(temp, tcp, ip, 0, 0, false);
			ctemp->SetSourceFrom(SF_KADEMLIA);
			// not actually sent or needed for HighID sources
			//ctemp->SetServerIP(serverip);
			//ctemp->SetServerPort(serverport);
			ctemp->SetKadPort(udp);
			byte cID[16];
			pcontactID->ToByteArray(cID);
			ctemp->SetUserHash(cID);
		}
		break;
	case 2:
		//Don't use this type... Some clients will process it wrong.
		break;
	case 5:
	case 3:
		//This will be a firewalled client connected to Kad only.
		// if we are firewalled ourself, the source is useless to us
		if (theApp.IsFirewalled())
			break;

		if (theApp.ipfilter->IsFiltered(dwBuddyIP)) {
			if (thePrefs.GetLogFilteredIPs())
				AddDebugLogLine(false, _T("Source with an IP-filtered buddy IP=%s (%s) received from Kademlia"), (LPCTSTR)ipstr(dwBuddyIP), (LPCTSTR)theApp.ipfilter->GetLastHit());
			break;
		}
		if (theApp.clientlist->IsBannedClient(dwBuddyIP)) {
			if (thePrefs.GetLogBannedClients())
				AddDebugLogLine(false, _T("Source with a Banned buddy IP=%s received from Kademlia"), (LPCTSTR)ipstr(dwBuddyIP));
		} else {
			//We set the clientID to 1 as a Kad user only has 1 buddy.
			ctemp = new CUpDownClient(temp, tcp, 1, 0, 0, false);
			//The only reason we set the real IP is for when we get a callback
			//from this firewalled source, the compare method will match them.
			ctemp->SetSourceFrom(SF_KADEMLIA);
			ctemp->SetKadPort(udp);
			byte cID[16];
			pcontactID->ToByteArray(cID);
			ctemp->SetUserHash(cID);
			pbuddyID->ToByteArray(cID);
			ctemp->SetBuddyID(cID);
			ctemp->SetBuddyIP(dwBuddyIP);
			ctemp->SetBuddyPort(dwBuddyPort);
		}
		break;
	case 6:
		// firewalled source which supports direct UDP callback
		// if we are firewalled ourselves, the source is useless to us
		if (theApp.IsFirewalled())
			break;

		if ((byCryptOptions & 0x08) == 0)
			DebugLogWarning(_T("Received Kad source type 6 (direct callback) which has the direct callback flag not set (%s)"), (LPCTSTR)ipstr(ED2Kip));
		else {
			ctemp = new CUpDownClient(temp, tcp, 1, 0, 0, false);
			ctemp->SetSourceFrom(SF_KADEMLIA);
			ctemp->SetKadPort(udp);
			ctemp->SetConnectIP(ED2Kip); // need to set the IP address, which cannot be used for TCP but for UDP
			byte cID[16];
			pcontactID->ToByteArray(cID);
			ctemp->SetUserHash(cID);
		}
	}

	if (ctemp != NULL) {
		// add encryption settings
		ctemp->SetConnectOptions(byCryptOptions);
		CheckAndAddSource(temp, ctemp);
	}
}

void CDownloadQueue::ExportPartMetFilesOverview() const
{
	CString strFileListPath(thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("downloads.txt"));

	CString strTmpFileListPath = strFileListPath;
	PathRenameExtension(strTmpFileListPath.GetBuffer(MAX_PATH), _T(".tmp"));
	strTmpFileListPath.ReleaseBuffer();

	CSafeBufferedFile file;
	CFileException fexp;
	if (!file.Open(strTmpFileListPath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary | CFile::shareDenyWrite, &fexp)) {
		CString strError;
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		if (GetExceptionMessage(fexp, szError, _countof(szError)))
			strError.AppendFormat(_T(" - %s"), szError);
		LogError(_T("Failed to create part.met file list%s"), (LPCTSTR)strError);
		return;
	}

	// write Unicode byte order mark 0xFEFF
	fputwc(0xFEFFui16, file.m_pStream);

	try {
		file.printf(_T("Date:      %s\r\n"), (LPCTSTR)CTime::GetCurrentTime().Format(_T("%c")));
		if (thePrefs.GetTempDirCount() == 1)
			file.printf(_T("Directory: %s\r\n"), thePrefs.GetTempDir());
		file.printf(_T("\r\n"));
		file.printf(_T("Part file\teD2K link\r\n"));
		file.printf(_T("--------------------------------------------------------------------------------\r\n"));
		for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
			const CPartFile *pPartFile = filelist.GetNext(pos);
			if (pPartFile && pPartFile->GetStatus(true) != PS_COMPLETE) {
				CString strPartFilePath(pPartFile->GetFilePath());
				TCHAR szNam[_MAX_FNAME];
				TCHAR szExt[_MAX_EXT];
				_tsplitpath(strPartFilePath, NULL, NULL, szNam, szExt);
				if (thePrefs.GetTempDirCount() == 1)
					file.printf(_T("%s%s\t%s\r\n"), szNam, szExt, (LPCTSTR)pPartFile->GetED2kLink());
				else
					file.printf(_T("%s\t%s\r\n"), (LPCTSTR)pPartFile->GetFullName(), (LPCTSTR)pPartFile->GetED2kLink());
			}
		}

		if ((theApp.IsClosing() && thePrefs.GetCommitFiles() >= 1) || thePrefs.GetCommitFiles() >= 2) {
			file.Flush(); // flush file stream buffers to disk buffers
			if (_commit(_fileno(file.m_pStream)) != 0) // commit disk buffers to disk
				AfxThrowFileException(CFileException::hardIO, ::GetLastError(), file.GetFileName());
		}
		file.Close();

		CString strBakFileListPath = strFileListPath;
		PathRenameExtension(strBakFileListPath.GetBuffer(MAX_PATH), _T(".bak"));
		strBakFileListPath.ReleaseBuffer();

		if (_taccess(strBakFileListPath, 0) == 0)
			CFile::Remove(strBakFileListPath);
		if (_taccess(strFileListPath, 0) == 0)
			CFile::Rename(strFileListPath, strBakFileListPath);
		CFile::Rename(strTmpFileListPath, strFileListPath);
	} catch (CFileException *e) {
		CString strError;
		TCHAR szError[MAX_CFEXP_ERRORMSG];
		if (GetExceptionMessage(*e, szError, _countof(szError)))
			strError.AppendFormat(_T(" - %s"), szError);
		LogError(_T("Failed to write part.met file list%s"), (LPCTSTR)strError);
		e->Delete();
		file.Abort();
		(void)_tremove(file.GetFilePath());
	}
}

void CDownloadQueue::OnConnectionState(bool bConnected)
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		CPartFile *pPartFile = filelist.GetNext(pos);
		if (pPartFile->GetStatus() == PS_READY || pPartFile->GetStatus() == PS_EMPTY)
			pPartFile->SetActive(bConnected);
	}
}

CString CDownloadQueue::GetOptimalTempDir(UINT nCat, EMFileSize nFileSize)
{
	// shortcut
	if (thePrefs.GetTempDirCount() == 1)
		return thePrefs.GetTempDir();

	CMap<int, int, sint64, sint64> mapNeededSpaceOnDrive;
	CMap<int, int, sint64, sint64> mapFreeSpaceOnDrive;

	sint64 llBuffer = 0;
	sint64 llHighestFreeSpace = 0;
	int	nHighestFreeSpaceDrive = -1;
	// first collect the free space on drives
	for (INT_PTR i = 0; i < thePrefs.GetTempDirCount(); ++i) {
		const int nDriveNumber = GetPathDriveNumber(thePrefs.GetTempDir(i));
		if (mapFreeSpaceOnDrive.Lookup(nDriveNumber, llBuffer))
			continue;
		llBuffer = GetFreeDiskSpaceX(thePrefs.GetTempDir(i)) - thePrefs.GetMinFreeDiskSpace();
		mapFreeSpaceOnDrive.SetAt(nDriveNumber, llBuffer);
		if (llBuffer > llHighestFreeSpace) {
			nHighestFreeSpaceDrive = nDriveNumber;
			llHighestFreeSpace = llBuffer;
		}

	}

	// now get the space we would need to download all files in the current queue
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;) {
		const CPartFile *pCurFile = filelist.GetNext(pos);
		const int nDriveNumber = GetPathDriveNumber(pCurFile->GetTempPath());

		sint64 llNeededForCompletion = 0;
		switch (pCurFile->GetStatus(false)) {
		case PS_READY:
		case PS_EMPTY:
		case PS_WAITINGFORHASH:
		case PS_INSUFFICIENT:
			llNeededForCompletion = (uint64)pCurFile->GetFileSize() - (uint64)pCurFile->GetRealFileSize();
			if (llNeededForCompletion < 0)
				llNeededForCompletion = 0;
		}
		if (!mapNeededSpaceOnDrive.Lookup(nDriveNumber, llBuffer))
			llBuffer = 0;
		mapNeededSpaceOnDrive.SetAt(nDriveNumber, llBuffer + llNeededForCompletion);
	}

	sint64 llHighestTotalSpace = 0;
	int	nHighestTotalSpaceDir = -1;
	int	nHighestFreeSpaceDir = -1;
	int	nAnyAvailableDir = -1;
	// first round (0): on same drive as incoming and enough space for all downloading
	// second round (1): enough space for all downloading
	// third round (2): largest actual free space
	for (INT_PTR i = 0; i < thePrefs.GetTempDirCount(); ++i) {
		const int nDriveNumber = GetPathDriveNumber(thePrefs.GetTempDir(i));
		sint64 llAvailableSpace;
		if (!mapFreeSpaceOnDrive.Lookup(nDriveNumber, llAvailableSpace))
			llAvailableSpace = 0;
		if (mapNeededSpaceOnDrive.Lookup(nDriveNumber, llBuffer))
			llAvailableSpace -= llBuffer;

		// no condition can be met for a large file on a FAT volume
		if (nFileSize <= OLD_MAX_EMULE_FILE_SIZE || !IsFileOnFATVolume(thePrefs.GetTempDir(i))) {
			// condition 0
			// needs to be same drive and enough space
			if (GetPathDriveNumber(thePrefs.GetCatPath(nCat)) == nDriveNumber &&
				llAvailableSpace > (sint64)(uint64)nFileSize) {
				//this one is perfect
				return thePrefs.GetTempDir(i);
			}
			// condition 1
			// needs to have enough space for downloading
			if (llAvailableSpace > (sint64)nFileSize && llAvailableSpace > llHighestTotalSpace) {
				llHighestTotalSpace = llAvailableSpace;
				nHighestTotalSpaceDir = (int)i;
			}
			// condition 2
			// first one which has the highest actually free space
			if (nDriveNumber == nHighestFreeSpaceDrive && nHighestFreeSpaceDir == -1)
				nHighestFreeSpaceDir = (int)i;
			// condition 3
			// any directory which can be used for this file (ak not FAT for large files)
			if (nAnyAvailableDir == -1)
				nAnyAvailableDir = (int)i;
		}
	}

	if (nHighestTotalSpaceDir != -1) // condition 0 was apparently too much, take 1
		return thePrefs.GetTempDir(nHighestTotalSpaceDir);

	if (nHighestFreeSpaceDir != -1) // condition 1 could not be met too, take 2
		return thePrefs.GetTempDir(nHighestFreeSpaceDir);

	if (nAnyAvailableDir != -1)
		return thePrefs.GetTempDir(nAnyAvailableDir);

	// so was condition 2 and 3, take 4... wait there is no 3 - this must be a bug
	ASSERT(0);
	return thePrefs.GetTempDir();
}

void CDownloadQueue::RefilterAllComments()
{
	for (POSITION pos = filelist.GetHeadPosition(); pos != NULL;)
		filelist.GetNext(pos)->RefilterFileComments();
}