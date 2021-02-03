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
#include <map>
#include "ListCtrlItemWalk.h"

#define COLLAPSE_ONLY	0
#define EXPAND_ONLY		1
#define EXPAND_COLLAPSE	2

// Forward declaration
class CPartFile;
class CUpDownClient;
class CDownloadListCtrl;
class CToolTipCtrlX;


///////////////////////////////////////////////////////////////////////////////
// CtrlItem_Struct

enum ItemType
{
	INVALID_TYPE = -1,
	FILE_TYPE = 1,
	AVAILABLE_SOURCE = 2,
	UNAVAILABLE_SOURCE = 3
};

class CtrlItem_Struct : public CObject
{
	DECLARE_DYNAMIC(CtrlItem_Struct)

public:
	~CtrlItem_Struct()							{ status.DeleteObject(); }

	ItemType		type;
	CPartFile		*owner;
	void			*value; // could be either CPartFile or CUpDownClient
	CtrlItem_Struct	*parent;
	DWORD			dwUpdated;
	CBitmap			status;
};


///////////////////////////////////////////////////////////////////////////////
// CDownloadListListCtrlItemWalk

class CDownloadListListCtrlItemWalk : public CListCtrlItemWalk
{
public:
	explicit CDownloadListListCtrlItemWalk(CDownloadListCtrl *pListCtrl);

	virtual CObject* GetNextSelectableItem();
	virtual CObject* GetPrevSelectableItem();

	void SetItemType(ItemType eItemType)		{ m_eItemType = eItemType; }

protected:
	CDownloadListCtrl *m_pDownloadListCtrl;
	ItemType m_eItemType;
};


///////////////////////////////////////////////////////////////////////////////
// CDownloadListCtrl

class CDownloadListCtrl : public CMuleListCtrl, public CDownloadListListCtrlItemWalk
{
	DECLARE_DYNAMIC(CDownloadListCtrl)
	friend class CDownloadListListCtrlItemWalk;

public:
	CDownloadListCtrl();
	virtual	~CDownloadListCtrl();
	CDownloadListCtrl(const CDownloadListCtrl&) = delete;
	CDownloadListCtrl& operator=(const CDownloadListCtrl&) = delete;

	UINT	curTab;

	void	UpdateItem(void *toupdate);
	void	Init();
	void	AddFile(CPartFile *toadd);
	void	AddSource(CPartFile *owner, CUpDownClient *source, bool notavailable);
	void	RemoveSource(CUpDownClient *source, CPartFile *owner);
	bool	RemoveFile(const CPartFile *toremove);
	void	ClearCompleted(int incat = -2);
	void	ClearCompleted(const CPartFile *pFile);
	void	SetStyle();
	void	CreateMenus();
	void	Localize();
	void	ShowFilesCount();
	void	ChangeCategory(int newsel);
	CString getTextList();
	void	ShowSelectedFileDetails();
	void	HideFile(CPartFile *tohide);
	void	ShowFile(CPartFile *toshow);
	void	ExpandCollapseItem(int iItem, int iAction, bool bCollapseSource = false);
	void	HideSources(CPartFile *toCollapse);
	void	GetDisplayedFiles(CArray<CPartFile*, CPartFile*> *list);
	void	MoveCompletedfilesCat(UINT from, UINT to);
	int		GetCompleteDownloads(int cat, int &total);
	void	UpdateCurrentCategoryView();
	void	UpdateCurrentCategoryView(CPartFile *thisfile);
	CImageList* CreateDragImage(int iItem, LPPOINT lpPoint);
	void	FillCatsMenu(CMenu &rCatsMenu, int iFilesInCats = -1);
	CTitleMenu* GetPrioMenu();
	float	GetFinishedSize();
	bool	ReportAvailableCommands(CList<int> &liAvailableCommands);

protected:
	CImageList  m_ImageList;
	CTitleMenu	m_PrioMenu;
	CTitleMenu	m_FileMenu;
	CTitleMenu	m_PreviewMenu;
	CMenu		m_SourcesMenu;
	bool		m_bRemainSort;
	typedef std::pair<void*, CtrlItem_Struct*> ListItemsPair;
	typedef std::multimap<void*, CtrlItem_Struct*> ListItems;
	ListItems	m_ListItems;
	CFont		m_fontBold; // may contain a locally created bold font
	CFont		*m_pFontBold;// points to the bold font which is to be used (may be the locally created or the default bold font)
	CToolTipCtrlX *m_tooltip;
	uint32		m_dwLastAvailableCommandsCheck;
	bool		m_availableCommandsDirty;

	void ShowFileDialog(UINT uInvokePage);
	void ShowClientDialog(CUpDownClient *pClient);
	void SetAllIcons();
	void DrawFileItem(CDC *dc, int nColumn, LPCRECT lpRect, UINT uDrawTextAlignment, CtrlItem_Struct *pCtrlItem);
	void DrawSourceItem(CDC *dc, int nColumn, LPCRECT lpRect, UINT uDrawTextAlignment, CtrlItem_Struct *pCtrlItem);
	int GetFilesCountInCurCat();
	CString GetFileItemDisplayText(const CPartFile *lpPartFile, int iSubItem);
	CString GetSourceItemDisplayText(const CtrlItem_Struct *pCtrlItem, int iSubItem);

	static int CALLBACK SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	static int Compare(const CPartFile *file1, const CPartFile *file2, LPARAM lParamSort);
	static int Compare(const CUpDownClient *client1, const CUpDownClient *client2, LPARAM lParamSort);

	virtual BOOL OnCommand(WPARAM wParam, LPARAM);
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnContextMenu(CWnd*, CPoint point);
	afx_msg void OnListModified(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnGetDispInfo(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnGetInfoTip(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnItemActivate(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void OnNmDblClk(LPNMHDR, LRESULT *pResult);
	afx_msg void OnSysColorChange();
};