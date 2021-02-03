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
#include "MuleListCtrl.h"
#include "TitleMenu.h"
#include "ListCtrlItemWalk.h"

class CSharedFileList;
class CKnownFile;
class CShareableFile;
class CDirectoryItem;
class CToolTipCtrlX;

class CSharedFilesCtrl : public CMuleListCtrl, public CListCtrlItemWalk
{
	DECLARE_DYNAMIC(CSharedFilesCtrl)
	friend class CSharedDirsTreeCtrl;

public:
	class CShareDropTarget: public COleDropTarget
	{
	public:
		CShareDropTarget();
		virtual	~CShareDropTarget();
		void	SetParent(CSharedFilesCtrl *pParent)	{ m_pParent = pParent; }

		DROPEFFECT	OnDragEnter(CWnd *pWnd, COleDataObject *pDataObject, DWORD dwKeyState, CPoint point);
		DROPEFFECT	OnDragOver(CWnd*, COleDataObject *pDataObject, DWORD, CPoint point);
		BOOL		OnDrop(CWnd*, COleDataObject *pDataObject, DROPEFFECT dropEffect, CPoint point);
		void		OnDragLeave(CWnd*);

	protected:
		IDropTargetHelper	*m_piDropHelper;
		bool				m_bUseDnDHelper;
//		BOOL ReadHdropData (COleDataObject *pDataObject);
		CSharedFilesCtrl	*m_pParent;
	};

	CSharedFilesCtrl();
	virtual	~CSharedFilesCtrl();

	void	Init();
	void	SetToolTipsDelay(DWORD dwDelay);
	void	CreateMenus();
	void	ReloadFileList();
	void	AddFile(const CShareableFile *file);
	void	RemoveFile(const CShareableFile *file, bool bDeletedFromDisk);
	void	UpdateFile(const CShareableFile *file, bool bUpdateFileSummary = true);
	void	Localize();
	void	ShowFilesCount();
	void	ShowComments(CShareableFile *file);
	void	SetAICHHashing(INT_PTR nVal)				{ nAICHHashing = nVal; }
	void	SetDirectoryFilter(CDirectoryItem *pNewFilter, bool bRefresh = true);

protected:
	CTitleMenu		m_SharedFilesMenu;
	CTitleMenu		m_CollectionsMenu;
	CMenu			m_PrioMenu;
	bool			m_aSortBySecondValue[4];
	CImageList		m_ImageList;
	CDirectoryItem	*m_pDirectoryFilter;
	volatile INT_PTR	nAICHHashing;
	CToolTipCtrlX	*m_pToolTip;
	CTypedPtrList<CPtrList, CShareableFile*>	liTempShareableFilesInDir;
	CShareableFile	*m_pHighlightedItem;
	CShareDropTarget m_ShareDropTarget;

	static int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	void OpenFile(const CShareableFile *file);
	void ShowFileDialog(CTypedPtrList<CPtrList, CShareableFile*> &aFiles, UINT uInvokePage = 0);
	void SetAllIcons();
	int FindFile(const CShareableFile *pFile);
	CString GetItemDisplayText(const CShareableFile *file, int iSubItem) const;
	bool IsFilteredItem(const CShareableFile *pKnownFile) const;
	bool IsSharedInKad(const CKnownFile *file) const;
	void AddShareableFiles(const CString &strFromDir);
	void CheckBoxClicked(int iItem);
	bool CheckBoxesEnabled() const;

	virtual BOOL OnCommand(WPARAM wParam, LPARAM);
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnNmDblClk(LPNMHDR, LRESULT *pResult);
	afx_msg void OnSysColorChange();
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnNMClick(LPNMHDR pNMHDR, LRESULT *pResult);
};