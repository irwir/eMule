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
#include "emule.h"
#include "KademliaWnd.h"
#include "KadContactListCtrl.h"
#include "Ini2.h"
#include "OtherFunctions.h"
#include "emuledlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// CONContactListCtrl

IMPLEMENT_DYNAMIC(CKadContactListCtrl, CMuleListCtrl)

BEGIN_MESSAGE_MAP(CKadContactListCtrl, CMuleListCtrl)
	ON_NOTIFY_REFLECT(LVN_COLUMNCLICK, OnLvnColumnClick)
	ON_WM_DESTROY()
	ON_WM_SYSCOLORCHANGE()
END_MESSAGE_MAP()

CKadContactListCtrl::CKadContactListCtrl()
{
	SetGeneralPurposeFind(true);
	SetSkinKey(_T("KadContactsLv"));
}

void CKadContactListCtrl::Init()
{
	SetPrefsKey(_T("ONContactListCtrl"));
	SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	InsertColumn(colID,		  GetResString(IDS_ID),			LVCFMT_LEFT, 16 + DFLT_HASH_COL_WIDTH);
	InsertColumn(colType,	  GetResString(IDS_TYPE),		LVCFMT_LEFT, 50);
	InsertColumn(colDistance, GetResString(IDS_KADDISTANCE),LVCFMT_LEFT, 600);

	SetAllIcons();
	Localize();

	LoadSettings();
	int iSortItem = GetSortItem();
	bool bSortAscending = GetSortAscending();

	SetSortArrow(iSortItem, bSortAscending);
	SortItems(SortProc, MAKELONG(iSortItem, (bSortAscending ? 0 : 0x0001)));
}

void CKadContactListCtrl::SaveAllSettings()
{
	SaveSettings();
}

void CKadContactListCtrl::OnSysColorChange()
{
	CMuleListCtrl::OnSysColorChange();
	SetAllIcons();
}

void CKadContactListCtrl::SetAllIcons()
{
	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	iml.Add(CTempIconLoader(_T("Contact0")));
	iml.Add(CTempIconLoader(_T("Contact1")));
	iml.Add(CTempIconLoader(_T("Contact2")));
	iml.Add(CTempIconLoader(_T("Contact3")));
	iml.Add(CTempIconLoader(_T("Contact4")));
	iml.Add(CTempIconLoader(_T("SrcUnknown"))); // replace
	ASSERT((GetStyle() & LVS_SHAREIMAGELISTS) == 0);
	HIMAGELIST himl = ApplyImageList(iml.Detach());
	if (himl)
		::ImageList_Destroy(himl);
}

void CKadContactListCtrl::Localize()
{
	CHeaderCtrl *pHeaderCtrl = GetHeaderCtrl();
	HDITEM hdi;
	hdi.mask = HDI_TEXT;
	CString strRes;

	for (int icol = pHeaderCtrl->GetItemCount(); --icol >= 0;) {
		UINT uid;
		switch (icol) {
		case colID:
			uid = IDS_ID;
			break;
		case colType:
			uid = IDS_TYPE;
			break;
		case colDistance:
			uid = IDS_KADDISTANCE;
			break;
		default:
			uid = 0;
			strRes.Empty();
		}
		if (uid)
			strRes = GetResString(uid);
		hdi.pszText = const_cast<LPTSTR>((LPCTSTR)strRes);
		pHeaderCtrl->SetItem(icol, &hdi);
	}
}

void CKadContactListCtrl::UpdateContact(int iItem, const Kademlia::CContact *contact)
{
	CString id;
	contact->GetClientID(&id);
	SetItemText(iItem, colID, id);

	id.Format(_T("%i(%u)"), contact->GetType(), contact->GetVersion());
	SetItemText(iItem, colType, id);

	contact->GetDistance(&id);
	SetItemText(iItem, colDistance, id);

	UINT nImageShown;
	if (contact->IsBootstrapContact())
		nImageShown = 5; //contact->IsBootstrapFailed() ? 4 : 5;
	else {
		nImageShown = contact->GetType() > 4 ? 4 : contact->GetType();
		if (nImageShown < 3 && !contact->IsIpVerified())
			nImageShown = 5; // if we have an active contact, which is however not IP verified (and therefore not used), show this icon instead
	}
	SetItem(iItem, 0, LVIF_IMAGE, 0, nImageShown, 0, 0, 0, 0);
}

void CKadContactListCtrl::UpdateKadContactCount()
{
	theApp.emuledlg->kademliawnd->UpdateContactCount();
}

bool CKadContactListCtrl::ContactAdd(const Kademlia::CContact *contact)
{
	try {
		ASSERT(contact != NULL);
		int iItem = InsertItem(LVIF_TEXT | LVIF_PARAM, GetItemCount(), NULL, 0, 0, 0, (LPARAM)contact);
		if (iItem >= 0) {
			UpdateContact(iItem, contact);
			UpdateKadContactCount();
			return true;
		}
	} catch (...) {
		ASSERT(0);
	}
	return false;
}

void CKadContactListCtrl::ContactRem(const Kademlia::CContact *contact)
{
	try {
		ASSERT(contact != NULL);
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)contact;
		int iItem = FindItem(&find);
		if (iItem >= 0) {
			DeleteItem(iItem);
			UpdateKadContactCount();
		}
	} catch (...) {
		ASSERT(0);
	}
}

void CKadContactListCtrl::ContactRef(const Kademlia::CContact *contact)
{
	try {
		ASSERT(contact != NULL);
		LVFINDINFO find;
		find.flags = LVFI_PARAM;
		find.lParam = (LPARAM)contact;
		int iItem = FindItem(&find);
		if (iItem >= 0)
			UpdateContact(iItem, contact);
	} catch (...) {
		ASSERT(0);
	}
}

BOOL CKadContactListCtrl::OnCommand(WPARAM, LPARAM)
{
	// ???
	return TRUE;
}

void CKadContactListCtrl::OnLvnColumnClick(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMLISTVIEW *pNMListView = reinterpret_cast<NMLISTVIEW*>(pNMHDR);

	// Determine ascending based on whether already sorted on this column
	int iSortItem = GetSortItem();
	bool bOldSortAscending = GetSortAscending();
	bool bSortAscending = (iSortItem != pNMListView->iSubItem) ? true : !bOldSortAscending;

	// Item is column clicked
	iSortItem = pNMListView->iSubItem;

	// Sort table
	UpdateSortHistory(MAKELONG(iSortItem, (bSortAscending ? 0 : 0x0001)));
	SetSortArrow(iSortItem, bSortAscending);
	SortItems(SortProc, MAKELONG(iSortItem, (bSortAscending ? 0 : 0x0001)));

	*pResult = 0;
}

int CALLBACK CKadContactListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	const Kademlia::CContact *item1 = reinterpret_cast<Kademlia::CContact*>(lParam1);
	const Kademlia::CContact *item2 = reinterpret_cast<Kademlia::CContact*>(lParam2);
	if (item1 == NULL || item2 == NULL)
		return 0;

	int iResult;
	switch (LOWORD(lParamSort)) {
	case colID:
		{
			Kademlia::CUInt128 i1;
			Kademlia::CUInt128 i2;
			item1->GetClientID(&i1);
			item2->GetClientID(&i2);
			iResult = i1.CompareTo(i2);
		}
		break;
	case colType:
		iResult = item1->GetType() - item2->GetType();
		if (iResult == 0)
			iResult = item1->GetVersion() - item2->GetVersion();
		break;
	case colDistance:
		{
			Kademlia::CUInt128 distance1, distance2;
			item1->GetDistance(&distance1);
			item2->GetDistance(&distance2);
			iResult = distance1.CompareTo(distance2);
		}
		break;
	default:
		return 0;
	}
	return HIWORD(lParamSort) ? -iResult : iResult;
}