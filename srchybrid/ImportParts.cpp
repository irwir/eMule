#include "stdafx.h"
#include "resource.h"
#include "ImportParts.h"
#include "emule.h"
#include "emuleDlg.h"
#include "Log.h"
#include "Opcodes.h"
#include "PartFile.h"
#include "SHAHashSet.h"
#include "SharedFileList.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif
/*
bool CKnownFile::ImportParts()
{
	// General idea from xmrb's CKnownFile::ImportParts()
	// Unlike xmrb's version which scans entire file designated for import and then tries
	// to match each PARTSIZE bytes with all parts of partfile, my version assumes that
	// in file you're importing all parts stay on same place as they should be in partfile
	// (for example you're importing damaged version of file to recover some parts from ED2K)
	// That way it works much faster and almost always it is what expected from this
	// function. --Rowaa[SR13].
	// CHANGE BY SIROB, Only compute missing full chunk part
	if (!IsPartFile()) {
		LogError(LOG_STATUSBAR, GetResString(IDS_IMPORTPARTS_ERR_ALREADYCOMPLETE));
		return false;
	}

	CPartFile *partfile = (CPartFile*)this;
	if (partfile->GetFileOp() == PFOP_IMPORTPARTS) {
		partfile->SetFileOp(PFOP_NONE); //cancel import
		return false;
	}

	CFileDialog dlg(true, NULL, NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY);
	if (dlg.DoModal() != IDOK)
		return false;
	CString pathName = dlg.GetPathName();

	CAddFileThread *addfilethread = (CAddFileThread*)AfxBeginThread(RUNTIME_CLASS(CAddFileThread), THREAD_PRIORITY_LOWEST, 0, CREATE_SUSPENDED);
	if (addfilethread) {
		partfile->SetFileOpProgress(0);
		addfilethread->SetValues(theApp.sharedfiles, partfile->GetPath(), partfile->m_hpartfile.GetFileName(), _T(""), partfile);
		partfile->SetFileOp(addfilethread->SetPartToImport(pathName) ? PFOP_IMPORTPARTS : PFOP_HASHING);
		addfilethread->ResumeThread();
	}
	return true;
}

// Special case for SR13-ImportParts
uint16 CAddFileThread::SetPartToImport(LPCTSTR import)
{
	if (m_partfile->GetFilePath() == import)
		return 0;

	m_strImport = import;

	for (uint16 i = 0; i < m_partfile->GetPartCount(); ++i)
		if (!m_partfile->IsComplete(i * PARTSIZE, (i + 1) * PARTSIZE - 1, false))
			m_PartsToImport.Add(i);

	return (uint16)m_PartsToImport.GetSize();
}

bool CAddFileThread::ImportParts()
{
	uint16 partsuccess = 0;

	CFile f;
	if (!f.Open(m_strImport, CFile::modeRead | CFile::shareDenyNone)) {
		LogError(LOG_STATUSBAR, GetResString(IDS_IMPORTPARTS_ERR_CANTOPENFILE), (LPCTSTR)m_strImport);
		return false;
	}

	CString strFilePath;
	_tmakepath(strFilePath.GetBuffer(MAX_PATH), NULL, m_strDirectory, m_strFilename, NULL);
	strFilePath.ReleaseBuffer();

	Log(LOG_STATUSBAR, GetResString(IDS_IMPORTPARTS_IMPORTSTART), m_PartsToImport.GetSize(), (LPCTSTR)strFilePath);

	uint64 fileSize = f.GetLength();
	CKnownFile *kfcall = new CKnownFile;
	BYTE *partData = NULL;
	for (INT_PTR i = 0; i < m_PartsToImport.GetSize(); ++i) {
		uint16 partnumber = m_PartsToImport[i];
		if (PARTSIZE * partnumber > fileSize)
			break;

		try {
			uint32 partSize;
			try {
				if (partData == NULL)
					partData = new BYTE[PARTSIZE];
				CSingleLock sLock1(&theApp.hashing_mut, TRUE);	//SafeHash - wait a current hashing process end before read the chunk
				f.Seek((LONGLONG)PARTSIZE * partnumber, CFile::begin);
				partSize = f.Read(partData, PARTSIZE);
			} catch (...) {
				LogWarning(LOG_STATUSBAR, _T("Part %i: Not accessible (You may have a bad cluster on your harddisk)."), (int)partnumber);
				continue;
			}
			uchar hash[16];
			kfcall->CreateHash(partData, partSize, hash);
			ImportPart_Struct *importpart = new ImportPart_Struct;
			importpart->start = partnumber * PARTSIZE;
			importpart->end = importpart->start + partSize - 1;
			importpart->data = partData;
			VERIFY(PostMessage(theApp.emuledlg->m_hWnd, TM_IMPORTPART, (WPARAM)importpart, (LPARAM)m_partfile));
			partData = NULL; // Delete will happen in async write thread.
			//Log(LOG_STATUSBAR, GetResString(IDS_IMPORTPARTS_PARTIMPORTEDGOOD), partnumber);
			++partsuccess;

			if (theApp.IsRunning()) {
				WPARAM uProgress = (WPARAM)(i * 100 / m_PartsToImport.GetSize());
				VERIFY(PostMessage(theApp.emuledlg->GetSafeHwnd(), TM_FILEOPPROGRESS, uProgress, (LPARAM)m_partfile));
				Sleep(100); // sleep very shortly to give time to write (or else mem grows!)
			}

			if (!theApp.IsRunning() || partSize != PARTSIZE || m_partfile->GetFileOp() != PFOP_IMPORTPARTS)
				break;
		} catch (...) {
			//delete[] partData;
			//partData = NULL;
			//continue;
		}
	}
	f.Close();
	delete[] partData;
	delete kfcall;

	try {
		bool importaborted = m_partfile->GetFileOp() == PFOP_NONE || !theApp.IsRunning();
		if (m_partfile->GetFileOp() == PFOP_IMPORTPARTS)
			m_partfile->SetFileOp(PFOP_NONE);
		LPCTSTR p = importaborted ? _T("aborted") : _T("completed");
		Log(LOG_STATUSBAR, _T("Import %s. %i parts imported to %s."), p, (int)partsuccess, (LPCTSTR)m_strFilename);
	} catch (...) {
		//This could happen if we delete the part instance
	}
	return true;
}*/