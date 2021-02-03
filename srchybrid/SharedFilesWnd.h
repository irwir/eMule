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
#pragma once
#include "ResizableLib\ResizableDialog.h"
#include "SharedFilesCtrl.h"
#include "SharedDirsTreeCtrl.h"
#include "SplitterControl.h"
#include "EditDelayed.h"
#include "ListViewWalkerPropertySheet.h"
#include "filedetaildlgstatistics.h"
#include "ED2kLinkDlg.h"
#include "ArchivePreviewDlg.h"
#include "FileInfoDialog.h"
#include "MetaDataDlg.h"

/////////////////////////////////////////////////////////////////////////////////////////////
// CSharedFileDetailsModelessSheet
class CSharedFileDetailsModelessSheet : public CListViewPropertySheet
{
	DECLARE_DYNAMIC(CSharedFileDetailsModelessSheet)

public:
	CSharedFileDetailsModelessSheet();
	virtual	~CSharedFileDetailsModelessSheet() = default;
	CSharedFileDetailsModelessSheet(const CSharedFileDetailsModelessSheet&) = delete;
	CSharedFileDetailsModelessSheet& operator=(const CSharedFileDetailsModelessSheet&) = delete;

	virtual BOOL OnInitDialog();

	void SetFiles(CTypedPtrList<CPtrList, CShareableFile*> &aFiles);
	void Localize();
protected:
	CArchivePreviewDlg			m_wndArchiveInfo;
	CED2kLinkDlg				m_wndFileLink;
	CFileDetailDlgStatistics	m_wndStatistics;
	CFileInfoDialog				m_wndMediaInfo;
	CMetaDataDlg				m_wndMetaData;

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg LRESULT OnDataChanged(WPARAM, LPARAM);
};

class CSharedFilesWnd : public CResizableDialog
{
	DECLARE_DYNAMIC(CSharedFilesWnd)

	enum
	{
		IDD = IDD_FILES
	};

public:
	explicit CSharedFilesWnd(CWnd *pParent = NULL);   // standard constructor
	virtual	~CSharedFilesWnd();

	virtual BOOL OnInitDialog();
	void Localize();
	void SetToolTipsDelay(DWORD dwDelay);
	void Reload(bool bForceTreeReload = false);
	uint32	GetFilterColumn() const				{ return m_nFilterColumn; }
	void OnVolumesChanged()						{ m_ctlSharedDirTree.OnVolumesChanged(); }
	void OnSingleFileShareStatusChanged()		{ m_ctlSharedDirTree.FileSystemTreeUpdateBoldState(NULL); }
	void ShowSelectedFilesDetails(bool bForce = false);
	void ShowDetailsPanel(bool bShow);

	CSharedFilesCtrl sharedfilesctrl;
	CStringArray m_astrFilter;
	CSharedDirsTreeCtrl m_ctlSharedDirTree;

private:

	HICON icon_files;
	CSplitterControl m_wndSplitter;
	CEditDelayed	m_ctlFilter;
	CHeaderCtrl		m_ctlSharedListHeader;
	uint32			m_nFilterColumn;
	bool			m_bDetailsVisible;
	CSharedFileDetailsModelessSheet	m_dlgDetails;

protected:
	void SetAllIcons();
	void DoResize(int iDelta);

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual LRESULT DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor);
	afx_msg LRESULT OnChangeFilter(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedReloadSharedFiles();
	afx_msg void OnLvnItemActivateSharedFiles(LPNMHDR, LRESULT*);
	afx_msg void OnNmClickSharedFiles(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnStnDblClickFilesIco();
	afx_msg void OnSysColorChange();
	afx_msg void OnTvnSelChangedSharedDirsTree(LPNMHDR, LRESULT *pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT);
	afx_msg void OnBnClickedSfHideshowdetails();
	afx_msg void OnLvnItemchangedSflist(LPNMHDR, LRESULT *pResult);
};