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
#include "stdafx.h"
#include "emule.h"
#include "emuledlg.h"
#include "CollectionViewDialog.h"
#include "Collection.h"
#include "CollectionFile.h"
#include "DownloadQueue.h"
#include "TransferDlg.h"
#include "CatDialog.h"
#include "SearchDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define	PREF_INI_SECTION	_T("CollectionViewDlg")

IMPLEMENT_DYNAMIC(CCollectionViewDialog, CDialog)

BEGIN_MESSAGE_MAP(CCollectionViewDialog, CResizableDialog)
	ON_BN_CLICKED(IDC_VCOLL_CLOSE, OnBnClickedOk)
	ON_BN_CLICKED(IDC_VIEWCOLLECTIONDL, OnBnClickedViewCollection)
	ON_NOTIFY(NM_DBLCLK, IDC_COLLECTIONVEWLIST, OnNmDblClkCollectionList)
END_MESSAGE_MAP()

CCollectionViewDialog::CCollectionViewDialog(CWnd *pParent /*=NULL*/)
	: CResizableDialog(CCollectionViewDialog::IDD, pParent)
	, m_pCollection()
	, m_icoWnd()
	, m_icoColl()
{
}

CCollectionViewDialog::~CCollectionViewDialog()
{
	if (m_icoWnd)
		VERIFY(::DestroyIcon(m_icoWnd));
	if (m_icoColl)
		VERIFY(::DestroyIcon(m_icoColl));
}

void CCollectionViewDialog::DoDataExchange(CDataExchange *pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COLLECTIONVEWLIST, m_CollectionViewList);
	DDX_Control(pDX, IDC_COLLECTIONVIEWCATEGORYCHECK, m_AddNewCatagory);
	DDX_Control(pDX, IDC_COLLECTIONVIEWLISTLABEL, m_CollectionViewListLabel);
	DDX_Control(pDX, IDC_COLLECTIONVIEWLISTICON, m_CollectionViewListIcon);
	DDX_Control(pDX, IDC_VIEWCOLLECTIONDL, m_CollectionDownload);
	DDX_Control(pDX, IDC_VCOLL_CLOSE, m_CollectionExit);
	DDX_Control(pDX, IDC_COLLECTIONVIEWAUTHOR, m_CollectionViewAuthor);
	DDX_Control(pDX, IDC_COLLECTIONVIEWAUTHORKEY, m_CollectionViewAuthorKey);
}

void CCollectionViewDialog::SetCollection(CCollection *pCollection)
{
	if (!pCollection) {
		ASSERT(0);
		return;
	}
	m_pCollection = pCollection;
}

BOOL CCollectionViewDialog::OnInitDialog()
{
	CDialog::OnInitDialog();
	InitWindowStyles(this);

	if (!m_pCollection) {
		ASSERT(0);
		return TRUE;
	}

	m_CollectionViewList.Init(_T("CollectionView"));
	SetIcon(m_icoWnd = theApp.LoadIcon(_T("Collection_View")), FALSE);

	m_AddNewCatagory.SetCheck(0);

	CString str(GetResString(IDS_VIEWCOLLECTION));
	str.AppendFormat(_T(": %s"), (LPCTSTR)m_pCollection->m_sCollectionName);
	SetWindowText(str);

	m_icoColl = theApp.LoadIcon(_T("AABCollectionFileType"));
	m_CollectionViewListIcon.SetIcon(m_icoColl);
	m_CollectionDownload.SetWindowText(GetResString(IDS_DOWNLOAD));
	m_CollectionExit.SetWindowText(GetResString(IDS_CW_CLOSE));
	SetDlgItemText(IDC_COLLECTIONVIEWAUTHORLABEL, GetResString(IDS_AUTHOR) + _T(':'));
	SetDlgItemText(IDC_COLLECTIONVIEWAUTHORKEYLABEL, GetResString(IDS_AUTHORKEY) + _T(':'));
	SetDlgItemText(IDC_COLLECTIONVIEWCATEGORYCHECK, GetResString(IDS_COLL_ADDINCAT));
	SetDlgItemText(IDC_VCOLL_DETAILS, GetResString(IDS_DETAILS));
	SetDlgItemText(IDC_VCOLL_OPTIONS, GetResString(IDS_OPTIONS));

	m_CollectionViewAuthor.SetWindowText(m_pCollection->m_sCollectionAuthorName);
	m_CollectionViewAuthorKey.SetWindowText(m_pCollection->GetAuthorKeyHashString());

	AddAnchor(IDC_COLLECTIONVEWLIST, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_VCOLL_DETAILS, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_VCOLL_OPTIONS, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_COLLECTIONVIEWAUTHORLABEL, BOTTOM_LEFT);
	AddAnchor(IDC_COLLECTIONVIEWAUTHORKEYLABEL, BOTTOM_LEFT);
	AddAnchor(IDC_COLLECTIONVIEWCATEGORYCHECK, BOTTOM_LEFT);
	AddAnchor(IDC_COLLECTIONVIEWAUTHOR, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_COLLECTIONVIEWAUTHORKEY, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(IDC_VCOLL_CLOSE, BOTTOM_RIGHT);
	AddAnchor(IDC_VIEWCOLLECTIONDL, BOTTOM_RIGHT);
	EnableSaveRestore(PREF_INI_SECTION);

	CSKey key;
	for (POSITION pos = m_pCollection->m_CollectionFilesMap.GetStartPosition(); pos != NULL;) {
		CCollectionFile *pCollectionFile;
		m_pCollection->m_CollectionFilesMap.GetNextAssoc(pos, key, pCollectionFile);

		int iImage = theApp.GetFileTypeSystemImageIdx(pCollectionFile->GetFileName());
		int iItem = m_CollectionViewList.InsertItem(LVIF_TEXT | LVIF_PARAM | (iImage > 0 ? LVIF_IMAGE : 0), m_CollectionViewList.GetItemCount(), NULL, 0, 0, iImage, (LPARAM)pCollectionFile);
		if (iItem >= 0) {
			m_CollectionViewList.SetItemText(iItem, colName, pCollectionFile->GetFileName());
			m_CollectionViewList.SetItemText(iItem, colSize, (LPCTSTR)CastItoXBytes(pCollectionFile->GetFileSize()));
			m_CollectionViewList.SetItemText(iItem, colHash, md4str(pCollectionFile->GetFileHash()));
		}
	}

	for (int iItem = m_CollectionViewList.GetItemCount(); --iItem >= 0;)
		m_CollectionViewList.SetItemState(iItem, LVIS_SELECTED, LVIS_SELECTED);

	CString strTitle(GetResString(IDS_COLLECTIONLIST));
	strTitle.AppendFormat(_T(" (%d)"), m_CollectionViewList.GetItemCount());
	m_CollectionViewListLabel.SetWindowText(strTitle);

	return TRUE;
}

void CCollectionViewDialog::OnNmDblClkCollectionList(LPNMHDR, LRESULT *pResult)
{
	DownloadSelected();
	*pResult = 0;
}

void CCollectionViewDialog::DownloadSelected()
{
	int iNewIndex = 0;
	for (int iIndex = (int)thePrefs.GetCatCount(); --iIndex > 0;)
		if (!m_pCollection->m_sCollectionName.CompareNoCase(thePrefs.GetCategory(iIndex)->strTitle)) {
			iNewIndex = iIndex;
			break;
		}

	if (m_AddNewCatagory.GetCheck() && !iNewIndex) {
		iNewIndex = theApp.emuledlg->transferwnd->AddCategory(m_pCollection->m_sCollectionName, thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR), _T(""), _T(""), true);
		theApp.emuledlg->searchwnd->UpdateCatTabs();
	}

	CTypedPtrList<CPtrList, CCollectionFile*> collectionFileList;
	for (POSITION pos = m_CollectionViewList.GetFirstSelectedItemPosition(); pos != NULL;) {
		int index = m_CollectionViewList.GetNextSelectedItem(pos);
		if (index >= 0)
			collectionFileList.AddTail(reinterpret_cast<CCollectionFile*>(m_CollectionViewList.GetItemData(index)));
	}

	while (!collectionFileList.IsEmpty()) {
		CCollectionFile *pCollectionFile = collectionFileList.RemoveHead();
		if (pCollectionFile)
			theApp.downloadqueue->AddSearchToDownload(pCollectionFile->GetED2kLink(), thePrefs.AddNewFilesPaused(), iNewIndex);
	}
}

void CCollectionViewDialog::OnBnClickedViewCollection()
{
	DownloadSelected();
	OnBnClickedOk();
}

void CCollectionViewDialog::OnBnClickedOk()
{
	OnOK();
}