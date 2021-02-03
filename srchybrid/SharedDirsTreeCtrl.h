//this file is part of eMule
//Copyright (C)2002-2008 Merkur ( devs@emule-project.net / http://www.emule-project.net )
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
#include "TitleMenu.h"

enum ESpecialDirectoryItems
{
	SDI_NO = 0,
	SDI_ALL,				// "All Shared Files" node
	SDI_INCOMING,			// "Incoming Files" node
	SDI_TEMP,				// "Incomplete Files" node
	SDI_DIRECTORY,			// "Shared Directories" node
	SDI_CATINCOMING,		// Category subnode in the "Incoming Files" node
	SDI_UNSHAREDDIRECTORY,
	SDI_FILESYSTEMPARENT	// "All Directories" (the file system)
};

class CSharedFilesCtrl;
class CKnownFile;
class CShareableFile;

//**********************************************************************************
// CDirectoryItem

class CDirectoryItem{
public:
	explicit CDirectoryItem(const CString &strFullPath, HTREEITEM htItem = TVI_ROOT, ESpecialDirectoryItems eItemType = SDI_NO, int nCatFilter = -1);
	~CDirectoryItem();
	CDirectoryItem*	CloneContent()						{ return new CDirectoryItem(m_strFullPath, 0, m_eItemType, m_nCatFilter); }
	HTREEITEM		FindItem(CDirectoryItem *pContentToFind) const;

	CString		m_strFullPath;
	HTREEITEM	m_htItem;
	CList<CDirectoryItem*, CDirectoryItem*> liSubDirectories;
	int			m_nCatFilter;
	ESpecialDirectoryItems m_eItemType;
};

//**********************************************************************************
// CSharedDirsTreeCtrl

class CSharedDirsTreeCtrl : public CTreeCtrl
{
	DECLARE_DYNAMIC(CSharedDirsTreeCtrl)

public:
	CSharedDirsTreeCtrl();
	virtual	~CSharedDirsTreeCtrl();

	void			Initialize(CSharedFilesCtrl *pSharedFilesCtrl);
	void			SetAllIcons();

	CDirectoryItem* GetSelectedFilter() const;
	bool			IsCreatingTree() const				{ return m_bCreatingTree; };
	void			Localize();
	void			EditSharedDirectories(const CDirectoryItem *pDir, bool bAdd, bool bSubDirectories);
	void			Reload(bool bForce = false);
	void			OnVolumesChanged();
	void			FileSystemTreeUpdateBoldState(const CDirectoryItem *pDir = NULL);
	bool			ShowFileSystemDirectory(const CString &strDir);
	bool			ShowSharedDirectory(const CString &strDir);
	void			ShowAllSharedFiles();

protected:
	virtual BOOL	OnCommand(WPARAM wParam, LPARAM);
	void			CreateMenus();
	void			ShowFileDialog(CTypedPtrList<CPtrList, CShareableFile*> &aFiles, UINT uInvokePage = 0);
	void			DeleteChildItems(CDirectoryItem *pParent);
	void			AddSharedDirectory(const CString &strDir, bool bSubDirectories);
	void			RemoveSharedDirectory(const CString &strDir, bool bSubDirectories);
	void			RemoveAllSharedDirectories();
	int				AddSystemIcon(HICON hIcon, int nSystemListPos);
	void			FetchSharedDirsList();

	DECLARE_MESSAGE_MAP()
	afx_msg void	OnSysColorChange();
	afx_msg void	OnContextMenu(CWnd*, CPoint point);
	afx_msg	void	OnRButtonDown(UINT, CPoint point);
	afx_msg	void	OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void	OnTvnItemexpanding(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void	OnTvnGetdispinfo(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void	OnTvnBeginDrag(LPNMHDR pNMHDR, LRESULT *pResult);
	afx_msg void	OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void	OnCancelMode();

	CTitleMenu		m_SharedFilesMenu;
	CTitleMenu		m_ShareDirsMenu;
	CMenu			m_PrioMenu;
	CDirectoryItem	*m_pRootDirectoryItem;
	CDirectoryItem	*m_pRootUnsharedDirectries;
	CDirectoryItem	*m_pDraggingItem;
	CSharedFilesCtrl *m_pSharedFilesCtrl;
	CStringList		m_strliSharedDirs;
	CStringList		m_strliCatIncomingDirs;
	CImageList		m_imlTree;
	bool			m_bFileSystemRootDirty;

private:
	void	InitalizeStandardItems();

	void	FileSystemTreeCreateTree();
	void	FileSystemTreeAddChildItem(CDirectoryItem *pRoot, const CString &strText, bool bTopLevel);
	bool	FileSystemTreeHasSubdirectories(const CString &strDir);
	bool	FileSystemTreeHasSharedSubdirectory(const CString &strDir, bool bOrFiles);
	void	FileSystemTreeAddSubdirectories(CDirectoryItem *pRoot);
	bool	FileSystemTreeIsShared(const CString &strDir);

	void	FileSystemTreeUpdateShareState(const CDirectoryItem *pDir = NULL);
	void	FileSystemTreeSetShareState(const CDirectoryItem *pDir, bool bSubDirectories);
//	void	FilterTreeAddSharedDirectory(CDirectoryItem *pDir, bool bRefresh);
	void	FilterTreeAddSubDirectories(CDirectoryItem *pDirectory, const CStringList &liDirs, int nLevel, bool &rbShowWarning, bool bParentAccessible);
	bool	FilterTreeIsSubDirectory(const CString &strDir, const CString &strRoot, const CStringList &liDirs);
	void	FilterTreeReloadTree();

	bool	m_bCreatingTree;
	bool	m_bUseIcons;
	CMap<int, int, int, int> m_mapSystemIcons;
};