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
#include <sys/stat.h>
#include <share.h>
#include "emule.h"
#include "Preview.h"
#include "Preferences.h"
#include "PartFile.h"
#include "MenuCmds.h"
#include "opcodes.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CPreviewApps thePreviewApps;

///////////////////////////////////////////////////////////////////////////////
// CPreviewThread

IMPLEMENT_DYNCREATE(CPreviewThread, CWinThread)

BEGIN_MESSAGE_MAP(CPreviewThread, CWinThread)
END_MESSAGE_MAP()

CPreviewThread::CPreviewThread()
	: m_pPartfile()
{
}

BOOL CPreviewThread::InitInstance()
{
	DbgSetThreadName("PartFilePreview");
	InitThreadLocale();
	return TRUE;
}

BOOL CPreviewThread::Run()
{
	ASSERT(m_pPartfile);
	CFile srcFile;
	if (!srcFile.Open(m_pPartfile->GetFilePath(), CFile::modeRead | CFile::shareDenyNone))
		return FALSE;
	try {
		uint64 nSize = m_pPartfile->GetFileSize();
		const CString strExtension(_tcsrchr(m_pPartfile->GetFileName(), _T('.')));
		CString strPreviewName;
		strPreviewName.Format(_T("%s%s_preview%s"), (LPCTSTR)m_pPartfile->GetTempPath(), (LPCTSTR)m_pPartfile->GetFileName().Left(5), (LPCTSTR)strExtension);
		bool bFullSized = (strExtension.CompareNoCase(_T(".mpg")) && strExtension.CompareNoCase(_T(".mpeg")));
		CFile destFile;
		if (!destFile.Open(strPreviewName, CFile::modeWrite | CFile::shareDenyWrite | CFile::modeCreate))
			return FALSE;
		srcFile.SeekToBegin();
		if (bFullSized)
			destFile.SetLength(nSize);
		destFile.SeekToBegin();
		BYTE abyBuffer[4096];
		uint32 nRead;
		while (destFile.GetPosition() + (sizeof abyBuffer) < PARTSIZE * 2) {
			nRead = srcFile.Read(abyBuffer, sizeof abyBuffer);
			destFile.Write(abyBuffer, nRead);
		}
		srcFile.Seek(-(LONGLONG)(PARTSIZE * 2), CFile::end);
		uint32 nToGo = PARTSIZE * 2;
		if (bFullSized)
			destFile.Seek(-(LONGLONG)(PARTSIZE * 2), CFile::end);
		do {
			nRead = (nToGo - (sizeof abyBuffer) < 1) ? nToGo : sizeof abyBuffer;
			nToGo -= nRead;
			nRead = srcFile.Read(abyBuffer, sizeof abyBuffer);
			destFile.Write(abyBuffer, nRead);
		} while (nToGo);
		destFile.Close();
		srcFile.Close();
		m_pPartfile->m_bPreviewing = false;

		SHELLEXECUTEINFO SE = {};
		SE.fMask = SEE_MASK_NOCLOSEPROCESS;

		if (m_strCommand.IsEmpty()) {
			SE.lpVerb = NULL;	// use the default verb or the open verb for the document
			SE.lpFile = strPreviewName;
		} else {
			SE.lpVerb = _T("open");	// "open" the specified video player

			// get directory of video player application
			CString strCommandDir = m_strCommand;
			int iPos = strCommandDir.ReverseFind(_T('\\'));
			if (iPos < 0)
				strCommandDir.Empty();
			else
				strCommandDir.Truncate(iPos + 1);
			PathRemoveBackslash(strCommandDir.GetBuffer());
			strCommandDir.ReleaseBuffer();

			CString strArgs(m_strCommandArgs);
			if (!strArgs.IsEmpty())
				strArgs += _T(' ');
			if (strPreviewName.Find(_T(' ')) >= 0)
				strArgs.AppendFormat(_T("\"%s\""), (LPCTSTR)strPreviewName);
			else
				strArgs += strPreviewName;

			CString strCommand = m_strCommand;
			ExpandEnvironmentStrings(strCommand);
			ExpandEnvironmentStrings(strArgs);
			ExpandEnvironmentStrings(strCommandDir);
			SE.lpFile = strCommand;
			SE.lpParameters = strArgs;
			SE.lpDirectory = strCommandDir;
		}
		SE.nShow = SW_SHOW;
		SE.cbSize = (DWORD)sizeof SE;
		ShellExecuteEx(&SE);
		if (SE.hProcess) {
			::WaitForSingleObject(SE.hProcess, INFINITE);
			::CloseHandle(SE.hProcess);
		}
		CFile::Remove(strPreviewName);
	} catch (CFileException *error) {
		m_pPartfile->m_bPreviewing = false;
		error->Delete();
	}
	return TRUE;
}

void CPreviewThread::SetValues(CPartFile *pPartFile, LPCTSTR pszCommand, LPCTSTR pszCommandArgs)
{
	m_pPartfile = pPartFile;
	m_strCommand = pszCommand;
	m_strCommandArgs = pszCommandArgs;
}


///////////////////////////////////////////////////////////////////////////////
// CPreviewApps

CPreviewApps::CPreviewApps()
	: m_tDefAppsFileLastModified()
	, m_pLastCheckedPartFile()
	, m_pLastPartFileApp()
{
}

CString CPreviewApps::GetDefaultAppsFile()
{
	return thePrefs.GetMuleDirectory(EMULE_CONFIGDIR) + _T("PreviewApps.dat");
}

void CPreviewApps::RemoveAllApps()
{
	m_aApps.RemoveAll();
	m_tDefAppsFileLastModified = 0;
}

INT_PTR CPreviewApps::ReadAllApps()
{
	RemoveAllApps();

	const CString &strFilePath(GetDefaultAppsFile());
	FILE *readFile = _tfsopen(strFilePath, _T("r"), _SH_DENYWR);
	if (readFile != NULL) {
		CString sbuffer;
		while (!feof(readFile)) {
			TCHAR buffer[1024];
			if (_fgetts(buffer, _countof(buffer), readFile) == NULL)
				break;
			sbuffer = buffer;
			sbuffer.TrimRight(_T("\r\n\t"));

			// ignore comments & too short lines
			if (buffer[0] == _T('#') || buffer[0] == _T('/') || sbuffer.GetLength() < 5)
				continue;

			int iPos = 0;
			CString strTitle(sbuffer.Tokenize(_T("="), iPos));
			if (!strTitle.Trim().IsEmpty()) {
				CString strCommand(sbuffer.Tokenize(_T(";"), iPos));
				if (strCommand.Trim().IsEmpty())
					continue;

				LPCTSTR pszCommandArgs = PathGetArgs((LPCTSTR)strCommand);
				if (pszCommandArgs)
					strCommand.Truncate((int)(pszCommandArgs - (LPCTSTR)strCommand));
				if (strCommand.Trim(_T(" \t\"")).IsEmpty())
					continue;

				uint64 ullMinCompletedSize = 0;
				uint64 ullMinStartOfFile = 0;
				CStringArray astrExtensions;
				for (; iPos >= 0;) {
					const CString &strParams(sbuffer.Tokenize(_T(";"), iPos));
					if (strParams.IsEmpty())
						break;
					int iPosParam = 0;
					CString strId(strParams.Tokenize(_T("="), iPosParam));
					if (strId.IsEmpty())
						continue;
					const CString &strValue(strParams.Tokenize(_T("="), iPosParam));
					if (strValue.IsEmpty())
						continue;
					strId.MakeLower();
					if (strId == _T("ext"))
						astrExtensions.Add((strValue[0] == _T('.')) ? strValue : _T('.') + strValue);
					else if (strId == _T("minsize"))
						(void)_stscanf(strValue, _T("%I64u"), &ullMinCompletedSize);
					else if (strId == _T("minstart"))
						(void)_stscanf(strValue, _T("%I64u"), &ullMinStartOfFile);
				}

				SPreviewApp svc;
				svc.strTitle = strTitle;
				svc.strCommand = strCommand;
				svc.strCommandArgs = CString(pszCommandArgs).Trim();
				svc.astrExtensions.Append(astrExtensions);
				svc.ullMinCompletedSize = ullMinCompletedSize;
				svc.ullMinStartOfFile = ullMinStartOfFile;
				m_aApps.Add(svc);
			}
		}
		fclose(readFile);

		struct _stat64 st;
		if (statUTC(strFilePath, st) == 0)
			m_tDefAppsFileLastModified = (time_t)st.st_mtime;
	}

	return m_aApps.GetCount();
}

void CPreviewApps::UpdateApps()
{
	struct _stat64 st;
	if (m_aApps.IsEmpty() || (statUTC(GetDefaultAppsFile(), st) == 0 && st.st_mtime > m_tDefAppsFileLastModified))
		ReadAllApps();
}

int CPreviewApps::GetAllMenuEntries(CMenu &rMenu, const CPartFile *file)
{
	UpdateApps();
	int count = min((int)m_aApps.GetCount(), MP_PREVIEW_APP_MAX - MP_PREVIEW_APP_MIN + 1);
	bool bEnabled = (file && (uint64)file->GetCompletedSize() >= 16ull * 1024);
	for (int i = 0; i < count; ++i)
		rMenu.AppendMenu(MF_STRING | (bEnabled ? MF_ENABLED : MF_GRAYED), MP_PREVIEW_APP_MIN + i, m_aApps[i].strTitle);
	return count;
}

void CPreviewApps::RunApp(CPartFile *file, UINT uMenuID) const
{
	const SPreviewApp &svc(m_aApps[uMenuID - MP_PREVIEW_APP_MIN]);
	ExecutePartFile(file, svc.strCommand, svc.strCommandArgs);
}

void ExecutePartFile(CPartFile *file, LPCTSTR pszCommand, LPCTSTR pszCommandArgs)
{
	if (!CheckFileOpen(file->GetFilePath(), file->GetFileName()))
		return;

	// get directory of video player application
	CString strCommandDir(pszCommand);
	int iPos = strCommandDir.ReverseFind(_T('\\'));
	if (iPos < 0)
		strCommandDir.Empty();
	else
		strCommandDir.Truncate(iPos + 1);
	PathRemoveBackslash(strCommandDir.GetBuffer());
	strCommandDir.ReleaseBuffer();

	CString strArgs(pszCommandArgs);
	if (!strArgs.IsEmpty())
		strArgs += _T(' ');

	const CString &strQuotedPartFilePath(file->GetFilePath());
	// if the path contains spaces, quote the entire path
	if (strQuotedPartFilePath.Find(_T(' ')) >= 0)
		strArgs.AppendFormat(_T("\"%s\""), (LPCTSTR)strQuotedPartFilePath);
	else
		strArgs += strQuotedPartFilePath;

	file->FlushBuffer(true);

	CString strCommand(pszCommand);
	ExpandEnvironmentStrings(strCommand);
	ExpandEnvironmentStrings(strArgs);
	ExpandEnvironmentStrings(strCommandDir);

	LPCTSTR pszVerb;

	// Backward compatibility with old 'preview' command (when no preview application is specified):
	//	"ShellExecute(NULL, NULL, strPartFilePath, NULL, NULL, SW_SHOWNORMAL);"
	if (strCommand.IsEmpty()) {
		strCommand = strArgs;
		strArgs.Empty();
		strCommandDir.Empty();
		pszVerb = NULL;
	} else
		pszVerb = _T("open");

	TRACE(_T("Starting preview application:\n"));
	TRACE(_T("  Command =%s\n"), (LPCTSTR)strCommand);
	TRACE(_T("  Args    =%s\n"), (LPCTSTR)strArgs);
	TRACE(_T("  Dir     =%s\n"), (LPCTSTR)strCommandDir);
	DWORD dwError = (DWORD)ShellExecute(NULL, pszVerb, strCommand, strArgs.IsEmpty() ? NULL : (LPCTSTR)strArgs, strCommandDir.IsEmpty() ? NULL : (LPCTSTR)strCommandDir, SW_SHOWNORMAL);
	if (dwError <= 32) {
		//
		// Unfortunately, Windows may already have shown an error dialog which tells
		// the user about the failed 'ShellExecute' call. *BUT* that error dialog is not
		// shown in each case!
		//
		// Examples:
		//	 -	Specifying an executable which does not exist (e.g. APP.EXE)
		//		-> (Error 2) -> No error is shown.
		//
		//   -	Executing a document (verb "open") which has an unregistered extension
		//		-> (Error 31) -> Error is shown.
		//
		// I'm not sure whether this behaviour (showing an error dialog in cases of some
		// specific errors) is handled the same way in all Windows version -> therefore I
		// decide to always show an application specific error dialog!
		//
		CString strMsg;
		strMsg.Format(_T("Failed to execute: %s %s"), (LPCTSTR)strCommand, (LPCTSTR)strArgs);

		LPCTSTR strSysErrMsg = GetShellExecuteErrMsg(dwError);
		if (*strSysErrMsg)
			strMsg.AppendFormat(_T("\r\n\r\n%s"), strSysErrMsg);

		AfxMessageBox(strMsg, MB_ICONSTOP);
	}
}

int CPreviewApps::GetPreviewApp(const CPartFile *file)
{
	LPCTSTR pszExt = PathFindExtension(file->GetFileName());
	if (*pszExt) {
		UpdateApps();
		for (INT_PTR i = m_aApps.GetCount(); --i >= 0;) {
			const SPreviewApp &rApp(m_aApps[i]);
			for (INT_PTR j = rApp.astrExtensions.GetCount(); --j >= 0;)
				if (rApp.astrExtensions[j].CompareNoCase(pszExt) == 0)
					return (int)i;
		}
	}
	return -1;
}

CPreviewApps::ECanPreviewRes CPreviewApps::CanPreview(const CPartFile *file)
{
	int iApp = GetPreviewApp(file);
	if (iApp == -1)
		return NotHandled;

	const SPreviewApp *pApp = &m_aApps[iApp];
	if (pApp->ullMinCompletedSize != 0) {
		if (file->GetCompletedSize() < pApp->ullMinCompletedSize)
			return No;
	}

	if (pApp->ullMinStartOfFile != 0)
		if (!file->IsComplete(0, pApp->ullMinStartOfFile, false))
			return No;

	return Yes;
}

bool CPreviewApps::Preview(CPartFile *file)
{
	int iApp = GetPreviewApp(file);
	if (iApp == -1)
		return false;
	RunApp(file, MP_PREVIEW_APP_MIN + iApp);
	return true;
}