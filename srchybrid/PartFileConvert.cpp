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
#include "PartFileConvert.h"
#include "OtherFunctions.h"
#include "DownloadQueue.h"
#include "PartFile.h"
#include "Preferences.h"
#include "SafeFile.h"
#include "SharedFileList.h"
#include "emuledlg.h"
#include "Log.h"
#include "opcodes.h"
#include "MuleListCtrl.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

enum convstatus
{
	CONV_OK = 0,
	CONV_QUEUE,
	CONV_INPROGRESS,
	CONV_OUTOFDISKSPACE,
	CONV_PARTMETNOTFOUND,
	CONV_IOERROR,
	CONV_FAILED,
	CONV_BADFORMAT,
	CONV_ALREADYEXISTS
};

struct ConvertJob
{
	CString folder;
	CString filename;
	CString filehash;
	uint64	size;
	uint64	spaceneeded;
	int     format;
	int		state;
	uint8	partmettype;
	bool	removeSource;
	ConvertJob()
		: size()
		, spaceneeded()
		, format()
		, state()
		, partmettype(PMT_UNKNOWN)
		, removeSource(true)
	{
	}
	//~ConvertJob() = default;
};


int CPartFileConvert::ScanFolderToAdd(const CString &folder, bool deletesource)
{
	CFileFind finder;
	int count = 0;
	BOOL bWorking = finder.FindFile(folder + _T("\\*.part.met"));
	while (bWorking) {
		bWorking = finder.FindNextFile();
		ConvertToeMule(finder.GetFilePath(), deletesource);
		++count;
	}
	// Shareaza
	bWorking = finder.FindFile(folder + _T("\\*.sd"));
	while (bWorking) {
		bWorking = finder.FindNextFile();
		ConvertToeMule(finder.GetFilePath(), deletesource);
		++count;
	}

	bWorking = finder.FindFile(folder + _T("\\*.*"));
	while (bWorking) {
		bWorking = finder.FindNextFile();
		if (finder.IsDirectory() && finder.GetFileName().Left(1) != _T("."))
			count += ScanFolderToAdd(finder.GetFilePath(), deletesource);
	}
	return count;
}

void CPartFileConvert::ConvertToeMule(const CString &folder, bool deletesource)
{
	if (!PathFileExists(folder))
		return;
	//if (folder.Left(strlen(thePrefs.GetTempDir())).CompareNoCase(thePrefs.GetTempDir()) == 0)
	//	return;

	ConvertJob *newjob = new ConvertJob();
	newjob->folder = folder;
	newjob->removeSource = deletesource;
	newjob->state = CONV_QUEUE;
	m_jobs.AddTail(newjob);

	if (m_convertgui)
		m_convertgui->AddJob(newjob);

	StartThread();
}

void CPartFileConvert::StartThread()
{
	if (convertPfThread == NULL)
		convertPfThread = AfxBeginThread(run, NULL, THREAD_PRIORITY_LOWEST);
}

UINT AFX_CDECL CPartFileConvert::run(LPVOID /*lpParam*/)
{
	DbgSetThreadName("Partfile-Converter");
	InitThreadLocale();

	int imported = 0;

	for (;;) {
		// search next queued job and start it
		pfconverting = NULL;
		for (POSITION pos = m_jobs.GetHeadPosition(); pos != NULL;) {
			pfconverting = m_jobs.GetNext(pos);
			if (pfconverting->state == CONV_QUEUE)
				break;
			pfconverting = NULL;
		}
		if (pfconverting == NULL)
			break;// nothing more to do now
		pfconverting->state = CONV_INPROGRESS;
		UpdateGUI(pfconverting);
		pfconverting->state = performConvertToeMule(pfconverting->folder);

		if (pfconverting->state == CONV_OK)
			++imported;

		UpdateGUI(pfconverting);
		AddLogLine(true, GetResString(IDS_IMP_STATUS), (LPCTSTR)pfconverting->folder, (LPCTSTR)GetReturncodeText(pfconverting->state));
	}

	// clean up
	UpdateGUI(NULL);

	if (imported)
		theApp.sharedfiles->PublishNextTurn();

	convertPfThread = NULL;
	return 0;
}

int CPartFileConvert::performConvertToeMule(const CString &folder)
{
	int i = folder.ReverseFind(_T('\\'));
	if (i < 0)
		return CONV_FAILED;
	const CString &partfile(folder.Mid(i + 1));
	CString sDir(folder.Left(i)); //no trailing backslash

	UpdateGUI(0, GetResString(IDS_IMP_STEPREADPF), true);

	const CString &filepartindex(partfile.Left(partfile.Find(_T('.'))));

	UpdateGUI(4, GetResString(IDS_IMP_STEPBASICINF));

	CPartFile *file = new CPartFile();
	EPartFileFormat eFormat = PMT_UNKNOWN;
	if (file->LoadPartFile(sDir, partfile, &eFormat) == PLR_CHECKSUCCESS) {
		pfconverting->partmettype = (uint8)eFormat;
		switch (pfconverting->partmettype) {
		case PMT_UNKNOWN:
		case PMT_BADFORMAT:
			delete file;
			return CONV_BADFORMAT;
		}
	} else {
		delete file;
		return CONV_BADFORMAT;
	}

	pfconverting->size = file->GetFileSize();
	pfconverting->filename = file->GetFileName();
	pfconverting->filehash = EncodeBase16(file->GetFileHash(), 16);
	UpdateGUI(pfconverting);

	if (theApp.downloadqueue->GetFileByID(file->GetFileHash()) != NULL) {
		delete file;
		return CONV_ALREADYEXISTS;
	}

	sDir += _T('\\'); //now add trailing backslash
	CString newfilename;
	CFileFind finder;
	CString buffer;
	if (pfconverting->partmettype == PMT_SPLITTED) {
		try {
			CByteArray ba;
			ba.SetSize(PARTSIZE);

			// just count
			UINT maxindex = 0;
			UINT partfilecount = 0;
			BOOL bWorking = finder.FindFile(sDir + filepartindex + _T(".*.part"));
			while (bWorking) {
				bWorking = finder.FindNextFile();
				++partfilecount;
				buffer = finder.GetFileName();
				int pos1 = buffer.Find('.');
				int pos2 = buffer.Find('.', pos1 + 1);
				UINT fileindex = _tstoi(buffer.Mid(pos1 + 1, pos2 - pos1));
				if (fileindex > maxindex)
					maxindex = fileindex;
			}
			float stepperpart;
			if (partfilecount > 0) {
				stepperpart = (80.0f / partfilecount);
				if (maxindex*PARTSIZE <= pfconverting->size)
					pfconverting->spaceneeded = maxindex * PARTSIZE;
				else
					pfconverting->spaceneeded = ((pfconverting->size / PARTSIZE) * PARTSIZE) + (pfconverting->size % PARTSIZE);
			} else {
				stepperpart = 80.0f;
				pfconverting->spaceneeded = 0;
			}

			UpdateGUI(pfconverting);

			if (GetFreeDiskSpaceX(thePrefs.GetTempDir()) < maxindex*PARTSIZE) {
				delete file;
				return CONV_OUTOFDISKSPACE;
			}

			// create new partmetfile, and remember the new name
			file->CreatePartFile();
			newfilename = file->GetFullName();

			UpdateGUI(8, GetResString(IDS_IMP_STEPCRDESTFILE));
			file->m_hpartfile.SetLength(pfconverting->spaceneeded);

			CFile inputfile;
			unsigned curindex = 0;
			bWorking = finder.FindFile(sDir + filepartindex + _T(".*.part"));
			while (bWorking) {
				bWorking = finder.FindNextFile();

				//stats
				++curindex;
				buffer.Format(GetResString(IDS_IMP_LOADDATA), curindex, partfilecount);
				UpdateGUI(10 + (curindex*stepperpart), buffer);

				const CString &filename(finder.GetFileName());
				int pos1 = filename.Find('.');
				int pos2 = filename.Find('.', pos1 + 1);
				int fileindex = _tstoi(filename.Mid(pos1 + 1, pos2 - pos1));
				if (fileindex <= 0)
					continue;

				ULONGLONG chunkstart = fileindex * PARTSIZE + PARTSIZE - 1;

				// open, read data of the part-part-file into buffer, close file
				inputfile.Open(finder.GetFilePath(), CFile::modeRead | CFile::shareDenyWrite);
				UINT readed = inputfile.Read(ba.GetData(), (UINT)PARTSIZE);
				inputfile.Close();

				buffer.Format(GetResString(IDS_IMP_SAVEDATA), curindex, partfilecount);
				UpdateGUI(10 + (curindex*stepperpart), buffer);

				// write the buffered data
				file->m_hpartfile.Seek(chunkstart, CFile::begin);
				file->m_hpartfile.Write(ba.GetData(), readed);
			}
		} catch (CFileException *error) {
			CString strError(GetResString(IDS_IMP_IOERROR));
			TCHAR szError[MAX_CFEXP_ERRORMSG];
			if (GetExceptionMessage(*error, szError, _countof(szError)))
				strError.AppendFormat(_T(" - %s"), szError);
			LogError(LOG_DEFAULT, _T("%s"), (LPCTSTR)strError);
			error->Delete();
			delete file;
			return CONV_IOERROR;
		}
		file->m_hpartfile.Close();
	} else {
		// import an external common format part download
		//if (pfconverting->partmettype == PMT_DEFAULTOLD || pfconverting->partmettype == PMT_NEWOLD || Shareaza)
		const CString &oldfile(sDir + partfile.Left(partfile.GetLength() - ((pfconverting->partmettype == PMT_SHAREAZA) ? 3 : 4)));

		if (!pfconverting->removeSource)
			pfconverting->spaceneeded = GetDiskFileSize(oldfile);

		UpdateGUI(pfconverting);

		if (!pfconverting->removeSource && (GetFreeDiskSpaceX(thePrefs.GetTempDir()) < pfconverting->spaceneeded)) {
			delete file;
			return CONV_OUTOFDISKSPACE;
		}

		file->CreatePartFile();
		newfilename = file->GetFullName();

		file->m_hpartfile.Close();

		UpdateGUI(92, GetResString(IDS_COPY));
		::DeleteFile(newfilename.Left(newfilename.GetLength() - 4));

		BOOL ret;
		if (!PathFileExists(oldfile)) {
			// data file does not exist. well, then create a 0 byte big one
			HANDLE hFile = ::CreateFile(newfilename.Left(newfilename.GetLength() - 4),	// file to open
				GENERIC_WRITE,			// open for reading
				FILE_SHARE_READ,		// share for reading
				NULL,					// default security
				CREATE_NEW,				// existing file only
				FILE_ATTRIBUTE_NORMAL,	// normal file
				NULL);					// no attr. template

			ret = (hFile != INVALID_HANDLE_VALUE);

			::CloseHandle(hFile);
		} else if (pfconverting->removeSource)
			ret = ::MoveFile(oldfile, newfilename.Left(newfilename.GetLength() - 4));
		else
			ret = ::CopyFile(oldfile, newfilename.Left(newfilename.GetLength() - 4), FALSE);

		if (!ret) {
			file->DeletePartFile(); //delayed delete
			return CONV_FAILED;
		}
	}

	UpdateGUI(94, GetResString(IDS_IMP_GETPFINFO));

	::DeleteFile(newfilename);
	if (pfconverting->removeSource)
		::MoveFile(sDir + partfile, newfilename);
	else
		::CopyFile(sDir + partfile, newfilename, FALSE);

	// clean up
	file->GetFileIdentifier().DeleteMD4Hashset();
	while (!file->m_gaplist.IsEmpty())
		delete file->m_gaplist.RemoveHead();
	file->ClearTags();
	// reload from part.met file
	if (file->LoadPartFile(thePrefs.GetTempDir(), file->GetPartMetFileName()) != PLR_LOADSUCCESS) {
		file->DeletePartFile(); //delayed delete
		return CONV_BADFORMAT;
	}

	if (pfconverting->partmettype == PMT_NEWOLD || pfconverting->partmettype == PMT_SPLITTED) {
		file->m_completedsize = file->m_uTransferred;
		file->m_uCompressionGain = 0;
		file->m_uCorruptionLoss = 0;
	}

	UpdateGUI(100, GetResString(IDS_IMP_ADDDWL));

	theApp.downloadqueue->AddDownload(file, thePrefs.AddNewFilesPaused());
	file->SavePartFile();

	if (file->GetStatus(true) == PS_READY)
		theApp.sharedfiles->SafeAddKFile(file); // part files are always shared files


	if (pfconverting->removeSource) {
		BOOL bWorking = finder.FindFile(sDir + filepartindex + _T(".*"));
		while (bWorking) {
			bWorking = finder.FindNextFile();
			VERIFY(_tunlink(finder.GetFilePath()) == 0);
		}

		if (pfconverting->partmettype == PMT_SPLITTED)
			RemoveDirectory(sDir);
	}

	return CONV_OK;
}

void CPartFileConvert::UpdateGUI(float percent, const CString &text, bool fullinfo)
{
	if (m_convertgui == NULL)
		return;

	m_convertgui->pb_current.SetPos((int)percent);
	CString buffer;
	buffer.Format(_T("%.2f %%"), percent);
	m_convertgui->SetDlgItemText(IDC_CONV_PROZENT, buffer);

	if (!text.IsEmpty())
		m_convertgui->SetDlgItemText(IDC_CONV_PB_LABEL, text);

	if (fullinfo)
		m_convertgui->SetDlgItemText(IDC_CURJOB, pfconverting->folder);
}

void CPartFileConvert::UpdateGUI(ConvertJob *job)
{
	if (m_convertgui != NULL)
		m_convertgui->UpdateJobInfo(job);
}


void CPartFileConvert::ShowGUI()
{
	if (m_convertgui)
		m_convertgui->SetForegroundWindow();
	else {
		m_convertgui = new CPartFileConvertDlg();
		m_convertgui->Create(IDD_CONVERTPARTFILES, CWnd::GetDesktopWindow());//,  );
		InitWindowStyles(m_convertgui);
		m_convertgui->ShowWindow(SW_SHOW);

		m_convertgui->AddAnchor(IDC_CONV_PB_CURRENT, TOP_LEFT, TOP_RIGHT);
		m_convertgui->AddAnchor(IDC_CURJOB, TOP_LEFT, TOP_RIGHT);
		m_convertgui->AddAnchor(IDC_CONV_PB_LABEL, TOP_LEFT, TOP_RIGHT);
		m_convertgui->AddAnchor(IDC_CONV_PROZENT, TOP_RIGHT);
		m_convertgui->AddAnchor(IDC_JOBLIST, TOP_LEFT, BOTTOM_RIGHT);
		m_convertgui->AddAnchor(IDC_ADDITEM, BOTTOM_LEFT);
		m_convertgui->AddAnchor(IDC_RETRY, BOTTOM_LEFT);
		m_convertgui->AddAnchor(IDC_CONVREMOVE, BOTTOM_LEFT);
		m_convertgui->AddAnchor(IDC_HIDECONVDLG, BOTTOM_RIGHT);

		m_convertgui->SetIcon(m_convertgui->m_icoWnd = theApp.LoadIcon(_T("Convert")), FALSE);

		// init gui
		m_convertgui->pb_current.SetRange(0, 100);
		m_convertgui->joblist.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

		if (!pfconverting == NULL) {
			UpdateGUI(pfconverting);
			UpdateGUI(50, GetResString(IDS_IMP_FETCHSTATUS), true);
		}

		Localize();

		// fill joblist
		for (POSITION pos = m_jobs.GetHeadPosition(); pos != NULL;) {
			ConvertJob *job = m_jobs.GetNext(pos);
			m_convertgui->AddJob(job);
			UpdateGUI(job);
		}
	}
}

void CPartFileConvert::Localize()
{
	if (!m_convertgui)
		return;

	for (int i = 0; i < 4; ++i)
		m_convertgui->joblist.DeleteColumn(0);

	m_convertgui->joblist.InsertColumn(0, GetResString(IDS_DL_FILENAME), LVCFMT_LEFT, DFLT_FILENAME_COL_WIDTH);
	m_convertgui->joblist.InsertColumn(1, GetResString(IDS_STATUS), LVCFMT_LEFT, 110);
	m_convertgui->joblist.InsertColumn(2, GetResString(IDS_DL_SIZE), LVCFMT_LEFT, DFLT_SIZE_COL_WIDTH);
	m_convertgui->joblist.InsertColumn(3, GetResString(IDS_FILEHASH), LVCFMT_LEFT, DFLT_HASH_COL_WIDTH);

	// set gui labels
	m_convertgui->SetDlgItemText(IDC_ADDITEM, GetResString(IDS_IMP_ADDBTN));
	m_convertgui->SetDlgItemText(IDC_RETRY, GetResString(IDS_IMP_RETRYBTN));
	m_convertgui->SetDlgItemText(IDC_CONVREMOVE, GetResString(IDS_IMP_REMOVEBTN));
	m_convertgui->SetDlgItemText(IDC_HIDECONVDLG, GetResString(IDS_FD_CLOSE));
	m_convertgui->SetWindowText(GetResString(IDS_IMPORTSPLPF));
}

void CPartFileConvert::CloseGUI()
{
	if (m_convertgui != NULL) {
		m_convertgui->DestroyWindow();
		ClosedGUI();
	}
}

void CPartFileConvert::ClosedGUI()
{
	m_convertgui = NULL;
}

void CPartFileConvert::RemoveAllJobs()
{
	while (!m_jobs.IsEmpty()) {
		ConvertJob *del = m_jobs.RemoveHead();
		if (m_convertgui)
			m_convertgui->RemoveJob(del);
		delete del;
	}
}

void CPartFileConvert::RemoveJob(ConvertJob *job)
{
	POSITION pos = NULL;
	while ((pos = m_jobs.Find(job, pos)) != NULL) {
		ConvertJob *del = m_jobs.GetAt(pos);
		if (m_convertgui)
			m_convertgui->RemoveJob(del);
		m_jobs.RemoveAt(pos);
		delete del;
	}
}

void CPartFileConvert::RemoveAllSuccJobs()
{
	for (POSITION pos = m_jobs.GetHeadPosition(); pos != NULL;) {
		POSITION pos2 = pos;
		ConvertJob *del = m_jobs.GetNext(pos);
		if (del->state == CONV_OK) {
			if (m_convertgui)
				m_convertgui->RemoveJob(del);
			m_jobs.RemoveAt(pos2);
			delete del;
		}
	}
}

CString CPartFileConvert::GetReturncodeText(int ret)
{
	UINT uid;
	switch (ret) {
	case CONV_OK:
		uid = IDS_DL_TRANSFCOMPL;
		break;
	case CONV_INPROGRESS:
		uid = IDS_IMP_INPROGR;
		break;
	case CONV_OUTOFDISKSPACE:
		uid = IDS_IMP_ERR_DISKSP;
		break;
	case CONV_PARTMETNOTFOUND:
		uid = IDS_IMP_ERR_PARTMETIO;
		break;
	case CONV_IOERROR:
		uid = IDS_IMP_ERR_IO;
		break;
	case CONV_FAILED:
		uid = IDS_IMP_ERR_FAILED;
		break;
	case CONV_QUEUE:
		uid = IDS_IMP_STATUSQUEUED;
		break;
	case CONV_ALREADYEXISTS:
		uid = IDS_IMP_ALRDWL;
		break;
	case CONV_BADFORMAT:
		uid = IDS_IMP_ERR_BADFORMAT;
		break;
	default:
		return CString(_T('?'));
	}
	return GetResString(uid);
}


// Modeless Dialog Implementation
// CPartFileConvertDlg dialog

IMPLEMENT_DYNAMIC(CPartFileConvertDlg, CDialog)

BEGIN_MESSAGE_MAP(CPartFileConvertDlg, CResizableDialog)
	ON_WM_SYSCOLORCHANGE()
	ON_BN_CLICKED(IDC_HIDECONVDLG, OnBnClickedOk)
	ON_BN_CLICKED(IDC_ADDITEM, OnAddFolder)
	ON_BN_CLICKED(IDC_RETRY, RetrySel)
	ON_BN_CLICKED(IDC_CONVREMOVE, RemoveSel)
END_MESSAGE_MAP()

CPartFileConvertDlg::CPartFileConvertDlg(CWnd *pParent /*=NULL*/)
	: CResizableDialog(CPartFileConvertDlg::IDD, pParent)
	, m_pParent(pParent)
	, m_icoWnd()
{
}

CPartFileConvertDlg::~CPartFileConvertDlg()
{
	if (m_icoWnd)
		VERIFY(::DestroyIcon(m_icoWnd));
}

void CPartFileConvertDlg::DoDataExchange(CDataExchange *pDX)
{
	CResizableDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_CONV_PB_CURRENT, pb_current);
	DDX_Control(pDX, IDC_JOBLIST, joblist);
}

// CPartFileConvertDlg message handlers

void CPartFileConvertDlg::OnBnClickedOk()
{
	DestroyWindow();
}

void CPartFileConvertDlg::OnCancel()
{
	DestroyWindow();
}

void CPartFileConvertDlg::PostNcDestroy()
{
	CPartFileConvert::ClosedGUI();

	CResizableDialog::PostNcDestroy();
	delete this;
}

void CPartFileConvertDlg::OnAddFolder()
{
	// browse...
	LPMALLOC pMalloc = NULL;
	if (SHGetMalloc(&pMalloc) == NOERROR) {
		// buffer - a place to hold the file system pathname
		TCHAR buffer[MAX_PATH];

		// This struct holds the various options for the dialog
		BROWSEINFO bi;
		bi.hwndOwner = m_hWnd;
		bi.pidlRoot = NULL;
		bi.pszDisplayName = buffer;
		CString title(GetResString(IDS_IMP_SELFOLDER));
		bi.lpszTitle = title.GetBuffer(title.GetLength());
		bi.ulFlags = BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON | BIF_SHAREABLE;
		bi.lpfn = NULL;

		// Now cause the dialog to appear.
		LPITEMIDLIST pidlRoot;
		if ((pidlRoot = SHBrowseForFolder(&bi)) != NULL) {
			int reply = IDNO;

			if (thePrefs.IsExtControlsEnabled())
				reply = LocMessageBox(IDS_IMP_DELSRC, MB_YESNOCANCEL | MB_DEFBUTTON2, 0);

			if (reply != IDCANCEL) {
				bool removesrc = (reply == IDYES);

				//
				// Again, almost undocumented. How to get an ASCII pathname
				// from the LPITEMIDLIST struct.
				// I guess you just have to "know" this stuff.
				//
				if (SHGetPathFromIDList(pidlRoot, buffer)) {
					// Do something with the converted string.
					CPartFileConvert::ScanFolderToAdd(CString(buffer), removesrc);
				}
			}

			// Free the returned item identifier list using the
			// shell's task allocator!Arghhhh.
			pMalloc->Free(pidlRoot);
		}
		pMalloc->Release();
	}
}

void CPartFileConvertDlg::UpdateJobInfo(ConvertJob *job)
{
	if (job == NULL) {
		SetDlgItemText(IDC_CURJOB, GetResString(IDS_FSTAT_WAITING));
		SetDlgItemText(IDC_CONV_PROZENT, _T(""));
		pb_current.SetPos(0);
		SetDlgItemText(IDC_CONV_PB_LABEL, _T(""));
		return;
	}

	// search jobitem in listctrl
	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)job;
	int iItem = joblist.FindItem(&find);
	if (iItem >= 0) {
		joblist.SetItemText(iItem, 0, job->filename.IsEmpty() ? job->folder : job->filename);
		joblist.SetItemText(iItem, 1, CPartFileConvert::GetReturncodeText(job->state));
		CString buffer;
		if (job->size > 0)
			buffer.Format(GetResString(IDS_IMP_SIZE), (LPCTSTR)CastItoXBytes(job->size), (LPCTSTR)CastItoXBytes(job->spaceneeded));
		joblist.SetItemText(iItem, 2, buffer);
		joblist.SetItemText(iItem, 3, job->filehash);
	}// else
	//	AddJob(job);	why???
}

void CPartFileConvertDlg::RemoveJob(ConvertJob *job)
{
	// search jobitem in listctrl
	LVFINDINFO find;
	find.flags = LVFI_PARAM;
	find.lParam = (LPARAM)job;
	int iItem = joblist.FindItem(&find);
	if (iItem >= 0)
		joblist.DeleteItem(iItem);
}

void CPartFileConvertDlg::AddJob(ConvertJob *job)
{
	int ix = joblist.InsertItem(LVIF_TEXT | LVIF_PARAM, joblist.GetItemCount(), job->folder, 0, 0, 0, (LPARAM)job);
	joblist.SetItemText(ix, 1, CPartFileConvert::GetReturncodeText(job->state));
}

void CPartFileConvertDlg::RemoveSel()
{
	if (joblist.GetSelectedCount() == 0)
		return;

	for (POSITION pos = joblist.GetFirstSelectedItemPosition(); pos != NULL;) {
		int index = joblist.GetNextSelectedItem(pos);
		if (index > -1) {
			ConvertJob *job = reinterpret_cast<ConvertJob*>(joblist.GetItemData(index));
			if (job->state != CONV_INPROGRESS) {
				RemoveJob(job); // from list
				CPartFileConvert::RemoveJob(job);
				pos = joblist.GetFirstSelectedItemPosition();
			}
		}
	}
}

void CPartFileConvertDlg::RetrySel()
{
	if (joblist.GetSelectedCount() == 0)
		return;

	for (POSITION pos = joblist.GetFirstSelectedItemPosition(); pos != NULL;) {
		int index = joblist.GetNextSelectedItem(pos);
		if (index > -1) {
			ConvertJob *job = reinterpret_cast<ConvertJob*>(joblist.GetItemData(index));
			if (job->state != CONV_OK && job->state != CONV_INPROGRESS) {
				UpdateJobInfo(job);
				job->state = CONV_QUEUE;
			}
		}
	}

	CPartFileConvert::StartThread();
}

void CPartFileConvertDlg::OnSysColorChange()
{
	pb_current.SetBkColor(::GetSysColor(COLOR_3DFACE));
	CResizableDialog::OnSysColorChange();
}