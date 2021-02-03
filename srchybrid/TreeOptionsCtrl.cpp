/*
Module : TreeOptionsCtrl.cpp
Purpose: Implementation for an MFC class to implement a tree options control
		 similar to the advanced tab as seen on the "Internet options" dialog in
		 Internet Explorer 4 and later

Created: PJN / 31-03-1999
History: PJN / 21-04-1999 Added full support for enabling / disabling all the item types
		 PJN / 05-10-1999 Made class more self contained by internally managing the image list
		 PJN / 07-10-1999 1. Added support for including combo boxes as well as edit boxes into the
						  edit control.
						  2. Added support for customizing the image list to use
		 PJN / 29-02-2000 Removed a VC 6 level 4 warning
		 PJN / 03-04-2000 Added support for navigation into and out of the combo boxes and edit controls
						  inside the control
		 PJN / 10-05-2000 1. Fixed a bug where the text was not being transferred to the control when the
						  in in-place combo or edit box is active and the tree control gets destroyed.
						  2. Added support for having check box items as children of other check box items
						  3. Setting the check box state of a parent now also sets the check box state
						  for all child box children.
						  4. Setting the check box state affects the check box state of the parent if that
						  parent is also a check box.
		 PJN / 30-05-2000 Code now uses ON_NOTIFY_REFLECT_EX instead of ON_NOTIFY_REFLECT. This allows
						  derived classes to handle the reflected messages also.
		 PJN / 03-07-2000 Now includes support for edit boxes with accompanying spin controls
		 PJN / 25-04-2001 1. Creation of the image list is now virtual. This allows customisation such as
						  being able to use high color bitmaps
						  2. Added an option which determines if check box state should be changed when you
						  click anywhere on an item or just over the checkbox icon
						  3. Updated copyright message
		 PJN / 12-08-2001 1. Fixed an issue in GetComboText and GetEditText where the code was modifying the
						  contents of the combo/edit box when it was being read. This was because that
						  function is doing double duty as it is called when the child control is about to be
						  created in place, and you want to remove the text from the tree control and put it
						  in the child control. Thanks to "Jef" for spotting this.
						  2. Made the code in SetComboText more robust. Thanks to "Jef" for this also.
						  3. Added a DDX method for integers in an edit box. Thanks to Colin Urquhart for this.
						  4. Added an extra member to CTreeOptionsItemData to be used as an item data. This
						  allows you to avoid having to implement multiple derived classes and instead use
						  the item data's I now provide to stash away pointers etc.
		 PJN / 27-08-2001 1. Provided a "GetUserItemData" member to provide access to the item data provided
						  by the class. Thanks to Colin Urquhart for this.
						  2. Fixed a redraw problem which occurred when you scrolled using a wheel enabled
						  mouse. Thanks to "Jef" for spotting this.
						  3. Provided an AutoSelect option which automatically sets focus to child control
						  in the tree control. Thanks to "Jef" for this suggestion.
						  4. Added full support for CBS_DROPDOWN and CBS_SIMPLE style combo boxes. Thanks
						  to "Jef" for this suggestion.
		 PJN / 16-08-2001 1. Provided support for specify a color via CColorDialog.
						  2. Controls are now created to fill the full width of the tree control
						  3. Provided support for specifying a font via CFontDialog
						  4. Provided support for specifying a font name from a combo box
						  5. Provided support for specifying a boolean value from a combo box
		 PJN / 27-11-2001 1. Fixed a bug where the message map for OnMouseWheel was setup incorrectly. It
						  should have been ON_WM_MOUSEWHEEL instead of ON_MESSAGE!!!.
						  2. Allowed passing in the hAfter item for the InsertItem calls. All params are defaulted
						  as to not affect any current code.
						  3. Made possible the use of radio button groups followed by other items (in which case
						  the group is considered complete).
						  4. Added a couple of utility functions at the bottom of the cpp file. Thanks to Mike
						  Funduc for all these updates.
		 PJN / 05-11-2001 1. Minor code tidy up following development of the author's CTreeOptionsCtrl class
		 PJN / 13-12-2001 1. Fixed an assertion in OnClick. Thanks to "flipposwitch" for spotting the problem
		 PJN / 14-02-2002 1. Now allows item data to be associated with any item added to the control
						  2. Fixed issue with return value from GetUserItemData
		 PJN / 02-06-2002 1. Moved sample app to VC 6 to facilitate support for IP Address control and date and
						  time controls.
						  2. Fixed a bug where the child controls can get orphaned when a node in the tree
						  control is expanded or contracted. Thanks to Lauri Ott for spotting this problem.
						  3. Now fully supports the CDateTimeCtrl for editing of dates and times
						  4. Now fully supports the CIPAddressCtrl for editing of IP addresses
						  5. Custom draw support for color browser items is now configurable via an additional
						  parameter of the AddColorBrowser method
		 PJN / 24-09-2002 1. Updated documentation which incorrectly stated that the parent of a check box item
						  must be a group item as inserted with InsertGroup. Thanks to Kögl Christoph for
						  spotting this.
						  2. Fixed an issue with "IMPLEMENT_DYNAMIC(CDateTimeCtrl..." not being declared propertly.
						  Some users reported that it worked OK, while others said that my fix was causing link
						  problems. The problem should be sorted out for good now. Thanks to Kögl Christoph for
						  reporting this.
						  3. Renamed the SetImageListToUse function to "SetImageListResourceIDToUse".
						  4. Provided a GetImageListResourceIDToUse function to match up with the Set function.
						  5. Provided a method to allow the user item data to be changed after an item has
						  been created.
						  6. Provided some documentation info how to safely use item data in the control.
						  Thanks to Kögl Christoph for reporting this.
						  7. Fixed a potential memory leak in AddComboBox if the function is invoked twice without
						  an explicit delete of the item first. Thanks to Kögl Christoph for reporting this.
						  8. Fixed some typos in the documentation. It incorrectly stated that the return type of
						  member functions InsertGroup, InsertCheckBox, and InsertRadioButton is BOOL when in fact
						  it is HTREEITEM.  Thanks to Kögl Christoph for reporting this.
						  9. Improved the look of the disabled checked check button. Thanks to Kögl Christoph for
						  reporting this.
						  10. Improved the look of the disabled radio button which is selected. Thanks to Kögl
						  Christoph for reporting this.
		 PJN / 17-10-2002 1. Added a method to add an "Opaque Browser" to the Tree options control. An
						  Opaque Browser is where the tree options control allows an end user specified
						  structure to be edited by the tree options control without it explicitly
						  knowing what it is editing.
		 PJN / 25-10-2002 1. Updated the download to include the missing files OpaqueShow.cpp/h. Thanks to Kögl
						  Christoph for reporting this.
						  2. Made the class more const-correct. e.g. the Get... member functions are now const.
						  Thanks to Kögl Christoph for reporting this. Also updated the documentation for this.
						  3. Updated the documentation to refer to the "Opaque Browser" support.
		 PJN / 28-10-2002 1. Fixed a bug where upon a combo losing focus it will also result in the associated
						  button control would also be destroyed. Thanks to Kögl Christoph for reporting this
						  problem. Fixed this bug should also fix an intermittent release bug which was occurring
						  in this area.
		 PJN / 30-10-2002 1. Made a number of other methods const. Thanks to Kögl Christoph for reporting this.
		 PJN / 15-11-2002 1. Now allows the Field Data separator i.e. ": " to be configured. Please note that the
						  characters you pick should be avoided in the descriptive text you display for an item
						  as it is used as the divider between the descriptive text and the actual data to be
						  edited. Thanks to Kögl Christoph for this update.
						  2. Fixed an access violation in CTreeOptionsCtrl::OnSelchanged when there is no selected
						  item in the control. Thanks to Kögl Christoph for this update.
		 PJN / 06-03-2003 1. Fixed a memory leak which can occur when the control is used in a property sheet.
						  Thanks to David Rainey for reporting this problem.
						  2. Fixed another memory leak in the destructor of the CTreeOptionsCtrl class when the
						  test app is closed
		 PJN / 14-05-2003 1. Fixed a bug where the OnSelChanged function was getting in the way when the control was
						  being cleared down, leading to an ASSERT. Thanks to Chen Fu for reporting this problem.
		 PJN / 07-06-2003 1. Fixed a bug where the date time control was not reflecting the changes when the child
						  control was displayed. Thanks to Tom Serface for reporting this problem.
		 PJN / 17-07-2003 1. Made SetRadioButton methods in CTreeOptionsCtrl virtual to allow further client
						  customisation.



Copyright (c) 1999 - 2003 by PJ Naughter.  (Web: www.naughter.com, Email: pjna@naughter.com)

All rights reserved.

Copyright / Usage Details:

You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise)
when your product is released in binary form. You are allowed to modify the source code in any way you want
except you cannot modify the copyright details at the top of each module. If you want to distribute source
code with your application, then you are only allowed to distribute versions released by the author. This is
to maintain a single distribution point for the source code.

*/


//////////////// Includes ////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#ifndef _SHLOBJ_H_
#pragma message("To avoid this message, please put shlobj.h in your pre compiled header (normally stdafx.h)")
#include <shlobj.h>
#endif
#include "TreeOptionsCtrl.h"


//////////////// Macros / Locals /////////////////////////////////////

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


const UINT TREE_OPTIONS_COMBOBOX_ID = 100;
const UINT TREE_OPTIONS_EDITBOX_ID = 101;
const UINT TREE_OPTIONS_SPINCTRL_ID = 102;
const UINT TREE_OPTIONS_BROWSEBUTTONCTRL_ID = 103;
const UINT TREE_OPTIONS_STATIC_ID = 104;
const UINT TREE_OPTIONS_DATETIMECTRL_ID = 105;
const UINT TREE_OPTIONS_IPADDRESSCTRL_ID = 106;

#define WM_TOC_SETFOCUS_TO_CHILD WM_APP + 1
#define WM_TOC_REPOSITION_CHILD_CONTROL WM_APP + 2



//////////////// Implementation //////////////////////////////////////

IMPLEMENT_DYNAMIC(CTreeOptionsCtrl, CTreeCtrl)

BEGIN_MESSAGE_MAP(CTreeOptionsCtrl, CTreeCtrl)
	//{{AFX_MSG_MAP(CTreeOptionsCtrl)
	ON_WM_CHAR()
	ON_WM_DESTROY()
	ON_WM_HSCROLL()
	ON_WM_KEYDOWN()
	ON_WM_KILLFOCUS()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEWHEEL()
	ON_WM_VSCROLL()
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_TOC_REPOSITION_CHILD_CONTROL, OnRepositionChild)
	ON_MESSAGE(WM_TOC_SETFOCUS_TO_CHILD, OnSetFocusToChild)
	ON_NOTIFY_REFLECT_EX(NM_CLICK, OnNmClick)
	ON_NOTIFY_REFLECT_EX(NM_CUSTOMDRAW, OnCustomDraw)
	ON_NOTIFY_REFLECT_EX(TVN_DELETEITEM, OnDeleteItem)
	ON_NOTIFY_REFLECT_EX(TVN_ITEMEXPANDING, OnItemExpanding)
	ON_NOTIFY_REFLECT_EX(TVN_SELCHANGED, OnSelchanged)
END_MESSAGE_MAP()

CTreeOptionsCtrl::CTreeOptionsCtrl()
	: m_pCombo()
	, m_pEdit()
	, m_pSpin()
	, m_pButton()
	, m_pDateTime()
	, m_pIPAddress()
	, m_hControlItem()
	, m_sSeparator(_T(": "))
#ifdef IDB_TREE_CTRL_OPTIONS
	, m_nilID(IDB_TREE_CTRL_OPTIONS)
#else
	, m_nilID()
#endif
	, m_bToggleOverIconOnly()
	, m_bAutoSelect()
	, m_bBeingCleared()
{
}

CTreeOptionsCtrl::~CTreeOptionsCtrl()
{
	CTreeOptionsCtrl::DestroyOldChildControl();

	ASSERT(m_pCombo == NULL);
	ASSERT(m_pEdit == NULL);
	ASSERT(m_pSpin == NULL);
	ASSERT(m_pButton == NULL);
	ASSERT(m_pDateTime == NULL);
	ASSERT(m_pIPAddress == NULL);
}

LRESULT CTreeOptionsCtrl::OnSetFocusToChild(WPARAM, LPARAM)
{
	if (m_pCombo)
		m_pCombo->SetFocus();
	else if (m_pEdit)
		m_pEdit->SetFocus();
	else if (m_pDateTime)
		m_pDateTime->SetFocus();
	else if (m_pIPAddress)
		m_pIPAddress->SetFocus();
	return 0;
}

LRESULT CTreeOptionsCtrl::OnRepositionChild(WPARAM, LPARAM)
{
	HTREEITEM hItem = GetSelectedItem();
	if (hItem) {
		UpdateTreeControlValueFromChildControl(hItem);
		DestroyOldChildControl();
		CreateNewChildControl(hItem);
	}
	return 0;
}

DWORD CTreeOptionsCtrl::GetUserItemData(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	return pItemData->m_dwItemData;
}

BOOL CTreeOptionsCtrl::SetUserItemData(HTREEITEM hItem, DWORD dwData)
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	pItemData->m_dwItemData = dwData;
	return TRUE;
}

BOOL CTreeOptionsCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	//Clean up any controls currently open we used
	if (m_hControlItem) {
		UpdateTreeControlValueFromChildControl(m_hControlItem);
		DestroyOldChildControl();
	}

	//Let the parent class do its thing
	return CTreeCtrl::OnMouseWheel(nFlags, zDelta, pt);
}

BOOL CTreeOptionsCtrl::OnDeleteItem(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMTREEVIEW *pNMTreeView = reinterpret_cast<NMTREEVIEW*>(pNMHDR);

	*pResult = 0;

	//Free up the memory we had allocated in the item data (if needed)
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(pNMTreeView->itemOld.hItem));
	delete pItemData;

	return FALSE;
}

void CTreeOptionsCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_RIGHT) {
		HTREEITEM hItem = GetSelectedItem();
		if (GetItemData(hItem) && m_hControlItem != NULL) {
			// if we have a children and VK_RIGHT -> Focus on it !
			CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
			if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsCombo))) {
				if (m_pCombo->IsWindowVisible())
					m_pCombo->SetFocus();
			} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsEdit))) {
				if (m_pEdit->IsWindowVisible())
					m_pEdit->SetFocus();
			} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsDateCtrl))) {
				if (m_pDateTime->IsWindowVisible())
					m_pDateTime->SetFocus();
			} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsIPAddressCtrl))) {
				if (m_pIPAddress->IsWindowVisible())
					m_pIPAddress->SetFocus();
			} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsBrowseButton))) {
				if (m_pButton->IsWindowVisible())
					m_pButton->SetFocus();
			} else {
				//Pass on to the parent since we didn't handle it
				CTreeCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
			}
		} else {
			//Pass on to the parent since we didn't handle it
			CTreeCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
		}
	} else {
		//Pass on to the parent since we didn't handle it
		CTreeCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
	}
}

void CTreeOptionsCtrl::HandleCheckBox(HTREEITEM hItem, BOOL bCheck)
{
	//Turn of redraw to Q all the changes we're going to make here
	SetRedraw(FALSE);

	//Toggle the state
	VERIFY(SetCheckBox(hItem, !bCheck));

	//If the item has children, then iterate through them and for all items
	//which are check boxes set their state to be the same as the parent
	HTREEITEM hChild = GetNextItem(hItem, TVGN_CHILD);
	while (hChild) {
		if (IsCheckBox(hChild))
			SetCheckBox(hChild, !bCheck);

		//Move on to the next item
		hChild = GetNextItem(hChild, TVGN_NEXT);
	}

	//Get the parent item and if it is a checkbox, then iterate through
	//all its children and if all the checkboxes are checked, then also
	//automatically check the parent. If no checkboxes are checked, then
	//also automatically uncheck the parent.
	HTREEITEM hParent = GetNextItem(hItem, TVGN_PARENT);
	if (hParent && IsCheckBox(hParent)) {
		BOOL bNoCheckBoxesChecked = TRUE;
		BOOL bAllCheckBoxesChecked = TRUE;
		hChild = GetNextItem(hParent, TVGN_CHILD);
		while (hChild) {
			if (IsCheckBox(hChild)) {
				BOOL bThisChecked;
				VERIFY(GetCheckBox(hChild, bThisChecked));
				bNoCheckBoxesChecked = bNoCheckBoxesChecked && !bThisChecked;
				bAllCheckBoxesChecked = bAllCheckBoxesChecked && bThisChecked;
			}

			//Move on to the next item
			hChild = GetNextItem(hChild, TVGN_NEXT);
		}

		if (bNoCheckBoxesChecked)
			SetCheckBox(hParent, FALSE);
		else if (bAllCheckBoxesChecked) {
			SetCheckBox(hParent, FALSE); //gets rid of the semi state
			SetCheckBox(hParent, TRUE);
		} else {
			BOOL bEnable;
			VERIFY(GetCheckBoxEnable(hParent, bEnable));
			SetEnabledSemiCheckBox(hParent, bEnable);
		}
	}

	//Reset the redraw flag
	SetRedraw(TRUE);
}

void CTreeOptionsCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
	UINT uFlags;
	HTREEITEM hItem = HitTest(point, &uFlags);
	if (!hItem)
		uFlags = 0;

	//If the mouse was over the label, icon or to the left or right of the item?
	BOOL bHit;
	if (m_bToggleOverIconOnly)
		bHit = uFlags == TVHT_ONITEMICON;
	else
		bHit = (uFlags & TVHT_ONITEM) || (uFlags & TVHT_ONITEMINDENT) || (uFlags & TVHT_ONITEMRIGHT);

	if (bHit) {
		if (IsCheckBox(hItem)) {
			BOOL bEnable;
			VERIFY(GetCheckBoxEnable(hItem, bEnable));
			if (bEnable) {
				//Turn of redraw to Q all the changes we're going to make here
				SetRedraw(FALSE);

				//Toggle the state of check items
				BOOL bCheck;
				VERIFY(GetCheckBox(hItem, bCheck));
				HandleCheckBox(hItem, bCheck);
			}
		} else if (IsRadioButton(hItem)) {
			BOOL bEnable;
			VERIFY(GetRadioButtonEnable(hItem, bEnable));
			if (bEnable) {
				//Check the radio button if not already checked
				BOOL bCheck;
				VERIFY(GetRadioButton(hItem, bCheck));
				if (!bCheck)
					VERIFY(SetRadioButton(hItem));
			}
		}

		//Pass on to the parent now that we that we have done our stuff
		CTreeCtrl::OnLButtonDown(nFlags, point);
	} else {
		//Pass on to the parent since we didn't handle it
		CTreeCtrl::OnLButtonDown(nFlags, point);
	}
}

void CTreeOptionsCtrl::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_SPACE) {
		HTREEITEM hItem = GetSelectedItem();

		//If the space key is hit, and the item is a combo item, then toggle the check value
		if (IsCheckBox(hItem)) {
			BOOL bEnable;
			VERIFY(GetCheckBoxEnable(hItem, bEnable));
			if (bEnable) {
				//Turn of redraw to Q all the changes we're going to make here
				SetRedraw(FALSE);

				BOOL bCheck;
				VERIFY(GetCheckBox(hItem, bCheck));
				HandleCheckBox(hItem, bCheck);
			} else {
				//Pass on to the parent since we didn't handle it
				CTreeCtrl::OnChar(nChar, nRepCnt, nFlags);
			}
		} else if (IsRadioButton(hItem)) { //If the item is a radio item, then check it and uncheck all other items
			BOOL bEnable;
			VERIFY(GetRadioButtonEnable(hItem, bEnable));
			if (bEnable) {
				//Check the radio button if not already checked
				BOOL bCheck;
				VERIFY(GetRadioButton(hItem, bCheck));
				if (!bCheck)
					VERIFY(SetRadioButton(hItem));
			} else {
				//Pass on to the parent since we didn't handle it
				CTreeCtrl::OnChar(nChar, nRepCnt, nFlags);
			}
		} else {
			//Pass on to the parent since we didn't handle it
			CTreeCtrl::OnChar(nChar, nRepCnt, nFlags);
		}
	} else {
		//Pass on to the parent since we didn't handle it
		CTreeCtrl::OnChar(nChar, nRepCnt, nFlags);
	}
}

HTREEITEM CTreeOptionsCtrl::InsertGroup(LPCTSTR lpszItem, int nImage, HTREEITEM hParent, HTREEITEM hAfter, DWORD dwItemData)
{
	ASSERT(nImage > 9); //You must specify an image index greater than 9 as the
	//first 10 images in the image list are reserved for the
	//checked and unchecked check box and radio buttons images

	HTREEITEM hItem = InsertItem(lpszItem, nImage, nImage, hParent, hAfter);
	CTreeOptionsItemData *pItemData = new CTreeOptionsItemData;
	pItemData->m_pRuntimeClass1 = NULL;
	pItemData->m_Type = CTreeOptionsItemData::Normal;
	pItemData->m_dwItemData = dwItemData;
	SetItemData(hItem, (DWORD_PTR)pItemData);

	return hItem;
}

HTREEITEM CTreeOptionsCtrl::InsertCheckBox(LPCTSTR lpszItem, HTREEITEM hParent, BOOL bCheck, HTREEITEM hAfter, DWORD dwItemData)
{
	ASSERT((hParent == TVI_ROOT) || IsGroup(hParent) || IsCheckBox(hParent)); //The parent of a check box must be a group item or another check box

	HTREEITEM hItem = InsertItem(lpszItem, 0, 0, hParent, hAfter);
	CTreeOptionsItemData *pItemData = new CTreeOptionsItemData;
	pItemData->m_pRuntimeClass1 = NULL;
	pItemData->m_Type = CTreeOptionsItemData::CheckBox;
	pItemData->m_dwItemData = dwItemData;
	SetItemData(hItem, (DWORD_PTR)pItemData);
	VERIFY(SetCheckBox(hItem, bCheck));

	return hItem;
}

HTREEITEM CTreeOptionsCtrl::InsertRadioButton(LPCTSTR lpszItem, HTREEITEM hParent, BOOL bCheck, HTREEITEM hAfter, DWORD dwItemData)
{
	ASSERT(IsGroup(hParent)); //The parent of a radio item must be a group item

	HTREEITEM hItem = InsertItem(lpszItem, 2, 2, hParent, hAfter);
	CTreeOptionsItemData *pItemData = new CTreeOptionsItemData;
	pItemData->m_pRuntimeClass1 = NULL;
	pItemData->m_Type = CTreeOptionsItemData::RadioButton;
	pItemData->m_dwItemData = dwItemData;
	SetItemData(hItem, (DWORD_PTR)pItemData);
	if (bCheck)
		//if requested to, check the newly added radio button
		VERIFY(SetRadioButton(hItem));

	return hItem;
}

BOOL CTreeOptionsCtrl::IsGroup(HTREEITEM hItem) const
{
	int nImage = -1;
	int nSelectedImage = -1;
	VERIFY(GetItemImage(hItem, nImage, nSelectedImage));

	return (nImage > 7);
}

BOOL CTreeOptionsCtrl::IsCheckBox(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::CheckBox;
}

BOOL CTreeOptionsCtrl::IsRadioButton(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::RadioButton;
}

BOOL CTreeOptionsCtrl::IsEditBox(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::EditBox;
}

BOOL CTreeOptionsCtrl::IsColorItem(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::ColorBrowser;
}

BOOL CTreeOptionsCtrl::IsFontItem(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::FontBrowser;
}

BOOL CTreeOptionsCtrl::IsFileItem(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::FileBrowser;
}

BOOL CTreeOptionsCtrl::IsFolderItem(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::FolderBrowser;
}

BOOL CTreeOptionsCtrl::IsDateTimeItem(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::DateTimeCtrl;
}

BOOL CTreeOptionsCtrl::IsIPAddressItem(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::IPAddressCtrl;
}

BOOL CTreeOptionsCtrl::IsOpaqueItem(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	return pItemData && pItemData->m_Type == CTreeOptionsItemData::OpaqueBrowser;
}

BOOL CTreeOptionsCtrl::SetCheckBox(HTREEITEM hItem, BOOL bCheck)
{
	//Validate our parameters
	ASSERT(IsCheckBox(hItem)); //Must be a check box to check it

	BOOL bSemi;
	VERIFY(GetSemiCheckBox(hItem, bSemi));

	BOOL bSuccess;
	if (bCheck)
		if (bSemi)
			bSuccess = SetItemImage(hItem, 8, 8);
		else
			bSuccess = SetItemImage(hItem, 1, 1);
	else
		bSuccess = SetItemImage(hItem, 0, 0);

	return bSuccess;
}

BOOL CTreeOptionsCtrl::SetEnabledSemiCheckBox(HTREEITEM hItem, BOOL bSemi)
{
	//Validate our parameters
	ASSERT(IsCheckBox(hItem)); //Must be a check box to check it

	if (bSemi)
		return SetItemImage(hItem, 8, 8);
	return SetItemImage(hItem, 9, 9);
}

BOOL CTreeOptionsCtrl::GetCheckBox(HTREEITEM hItem, BOOL &bCheck) const
{
	//Validate our parameters
	ASSERT(IsCheckBox(hItem)); //Must be a combo item to check it

	int nImage;
	int nSelectedImage;
	BOOL bSuccess = GetItemImage(hItem, nImage, nSelectedImage);
	ASSERT(bSuccess);

	bCheck = (nImage == 1 || nImage == 5 || nImage == 8 || nImage == 9);

	return bSuccess;
}

BOOL CTreeOptionsCtrl::SetRadioButton(HTREEITEM hParent, int nIndex)
{
	//Validate our parameters
	ASSERT(IsGroup(hParent)); //Parent item must be a group item

	//Iterate through the child items and turn on the specified one and turn off all the other ones
	HTREEITEM hChild = GetNextItem(hParent, TVGN_CHILD);

	//Turn of redraw to Q all the changes we're going to make here
	SetRedraw(FALSE);

	int i = 0;
	BOOL bCheckedSomeItem = FALSE;
	while (hChild) {
		//if we reach a non radio button then break out of the loop
		if (!IsRadioButton(hChild))
			break;

		if (i == nIndex) {
			//Turn this item on
			VERIFY(SetItemImage(hChild, 3, 3));
			bCheckedSomeItem = TRUE;
		} else {
			BOOL bEnable;
			VERIFY(GetRadioButtonEnable(hChild, bEnable));

			//Turn this item off
			if (bEnable)
				VERIFY(SetItemImage(hChild, 2, 2));
			else
				VERIFY(SetItemImage(hChild, 4, 4));
		}

		//Move on to the next item
		hChild = GetNextItem(hChild, TVGN_NEXT);
		++i;
	}
	ASSERT(bCheckedSomeItem); //You specified an index which does not exist

	//Reset the redraw flag
	SetRedraw(TRUE);

	return TRUE;
}

BOOL CTreeOptionsCtrl::SetRadioButton(HTREEITEM hItem)
{
	//Validate our parameters
	ASSERT(IsRadioButton(hItem)); //Must be a radio item to check it

	//Iterate through the sibling items and turn them all off except this one
	HTREEITEM hParent = GetNextItem(hItem, TVGN_PARENT);
	ASSERT(IsGroup(hParent)); //Parent item must be a group item

	//Iterate through the child items and turn on the specified one and turn off all the other ones
	HTREEITEM hChild = GetNextItem(hParent, TVGN_CHILD);

	//Turn of redraw to Q all the changes we're going to make here
	SetRedraw(FALSE);

	while (hChild) {
		//if we reach a non radio button then break out of the loop
		if (!IsRadioButton(hChild))
			break;

		if (hChild == hItem)
			//Turn this item on
			VERIFY(SetItemImage(hChild, 3, 3));
		else {
			BOOL bEnable;
			VERIFY(GetRadioButtonEnable(hChild, bEnable));

			//Turn this item off
			if (bEnable)
				VERIFY(SetItemImage(hChild, 2, 2));
			else
				VERIFY(SetItemImage(hChild, 6, 6));
		}

		//Move on to the next item
		hChild = GetNextItem(hChild, TVGN_NEXT);
	}

	//Reset the redraw flag
	SetRedraw(TRUE);

	return TRUE;
}

BOOL CTreeOptionsCtrl::GetRadioButton(HTREEITEM hParent, int &nIndex, HTREEITEM &hCheckItem) const
{
	ASSERT(IsGroup(hParent)); //Parent item must be a group item

	//Iterate through the child items and turn on the specified one and turn off all the other ones
	HTREEITEM hChild = GetNextItem(hParent, TVGN_CHILD);
	ASSERT(hChild); //Must have some child items

	//Find the checked item
	nIndex = 0;
	BOOL bFound = FALSE;
	while (!bFound) {
		if (!IsRadioButton(hChild))  // Handle multiple groups
			nIndex = 0;

		VERIFY(GetRadioButton(hChild, bFound));

		//Move on to the next sibling if not found
		if (!bFound) {
			hChild = GetNextItem(hChild, TVGN_NEXT);
			ASSERT(hChild);
			++nIndex;
		} else {
			hCheckItem = hChild;
			break;                       // This group is done
		}
	}

	return TRUE;
}

BOOL CTreeOptionsCtrl::GetRadioButton(HTREEITEM hItem, BOOL &bCheck) const
{
	ASSERT(IsRadioButton(hItem)); //Must be a radio item to check it

	int nImage;
	int nSelectedImage;
	BOOL bSuccess = GetItemImage(hItem, nImage, nSelectedImage);
	ASSERT(bSuccess);

	bCheck = (nImage == 3 || nImage == 7);

	return bSuccess;
}

BOOL CTreeOptionsCtrl::SetGroupEnable(HTREEITEM hItem, BOOL bEnable)
{
	//Allows you to quickly enable / disable all the items in a group

	ASSERT(IsGroup(hItem)); //Must be group item

	//Iterate through the child items and enable / disable all the items
	HTREEITEM hChild = GetNextItem(hItem, TVGN_CHILD);

	//Turn of redraw to Q all the changes we're going to make here
	SetRedraw(FALSE);

	while (hChild) {
		if (IsRadioButton(hChild)) {
			int nImage;
			int nSelectedImage;
			VERIFY(GetItemImage(hChild, nImage, nSelectedImage));
			BOOL bCheck = (nImage == 3 || nImage == 7);
			if (bCheck) {
				if (bEnable)
					VERIFY(SetItemImage(hChild, 3, 3));
				else
					VERIFY(SetItemImage(hChild, 7, 7));
			} else {
				if (bEnable)
					VERIFY(SetItemImage(hChild, 2, 2));
				else
					VERIFY(SetItemImage(hChild, 6, 6));
			}
		} else if (IsCheckBox(hChild))
			VERIFY(SetCheckBoxEnable(hChild, bEnable));
		else
			ASSERT(0);

		//Move onto the next child
		hChild = GetNextItem(hChild, TVGN_NEXT);
	}

	//Reset the redraw flag
	SetRedraw(TRUE);

	return TRUE;
}

BOOL CTreeOptionsCtrl::GetSemiCheckBox(HTREEITEM hItem, BOOL &bSemi) const
{
	ASSERT(IsCheckBox(hItem)); //Must be a check box

	int nImage;
	int nSelectedImage;
	BOOL bSuccess = GetItemImage(hItem, nImage, nSelectedImage);
	ASSERT(bSuccess);

	bSemi = (nImage == 8 || nImage == 9);

	return bSuccess;
}

BOOL CTreeOptionsCtrl::SetCheckBoxEnable(HTREEITEM hItem, BOOL bEnable)
{
	ASSERT(IsCheckBox(hItem)); //Must be a check box
	BOOL bSuccess;

	if (bEnable) {
		BOOL bCheck;
		VERIFY(GetCheckBox(hItem, bCheck));
		BOOL bSemi;
		VERIFY(GetSemiCheckBox(hItem, bSemi));
		if (bCheck) {
			if (bSemi)
				bSuccess = SetItemImage(hItem, 8, 8);
			else
				bSuccess = SetItemImage(hItem, 1, 1);
		} else
			bSuccess = SetItemImage(hItem, 0, 0);
	} else {
		BOOL bCheck;
		VERIFY(GetCheckBox(hItem, bCheck));
		BOOL bSemi;
		VERIFY(GetSemiCheckBox(hItem, bSemi));
		if (bCheck) {
			if (bSemi)
				bSuccess = SetItemImage(hItem, 9, 9);
			else
				bSuccess = SetItemImage(hItem, 5, 5);
		} else
			bSuccess = SetItemImage(hItem, 4, 4);
	}

	return bSuccess;
}

BOOL CTreeOptionsCtrl::SetRadioButtonEnable(HTREEITEM hItem, BOOL bEnable)
{
	ASSERT(IsRadioButton(hItem)); //Must be a radio button
	BOOL bSuccess;
	BOOL bCheck;

	if (bEnable) {
		VERIFY(GetRadioButton(hItem, bCheck));
		if (bCheck)
			bSuccess = SetItemImage(hItem, 3, 3);
		else
			bSuccess = SetItemImage(hItem, 2, 2);
	} else {
		VERIFY(GetRadioButton(hItem, bCheck));
		if (bCheck)
			bSuccess = SetItemImage(hItem, 7, 7);
		else
			bSuccess = SetItemImage(hItem, 6, 6);
	}

	return bSuccess;
}

BOOL CTreeOptionsCtrl::GetCheckBoxEnable(HTREEITEM hItem, BOOL &bEnable) const
{
	ASSERT(IsCheckBox(hItem)); //Must be a check box

	int nImage;
	int nSelectedImage;
	BOOL bSuccess = GetItemImage(hItem, nImage, nSelectedImage);
	ASSERT(bSuccess);

	bEnable = (nImage == 0 || nImage == 1 || nImage == 8);

	return bSuccess;
}

BOOL CTreeOptionsCtrl::GetRadioButtonEnable(HTREEITEM hItem, BOOL &bEnable) const
{
	ASSERT(IsRadioButton(hItem)); //Must be a radio button

	int nImage;
	int nSelectedImage;
	BOOL bSuccess = GetItemImage(hItem, nImage, nSelectedImage);
	ASSERT(bSuccess);

	bEnable = (nImage == 2 || nImage == 3);

	return bSuccess;
}

void CTreeOptionsCtrl::OnCreateImageList()
{
	//Load up the image list
	VERIFY(m_ilTree.Create(m_nilID, 16, 1, RGB(255, 0, 255)));
}

void CTreeOptionsCtrl::PreSubclassWindow()
{
	//Let the parent class do its thing
	CTreeCtrl::PreSubclassWindow();

	//Call the virtual function to setup the image list
	OnCreateImageList();

	//Hook it up to the tree control
	SetImageList(&m_ilTree, TVSIL_NORMAL);
}

BOOL CTreeOptionsCtrl::AddComboBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClass, DWORD dwItemData)
{
	ASSERT(pRuntimeClass);

	//Delete the old item data in the item if there is one already
	CTreeOptionsItemData *pOldItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	delete pOldItemData;

	//A pointer to the runtime class is stored in the Item data which itself is an
	//internal structure we maintain per tree item
	CTreeOptionsItemData *pItemData = new CTreeOptionsItemData;
	pItemData->m_dwItemData = dwItemData;
	pItemData->m_pRuntimeClass1 = pRuntimeClass;
	pItemData->m_Type = CTreeOptionsItemData::ComboBox;

	return SetItemData(hItem, (DWORD_PTR)pItemData);
}

CString CTreeOptionsCtrl::GetComboText(HTREEITEM hItem) const
{
	const CString &sText = GetItemText(hItem);
	int nSeparator = sText.Find(m_sSeparator);
	if (nSeparator >= 0)
		return sText.Right(sText.GetLength() - nSeparator - m_sSeparator.GetLength());
	return CString();
}

void CTreeOptionsCtrl::RemoveChildControlText(HTREEITEM hItem)
{
	CString sText = GetItemText(hItem);
	int nSeparator = sText.Find(m_sSeparator);
	if (nSeparator >= 0)
		sText.Truncate(nSeparator);
	SetItemText(hItem, sText);
}

void CTreeOptionsCtrl::SetComboText(HTREEITEM hItem, const CString &sComboText)
{
	CString sText = GetItemText(hItem);
	int nSeparator = sText.Find(m_sSeparator);
	if (nSeparator < 0)
		sText += m_sSeparator;
	else
		sText.Truncate(nSeparator + m_sSeparator.GetLength());

	SetItemText(hItem, sText + sComboText);
}

void CTreeOptionsCtrl::DestroyOldChildControl()
{
	if (m_pCombo) {
		m_pCombo->DestroyWindow();
		delete m_pCombo;
		m_pCombo = NULL;
	}
	if (m_pEdit) {
		m_pEdit->DestroyWindow();
		delete m_pEdit;
		m_pEdit = NULL;
	}
	if (m_pSpin) {
		m_pSpin->DestroyWindow();
		delete m_pSpin;
		m_pSpin = NULL;
	}
	if (m_pButton) {
		m_pButton->DestroyWindow();
		delete m_pButton;
		m_pButton = NULL;
	}
	if (m_pDateTime) {
		m_pDateTime->DestroyWindow();
		delete m_pDateTime;
		m_pDateTime = NULL;
	}
	if (m_pIPAddress) {
		m_pIPAddress->DestroyWindow();
		delete m_pIPAddress;
		m_pIPAddress = NULL;
	}

	//Free up the font object we have been using
	m_Font.DeleteObject();

	m_hControlItem = NULL;
}

void CTreeOptionsCtrl::CreateNewChildControl(HTREEITEM hItem)
{
	ASSERT(hItem);
	m_hControlItem = hItem;

	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);

	//Make a copy of the current font being used by the control
	ASSERT(m_Font.m_hObject == NULL);
	CFont *pFont = GetFont();
	LOGFONT lf;
	pFont->GetLogFont(&lf);
	VERIFY(m_Font.CreateFontIndirect(&lf));

	//Allocate the main control
	ASSERT(pItemData->m_pRuntimeClass1);
	CString sComboText;
	CString sEditText;
	if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsCombo))) {
		//Get the current text in the combo item
		sComboText = GetComboText(hItem);

		//Now that we have the current text remove it from the tree control text
		RemoveChildControlText(hItem);

		//Create the new combo box
		m_pCombo = static_cast<CTreeOptionsCombo*>(pItemData->m_pRuntimeClass1->CreateObject());
		ASSERT(m_pCombo);
		ASSERT(m_pCombo->IsKindOf(RUNTIME_CLASS(CTreeOptionsCombo)));  //Your class must be derived from CTreeOptionsCombo
		m_pCombo->SetTreeBuddy(this);
		m_pCombo->SetTreeItem(hItem);
	} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsEdit))) {
		//Get the current text in the edit box item
		sEditText = GetEditText(hItem);

		//Now that we have the current text remove it from the tree control text
		RemoveChildControlText(hItem);

		//Create the new edit box
		m_pEdit = static_cast<CTreeOptionsEdit*>(pItemData->m_pRuntimeClass1->CreateObject());
		ASSERT(m_pEdit);
		ASSERT(m_pEdit->IsKindOf(RUNTIME_CLASS(CTreeOptionsEdit)));  //Your class must be derived from CTreeOptionsEdit
		m_pEdit->SetTreeBuddy(this);
		m_pEdit->SetTreeItem(hItem);
	} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsDateCtrl))) {
		//Get the current text in the edit box item
		sEditText = GetEditText(hItem);

		//Now that we have the current text remove it from the tree control text
		RemoveChildControlText(hItem);

		//Create the new edit box
		m_pDateTime = static_cast<CTreeOptionsDateCtrl*>(pItemData->m_pRuntimeClass1->CreateObject());
		ASSERT(m_pDateTime);
		ASSERT(m_pDateTime->IsKindOf(RUNTIME_CLASS(CTreeOptionsDateCtrl)));  //Your class must be derived from CTreeOptionsDateCtrl
		m_pDateTime->SetTreeBuddy(this);
		m_pDateTime->SetTreeItem(hItem);
	} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsIPAddressCtrl))) {
		//Get the current text in the edit box item
		sEditText = GetEditText(hItem);

		//Now that we have the current text remove it from the tree control text
		RemoveChildControlText(hItem);

		//Create the new edit box
		m_pIPAddress = static_cast<CTreeOptionsIPAddressCtrl*>(pItemData->m_pRuntimeClass1->CreateObject());
		ASSERT(m_pIPAddress);
		ASSERT(m_pIPAddress->IsKindOf(RUNTIME_CLASS(CTreeOptionsIPAddressCtrl)));  //Your class must be derived from CTreeOptionsIPAddressCtrl
		m_pIPAddress->SetTreeBuddy(this);
		m_pIPAddress->SetTreeItem(hItem);
	} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsBrowseButton))) {
		//Create the new static
		m_pButton = static_cast<CTreeOptionsBrowseButton*>(pItemData->m_pRuntimeClass1->CreateObject());
		ASSERT(m_pButton);
		ASSERT(m_pButton->IsKindOf(RUNTIME_CLASS(CTreeOptionsBrowseButton)));  //Your class must be derived from CTreeOptionsStatic
		m_pButton->SetTreeBuddy(this);
		m_pButton->SetTreeItem(hItem);

		if (pItemData->m_Type == CTreeOptionsItemData::ColorBrowser) {
			//Get the current color from the control and let the button know about it
			COLORREF color = GetColor(hItem);
			m_pButton->SetColor(color);
		} else if (pItemData->m_Type == CTreeOptionsItemData::FontBrowser) {
			GetFontItem(hItem, &lf);
			m_pButton->SetFontItem(&lf);
		} else
			ASSERT(pItemData->m_Type == CTreeOptionsItemData::OpaqueBrowser);
	} else
		ASSERT(0); //Your class must be derived from one of the classes in the previous statements

	//allocate the secondary control
	if (pItemData->m_pRuntimeClass2) {
		if (pItemData->m_pRuntimeClass2->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsSpinCtrl))) {
			m_pSpin = static_cast<CTreeOptionsSpinCtrl*>(pItemData->m_pRuntimeClass2->CreateObject());
			ASSERT(m_pSpin);
			ASSERT(m_pSpin->IsKindOf(RUNTIME_CLASS(CTreeOptionsSpinCtrl)));  //Your class must be derived from CTreeOptionsSpinCtrl
			m_pSpin->SetTreeBuddy(this);
			m_pSpin->SetTreeItem(hItem);
		} else {
			ASSERT(pItemData->m_pRuntimeClass2->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsBrowseButton)));
			m_pButton = static_cast<CTreeOptionsBrowseButton*>(pItemData->m_pRuntimeClass2->CreateObject());
			ASSERT(m_pButton);
			ASSERT(m_pButton->IsKindOf(RUNTIME_CLASS(CTreeOptionsBrowseButton)));  //Your class must be derived from CTreeOptionsBrowseButton
			m_pButton->SetTreeBuddy(this);
			m_pButton->SetTreeItem(hItem);
		}
	}

	//Update the rects for item
	CRect rText;
	GetItemRect(hItem, rText, TRUE);
	RECT rLine;
	GetItemRect(hItem, &rLine, FALSE);

	RECT r;
	r.left = rText.right + 2;
	r.top = rText.top;

	//Now create the main control
	ASSERT(pItemData->m_pRuntimeClass1);
	if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsCombo))) {
		if (m_pButton)
			r.right = rLine.right - m_pButton->GetWidth();
		else
			r.right = rLine.right;
		r.bottom = r.top + m_pCombo->GetDropDownHeight(); //Ask the combo box for the height to use
		m_pCombo->Create(m_pCombo->GetWindowStyle(), r, this, TREE_OPTIONS_COMBOBOX_ID);
		ASSERT(m_pCombo->GetCount()); //You forget to add string items to the combo box in your OnCreate message handler!

		//set the font the combo box should use based on the font in the tree control,
		m_pCombo->SetFont(&m_Font);

		//Also select the right text into the combo box
		DWORD dwComboStyle = m_pCombo->GetStyle();
		BOOL bComboHasEdit = (((dwComboStyle & CBS_DROPDOWN) | (dwComboStyle & CBS_SIMPLE)) != 0);
		if ((dwComboStyle & CBS_DROPDOWNLIST) == CBS_DROPDOWNLIST)
			bComboHasEdit = FALSE;
		if (bComboHasEdit)
			m_pCombo->SetWindowText(sComboText);
		else
			m_pCombo->SelectString(-1, sComboText);

		//Auto select the control if configured to do so
		if (m_bAutoSelect)
			m_pCombo->SetFocus();
	} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsEdit))) {
		if (m_pButton)
			r.right = rLine.right - m_pButton->GetWidth();
		else
			r.right = rLine.right;
		r.bottom = r.top + m_pEdit->GetHeight(rText.Height());
		VERIFY(m_pEdit->CreateEx(WS_EX_CLIENTEDGE, _T("Edit"), sEditText, m_pEdit->GetWindowStyle(), r, this, TREE_OPTIONS_EDITBOX_ID));

		//set the font the edit box should use based on the font in the tree control
		m_pEdit->SetFont(&m_Font);

		//Auto select the control if configured to do so
		if (m_bAutoSelect)
			m_pEdit->SetFocus();
	} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsDateCtrl))) {
		r.right = rLine.right;
		r.bottom = rLine.bottom;
		VERIFY(m_pDateTime->Create(m_pDateTime->GetWindowStyle(), r, this, TREE_OPTIONS_DATETIMECTRL_ID));

		//set the font the date time control should use based on the font in the list control
		m_pDateTime->SetFont(&m_Font);

		//set the value in the control
		m_pDateTime->SetTime(pItemData->m_DateTime);

		//Auto select the control if configured to do so
		if (m_bAutoSelect)
			m_pDateTime->SetFocus();
	} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsIPAddressCtrl))) {
		r.right = rLine.right;
		r.bottom = rLine.bottom;

		VERIFY(m_pIPAddress->Create(m_pIPAddress->GetWindowStyle(), r, this, TREE_OPTIONS_IPADDRESSCTRL_ID));

		//set the font the IP Address control should use based on the font in the list control
		m_pIPAddress->SetFont(&m_Font);

		DWORD dwAddress = GetIPAddress(hItem);
		m_pIPAddress->SetAddress(dwAddress);

		//Auto select the control if configured to do so
		if (m_bAutoSelect)
			m_pIPAddress->SetFocus();
	} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsBrowseButton)))
		CreateBrowseButton(pItemData->m_pRuntimeClass1, rLine, rText);
	else
		ASSERT(0); //Your class must be derived from one of the classes in the statements above

	//Actually create the secondary control
	if (pItemData->m_pRuntimeClass2) {
		if (pItemData->m_pRuntimeClass2->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsSpinCtrl)))
			CreateSpinCtrl(pItemData->m_pRuntimeClass2, rLine, rText, r);
		else {
			ASSERT(pItemData->m_pRuntimeClass2->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsBrowseButton)));
			CreateBrowseButton(pItemData->m_pRuntimeClass2, rLine, rText);

			if (m_pEdit)
				m_pEdit->SetButtonBuddy(m_pButton);
			else {
				ASSERT(m_pCombo);
				m_pCombo->SetButtonBuddy(m_pButton);
			}
		}
	}
}

void CTreeOptionsCtrl::UpdateTreeControlValueFromChildControl(HTREEITEM hItem)
{
	if (m_pCombo) {
		CString sText;
		m_pCombo->GetWindowText(sText);
		SetComboText(m_hControlItem, sText);
	} else if (m_pEdit) {
		CString sText;
		m_pEdit->GetWindowText(sText);
		SetEditText(m_hControlItem, sText);
	} else if (m_pDateTime) {
		SYSTEMTIME st1;
		if (m_pDateTime->GetTime(&st1) == GDT_VALID)
			m_pDateTime->SetDateTime(st1);

		SYSTEMTIME st2;
		m_pDateTime->GetDateTime(st2);
		SetDateTime(m_hControlItem, st2);
		SetEditText(m_hControlItem, m_pDateTime->GetDisplayText(st2));
	} else if (m_pIPAddress) {
		DWORD dwAddress;
		if (m_pIPAddress->GetAddress(dwAddress) == 4) {
			SetIPAddress(m_hControlItem, dwAddress);
			SetEditText(m_hControlItem, m_pIPAddress->GetDisplayText(dwAddress));
		}
	} else if (m_pButton) {
		CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
		ASSERT(pItemData);
		if (pItemData->m_Type == CTreeOptionsItemData::ColorBrowser) {
			COLORREF color = m_pButton->GetColor();
			SetColor(m_hControlItem, color);
		} else if (pItemData->m_Type == CTreeOptionsItemData::FontBrowser) {
			LOGFONT lf;
			GetFontItem(hItem, &lf);
			m_pButton->SetFontItem(&lf);
		}
	}
}

BOOL CTreeOptionsCtrl::AddEditBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClassEditCtrl, DWORD dwItemData)
{
	//Just call the combo box version as currently there is no difference
	BOOL bSuccess = AddComboBox(hItem, pRuntimeClassEditCtrl, dwItemData);

	//Update the type in the item data
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	pItemData->m_Type = CTreeOptionsItemData::EditBox;

	return bSuccess;
}

BOOL CTreeOptionsCtrl::AddEditBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClassEditCtrl, CRuntimeClass *pRuntimeClassSpinCtrl, DWORD dwItemData)
{
	//Add the edit box
	BOOL bSuccess = AddEditBox(hItem, pRuntimeClassEditCtrl, dwItemData);

	//Add the spin ctrl
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	ASSERT(pItemData->m_pRuntimeClass1);
	ASSERT(pItemData->m_pRuntimeClass2 == NULL);
	ASSERT(pRuntimeClassSpinCtrl);
	pItemData->m_pRuntimeClass2 = pRuntimeClassSpinCtrl;
	pItemData->m_Type = CTreeOptionsItemData::Spin;
	pItemData->m_dwItemData = dwItemData;

	return bSuccess;
}

BOOL CTreeOptionsCtrl::AddFileEditBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClassEditCtrl, CRuntimeClass *pRuntimeClassButton, DWORD dwItemData)
{
	//Add the edit box
	BOOL bSuccess = AddEditBox(hItem, pRuntimeClassEditCtrl, dwItemData);

	//Add the browse button
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	ASSERT(pItemData->m_pRuntimeClass1);
	ASSERT(pItemData->m_pRuntimeClass2 == NULL);
	ASSERT(pRuntimeClassButton);
	pItemData->m_pRuntimeClass2 = pRuntimeClassButton;
	pItemData->m_Type = CTreeOptionsItemData::FileBrowser;

	return bSuccess;
}

BOOL CTreeOptionsCtrl::AddColorSelector(HTREEITEM hItem, CRuntimeClass *pRuntimeClassButton, DWORD dwItemData, BOOL bDrawColorForIcon)
{
	//Add the browse button as the primary control
	BOOL bSuccess = AddEditBox(hItem, pRuntimeClassButton, dwItemData);

	//Setup the browser type
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	pItemData->m_Type = CTreeOptionsItemData::ColorBrowser;
	pItemData->m_bDrawColorForIcon = bDrawColorForIcon;

	return bSuccess;
}

COLORREF CTreeOptionsCtrl::GetColor(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	return pItemData->m_Color;
}

void CTreeOptionsCtrl::SetColor(HTREEITEM hItem, COLORREF color)
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	pItemData->m_Color = color;

	//Also update the text while we are at it
	CString sColor;
	sColor.Format(_T("&%02X%02X%02X"), GetRValue(color), GetGValue(color), GetBValue(color));
	SetEditText(hItem, sColor);
}

BOOL CTreeOptionsCtrl::AddFontSelector(HTREEITEM hItem, CRuntimeClass *pRuntimeClassButton, DWORD dwItemData)
{
	//Add the browse button as the primary control
	BOOL bSuccess = AddEditBox(hItem, pRuntimeClassButton, dwItemData);

	//Setup the browser type
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	pItemData->m_Type = CTreeOptionsItemData::FontBrowser;

	return bSuccess;
}

void CTreeOptionsCtrl::GetFontItem(HTREEITEM hItem, LOGFONT *pLogFont) const
{
	ASSERT(pLogFont);
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	memcpy(pLogFont, &pItemData->m_Font, sizeof(LOGFONT));
}

void CTreeOptionsCtrl::SetFontItem(HTREEITEM hItem, const LOGFONT *pLogFont)
{
	ASSERT(pLogFont);
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	memcpy(&pItemData->m_Font, pLogFont, sizeof(LOGFONT));

	//Also update the text while we are at it
	SetEditText(hItem, pLogFont->lfFaceName);
}

CString CTreeOptionsCtrl::GetFileEditText(HTREEITEM hItem) const
{
	//Just call the edit box version as currently there is no difference
	return GetEditText(hItem);
}

void CTreeOptionsCtrl::SetFileEditText(HTREEITEM hItem, const CString &sEditText)
{
	//Just call the edit box version as currently there is no difference
	SetEditText(hItem, sEditText);
}

BOOL CTreeOptionsCtrl::AddFolderEditBox(HTREEITEM hItem, CRuntimeClass *pRuntimeClassEditCtrl, CRuntimeClass *pRuntimeClassButton, DWORD dwItemData)
{
	//Let the File edit box code do all the work
	BOOL bSuccess = AddFileEditBox(hItem, pRuntimeClassEditCtrl, pRuntimeClassButton, dwItemData);

	//Setup the correct edit type in the item data
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	pItemData->m_Type = CTreeOptionsItemData::FolderBrowser;

	return bSuccess;
}

CString CTreeOptionsCtrl::GetFolderEditText(HTREEITEM hItem) const
{
	//Just call the edit box version as currently there is no difference
	return GetEditText(hItem);
}

void CTreeOptionsCtrl::SetFolderEditText(HTREEITEM hItem, const CString &sEditText)
{
	//Just call the edit box version as currently there is no difference
	SetEditText(hItem, sEditText);
}

void CTreeOptionsCtrl::CreateSpinCtrl(CRuntimeClass *pRuntimeClassSpinCtrl, const CRect &rItem, const CRect& /*rText*/, const CRect &rPrimaryControl)
{
	ASSERT(pRuntimeClassSpinCtrl);
	if (pRuntimeClassSpinCtrl->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsSpinCtrl))) {
		//work out the rect this secondary control
		RECT r = { rPrimaryControl.right, rPrimaryControl.top, rItem.right, rPrimaryControl.bottom };

		//Create the new spin control
		ASSERT(m_pSpin);
		m_pSpin->SetEditBuddy(m_pEdit);

		//Create the spin control
		VERIFY(m_pSpin->Create(m_pSpin->GetWindowStyle(), r, this, TREE_OPTIONS_SPINCTRL_ID));

		//Setup the buddy and the default range
		m_pSpin->SetBuddy(m_pEdit);
		int nLower = 0;
		int nUpper = 0;
		m_pSpin->GetDefaultRange(nLower, nUpper);
		m_pSpin->SetRange((short)nLower, (short)nUpper);

		//set the font the edit box should use based on the font in the tree control
		m_pSpin->SetFont(&m_Font);
	} else
		ASSERT(0); //Your class must be derived from CTreeOptionsSpinCtrl
}

void CTreeOptionsCtrl::CreateBrowseButton(CRuntimeClass *pRuntimeClassBrowseButton, const CRect &rItem, const CRect &rText)
{
	ASSERT(pRuntimeClassBrowseButton);
	if (pRuntimeClassBrowseButton->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsBrowseButton))) {
		ASSERT(m_pButton);
		CWnd *owner;
		RECT r;
		r.right = rItem.right;
		r.left = rItem.right - m_pButton->GetWidth(); //Ask the browse button for the width to use
		if (m_pEdit) {
			//work out the rect for the button
			RECT rEdit;
			m_pEdit->GetWindowRect(&rEdit);
			ScreenToClient(&rEdit);
			r.top = rItem.top;
			r.bottom = rEdit.bottom;
			//Create the new browse button
			m_pButton->SetEditBuddy(m_pEdit);
			owner = m_pEdit;
		} else if (m_pCombo) {
			//work out the rect for the button
			RECT rCombo;
			m_pCombo->GetWindowRect(&rCombo);
			ScreenToClient(&rCombo);
			r.top = rItem.top;
			r.bottom = rCombo.bottom;
			//Create the new browse button
			m_pButton->SetComboBuddy(m_pCombo);
			owner = m_pCombo;
		} else {
			//work out the rect for the button
			r.top = rText.top;
			r.bottom = rText.bottom;
			owner = NULL;
		}

		//Create the browse button
		VERIFY(m_pButton->Create(m_pButton->GetCaption(), m_pButton->GetWindowStyle(), r, this, TREE_OPTIONS_BROWSEBUTTONCTRL_ID));
		if (owner)
			m_pButton->SetOwner(owner);
		//set the font the edit box should use based on the font in the tree control
		m_pButton->SetFont(&m_Font);
	} else
		ASSERT(0); //Your class must be derived from CTreeOptionsBrowseButton
}

CString CTreeOptionsCtrl::GetEditText(HTREEITEM hItem) const
{
	//Just call the combo box version as currently there is no difference
	return GetComboText(hItem);
}

void CTreeOptionsCtrl::SetEditText(HTREEITEM hItem, const CString &sEditText)
{
	//Just call the combo box version as currently there is no difference
	SetComboText(hItem, sEditText);
}

BOOL CTreeOptionsCtrl::OnSelchanged(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMTREEVIEW *pNMTreeView = (NMTREEVIEW*)pNMHDR;

	if (!m_bBeingCleared) {
		//Destroy the old combo or edit box if need be
		if (m_hControlItem) {
			UpdateTreeControlValueFromChildControl(m_hControlItem);
			DestroyOldChildControl();
			m_hControlItem = NULL;
		}

		//Create the new control if need be
		if (pNMTreeView->itemNew.hItem != NULL) {
			CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(pNMTreeView->itemNew.hItem));
			if (pItemData && pItemData->m_pRuntimeClass1)
				CreateNewChildControl(pNMTreeView->itemNew.hItem);
		}
	}

	return (BOOL)(*pResult = 0);
}

BOOL CTreeOptionsCtrl::OnItemExpanding(LPNMHDR, LRESULT *pResult)
{
	//Clean up any controls currently open we used (assuming it is a standard
	//scroll message and not from one of our spin controls)
	if (m_hControlItem) {
		UpdateTreeControlValueFromChildControl(m_hControlItem);
		DestroyOldChildControl();
	}

	return (BOOL)(*pResult = 0);
}

void CTreeOptionsCtrl::Clear()
{
	m_bBeingCleared = TRUE;
	HTREEITEM hRoot = GetRootItem();
	m_hControlItem = NULL;
	if (hRoot)
		MemDeleteAllItems(hRoot);
	m_bBeingCleared = FALSE;
}

void CTreeOptionsCtrl::MemDeleteAllItems(HTREEITEM hParent)
{
	HTREEITEM hItem = hParent;
	while (hItem) {
		HTREEITEM hNextItem = CTreeCtrl::GetNextItem(hItem, TVGN_NEXT);
		if (ItemHasChildren(hItem))
			MemDeleteAllItems(GetChildItem(hItem));

		CTreeOptionsItemData *pItem = reinterpret_cast<CTreeOptionsItemData*>(CTreeCtrl::GetItemData(hItem));
		delete pItem;
		SetItemData(hItem, 0);

		//let the base class do its thing
		CTreeCtrl::DeleteItem(hItem);

		//Move on to the next item
		hItem = hNextItem;
	}
}

void CTreeOptionsCtrl::OnDestroy()
{
	//Destroy the old combo or edit box if need be
	DestroyOldChildControl();

	//Delete all the items ourselves, rather than calling CTreeCtrl::DeleteAllItems
	Clear();

	//Let the parent class do its thing
	CTreeCtrl::OnDestroy();
}

BOOL CTreeOptionsCtrl::DeleteAllItems()
{
	Clear();

	//Let the base class do its thing
	return CTreeCtrl::DeleteAllItems();
}

void CTreeOptionsCtrl::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	if (!(pScrollBar && pScrollBar->IsKindOf(RUNTIME_CLASS(CTreeOptionsSpinCtrl)))) {
		//Clean up any controls currently open we used (assuming it is a standard
		//scroll message and not from one of our spin controls)
		if (m_hControlItem) {
			UpdateTreeControlValueFromChildControl(m_hControlItem);
			DestroyOldChildControl();
		}

		//Let the parent class do its thing
		CTreeCtrl::OnVScroll(nSBCode, nPos, pScrollBar);
	}
}

void CTreeOptionsCtrl::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	//Clean up any controls currently open we used
	if (m_hControlItem) {
		UpdateTreeControlValueFromChildControl(m_hControlItem);
		DestroyOldChildControl();
	}

	//Let the parent class do its thing
	CTreeCtrl::OnHScroll(nSBCode, nPos, pScrollBar);
}

BOOL CTreeOptionsCtrl::OnNmClick(LPNMHDR, LRESULT *pResult)
{
	//If the mouse was over the label or icon and the item is a combo box
	//or edit box and editing is currently not active then create the
	//new control
	UINT uFlags = 0;
	CPoint point = GetCurrentMessage()->pt;
	ScreenToClient(&point);
	HTREEITEM hItem = HitTest(point, &uFlags);
	if (hItem) {
		CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
		if ((uFlags & TVHT_ONITEM) && pItemData && pItemData->m_pRuntimeClass1 && m_hControlItem == NULL)
			CreateNewChildControl(hItem);

		//Auto select the control if configured to do so
		if (m_bAutoSelect)
			PostMessage(WM_TOC_SETFOCUS_TO_CHILD);
	}

	return (BOOL)(*pResult = 0);
}

void CTreeOptionsCtrl::OnKillFocus(CWnd *pNewWnd)
{
	//Clean up any controls currently open if we are losing focus to something else
	BOOL bForeignWnd = (m_hControlItem && (pNewWnd != m_pCombo) && (pNewWnd != m_pEdit) &&
		(pNewWnd != m_pDateTime) && (pNewWnd != m_pIPAddress) && (pNewWnd != m_pButton));
	if (bForeignWnd && m_pCombo)
		bForeignWnd = !m_pCombo->IsRelatedWnd(pNewWnd);
	if (bForeignWnd && m_pDateTime)
		bForeignWnd = !m_pDateTime->IsRelatedWnd(pNewWnd);
	if (bForeignWnd && m_pIPAddress)
		bForeignWnd = !m_pIPAddress->IsRelatedWnd(pNewWnd);

	if (bForeignWnd) {
		UpdateTreeControlValueFromChildControl(GetSelectedItem());
		DestroyOldChildControl();
	}

	//Let the parent class do its thing
	CTreeCtrl::OnKillFocus(pNewWnd);
}

void CTreeOptionsCtrl::HandleChildControlLosingFocus()
{
	//Clean up any controls currently open we used
	//if we are losing focus to something else
	UpdateTreeControlValueFromChildControl(GetSelectedItem());
	DestroyOldChildControl();
}

int CTreeOptionsCtrl::GetIndentPostion(HTREEITEM hItem) const
{
	UINT uIndent = GetIndent();

	int nAncestors = -1;
	while (hItem) {
		hItem = GetParentItem(hItem);
		++nAncestors;
	}

	return nAncestors * uIndent;
}

BOOL CTreeOptionsCtrl::OnCustomDraw(LPNMHDR pNMHDR, LRESULT *pResult)
{
	NMTVCUSTOMDRAW *pCustomDraw = reinterpret_cast<NMTVCUSTOMDRAW*>(pNMHDR);
	switch (pCustomDraw->nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
		*pResult = CDRF_NOTIFYITEMDRAW; //Tell the control that we are interested in item notifications
		break;
	case CDDS_ITEMPREPAINT:
		//Just let me know about post painting
		*pResult = CDRF_NOTIFYPOSTPAINT;
		break;
	case CDDS_ITEMPOSTPAINT:
		{
			HTREEITEM hItem = (HTREEITEM)pCustomDraw->nmcd.dwItemSpec;

			BOOL bDrawColor = FALSE;
			CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
			if (pItemData && pItemData->m_Type == CTreeOptionsItemData::ColorBrowser && pItemData->m_bDrawColorForIcon)
				bDrawColor = TRUE;

			if (bDrawColor) {
				//Draw the icon of the tree view item using the specified color
				CDC dc;
				dc.Attach(pCustomDraw->nmcd.hdc);
				RECT r;

				r.left = pCustomDraw->nmcd.rc.left;
				r.top = pCustomDraw->nmcd.rc.top;
				r.bottom = pCustomDraw->nmcd.rc.bottom;

				//Allow for the indent
				r.left += GetIndentPostion(hItem);

				r.right = r.left + 16;
				dc.FillSolidRect(&r, GetColor(hItem));
				dc.Detach();
			}
			*pResult = CDRF_DODEFAULT;
		}
		break;
	default:
		;
	}

	return TRUE; //Allow the message to be reflected again
}

BOOL CTreeOptionsCtrl::AddDateTime(HTREEITEM hItem, CRuntimeClass *pRuntimeClassDateTime, DWORD dwItemData)
{
	//Add the date time control as the primary control
	BOOL bSuccess = AddComboBox(hItem, pRuntimeClassDateTime, dwItemData);

	//Setup the item type
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	pItemData->m_Type = CTreeOptionsItemData::DateTimeCtrl;

	return bSuccess;
}

void CTreeOptionsCtrl::GetDateTime(HTREEITEM hItem, SYSTEMTIME &st) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	memcpy(&st, &pItemData->m_DateTime, sizeof(SYSTEMTIME));
}

void CTreeOptionsCtrl::SetDateTime(HTREEITEM hItem, const SYSTEMTIME &st)
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	memcpy(&pItemData->m_DateTime, &st, sizeof(SYSTEMTIME));

	//Also update the text while we are at it
	CTreeOptionsDateCtrl *pTempDateTime = static_cast<CTreeOptionsDateCtrl*>(pItemData->m_pRuntimeClass1->CreateObject());
	ASSERT(pTempDateTime);
	ASSERT(pTempDateTime->IsKindOf(RUNTIME_CLASS(CTreeOptionsDateCtrl)));  //Your class must be derived from CTreeOptionsDateCtrl
	const CString &sDateTime = pTempDateTime->GetDisplayText(st);
	SetEditText(hItem, sDateTime);
	delete pTempDateTime;
}

BOOL CTreeOptionsCtrl::AddIPAddress(HTREEITEM hItem, CRuntimeClass *pRuntimeClassDateTime, DWORD dwItemData)
{
	//Add the date time control as the primary control
	BOOL bSuccess = AddComboBox(hItem, pRuntimeClassDateTime, dwItemData);

	//Setup the item type
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	pItemData->m_Type = CTreeOptionsItemData::IPAddressCtrl;

	return bSuccess;
}

DWORD CTreeOptionsCtrl::GetIPAddress(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	return pItemData->m_dwIPAddress;
}

void CTreeOptionsCtrl::SetIPAddress(HTREEITEM hItem, DWORD dwAddress)
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	pItemData->m_dwIPAddress = dwAddress;

	//Also update the text while we are at it
	CTreeOptionsIPAddressCtrl *pTempIPAddress = static_cast<CTreeOptionsIPAddressCtrl*>(pItemData->m_pRuntimeClass1->CreateObject());
	ASSERT(pTempIPAddress);
	ASSERT(pTempIPAddress->IsKindOf(RUNTIME_CLASS(CTreeOptionsIPAddressCtrl)));  //Your class must be derived from CTreeOptionsIPAddressCtrl
	const CString &sIPAddress = pTempIPAddress->GetDisplayText(dwAddress);
	SetEditText(hItem, sIPAddress);
	delete pTempIPAddress;
}

BOOL CTreeOptionsCtrl::AddOpaque(HTREEITEM hItem, CRuntimeClass *pRuntimeClass1, CRuntimeClass *pRuntimeClass2, DWORD dwItemData)
{
	//Add the first class
	BOOL bSuccess = AddComboBox(hItem, pRuntimeClass1, dwItemData);

	//Add the second class
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	ASSERT(pItemData->m_pRuntimeClass1);
	ASSERT(pItemData->m_pRuntimeClass2 == NULL);
	pItemData->m_pRuntimeClass2 = pRuntimeClass2;

	//Setup the browser type
	pItemData->m_Type = CTreeOptionsItemData::OpaqueBrowser;

	return bSuccess;
}

DWORD CTreeOptionsCtrl::GetOpaque(HTREEITEM hItem) const
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	return pItemData->m_dwItemData;
}

void CTreeOptionsCtrl::SetOpaque(HTREEITEM hItem, DWORD dwItemData)
{
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(GetItemData(hItem));
	ASSERT(pItemData);
	pItemData->m_dwItemData = dwItemData;
}





IMPLEMENT_DYNCREATE(CTreeOptionsCombo, CComboBox)

CTreeOptionsCombo::CTreeOptionsCombo()
	: m_pTreeCtrl()
	, m_pButtonCtrl()
	, m_hTreeCtrlItem()
{
}

BEGIN_MESSAGE_MAP(CTreeOptionsCombo, CComboBox)
	//{{AFX_MSG_MAP(CTreeOptionsCombo)
	ON_WM_CHAR()
	ON_WM_GETDLGCODE()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

UINT CTreeOptionsCombo::OnGetDlgCode()
{
	return CComboBox::OnGetDlgCode() | DLGC_WANTTAB;
}

void CTreeOptionsCombo::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_TAB) {
		ASSERT(m_pTreeCtrl);
		m_pTreeCtrl->SetFocus();
	} else {
		//Pass on to the parent since we didn't handle it
		CComboBox::OnChar(nChar, nRepCnt, nFlags);
	}
}

DWORD CTreeOptionsCombo::GetWindowStyle()
{
	return WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST;
}

int CTreeOptionsCombo::GetDropDownHeight()
{
	return 100;
}

BOOL CTreeOptionsCombo::IsRelatedWnd(CWnd *pChild)
{
	BOOL bRelated = FALSE;
	if (pChild) {
		CWnd *pWnd = pChild;
		while (pWnd && !bRelated) {
			pWnd = pWnd->GetParent();
			bRelated = (pWnd == this);
		}
	}

	if (!bRelated) {
		ASSERT(m_pTreeCtrl);
		bRelated = (pChild == m_pTreeCtrl->m_pButton);
		if (!bRelated)
			bRelated = (pChild == m_pTreeCtrl->m_pSpin);
	}

	return bRelated;
}

void CTreeOptionsCombo::OnKillFocus(CWnd *pNewWnd)
{
	//Let the parent class do its thing
	CComboBox::OnKillFocus(pNewWnd);

	if (!IsRelatedWnd(pNewWnd)) {
		//update the tree control and close this window
		ASSERT(m_pTreeCtrl);
		m_pTreeCtrl->HandleChildControlLosingFocus();
	}
}



IMPLEMENT_DYNCREATE(CTreeOptionsFontNameCombo, CTreeOptionsCombo)

BEGIN_MESSAGE_MAP(CTreeOptionsFontNameCombo, CTreeOptionsCombo)
	//{{AFX_MSG_MAP(CTreeOptionsFontNameCombo)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CTreeOptionsFontNameCombo::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	//Let the parent class do its thing
	if (CTreeOptionsCombo::OnCreate(lpCreateStruct) == -1)
		return -1;

	//Enumerate all the fonts
	CDC *pDC = GetDC();
	ASSERT(pDC);
	EnumFonts(pDC->m_hDC, NULL, _EnumFontProc, (LPARAM)this);
	ReleaseDC(pDC);

	return 0;
}

int CTreeOptionsFontNameCombo::EnumFontProc(CONST LOGFONT *lplf, CONST TEXTMETRIC* /*lptm*/,
	DWORD /*dwType*/)
{
	//Add the font name to the combo box
	AddString(lplf->lfFaceName);

	return 1; //To continue enumeration
}

int CALLBACK CTreeOptionsFontNameCombo::_EnumFontProc(CONST LOGFONT *lplf, CONST TEXTMETRIC *lptm
	, DWORD dwType, LPARAM lpData) noexcept
{
	//Convert from the SDK world to the C++ world
	CTreeOptionsFontNameCombo *pThis = reinterpret_cast<CTreeOptionsFontNameCombo*>(lpData);
	ASSERT(pThis);
	return pThis->EnumFontProc(lplf, lptm, dwType);
}

DWORD CTreeOptionsFontNameCombo::GetWindowStyle()
{
	return WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_SORT;
}






//The following line is to fix a bug in VC 6 where the CDateTimeCtrl
//does not correctly expose its runtime information when you link
//to MFC as a DLL
#ifdef _AFXDLL
IMPLEMENT_DYNAMIC(CDateTimeCtrl, CWnd)
#endif

IMPLEMENT_DYNCREATE(CTreeOptionsDateCtrl, CDateTimeCtrl)

CTreeOptionsDateCtrl::CTreeOptionsDateCtrl()
	: m_pTreeCtrl()
	, m_hTreeCtrlItem()
	, m_bDoNotDestroyUponLoseFocus()
	, m_SystemTime()
{
}

CTreeOptionsDateCtrl::~CTreeOptionsDateCtrl()
{
}

BEGIN_MESSAGE_MAP(CTreeOptionsDateCtrl, CDateTimeCtrl)
	//{{AFX_MSG_MAP(CTreeOptionsDateCtrl)
	ON_WM_CHAR()
	ON_WM_GETDLGCODE()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

UINT CTreeOptionsDateCtrl::OnGetDlgCode()
{
	return CDateTimeCtrl::OnGetDlgCode() | DLGC_WANTTAB;
}

void CTreeOptionsDateCtrl::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_TAB) {
		ASSERT(m_pTreeCtrl);
		m_pTreeCtrl->SetFocus();
	} else {
		//Pass on to the parent since we didn't handle it
		CDateTimeCtrl::OnChar(nChar, nRepCnt, nFlags);
	}
}

DWORD CTreeOptionsDateCtrl::GetWindowStyle()
{
	return WS_CHILD | WS_VISIBLE | DTS_SHORTDATEFORMAT;
}

void CTreeOptionsDateCtrl::OnKillFocus(CWnd *pNewWnd)
{
	//Let the parent class do its thing
	CDateTimeCtrl::OnKillFocus(pNewWnd);

	//update the list control and close this window
	if (!IsRelatedWnd(pNewWnd)) {
		ASSERT(m_pTreeCtrl);
		m_pTreeCtrl->HandleChildControlLosingFocus();
	}
}

CString CTreeOptionsDateCtrl::GetDisplayText(const SYSTEMTIME &st)
{
	TCHAR sDate[100];
	sDate[0] = _T('\0');
	GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, sDate, 100);
	return sDate;
}

BOOL CTreeOptionsDateCtrl::IsRelatedWnd(CWnd *pChild)
{
	return (GetMonthCalCtrl() == pChild);
}




IMPLEMENT_DYNCREATE(CTreeOptionsTimeCtrl, CTreeOptionsDateCtrl)

CTreeOptionsTimeCtrl::CTreeOptionsTimeCtrl()
{
}

CTreeOptionsTimeCtrl::~CTreeOptionsTimeCtrl()
{
}

BEGIN_MESSAGE_MAP(CTreeOptionsTimeCtrl, CTreeOptionsDateCtrl)
	//{{AFX_MSG_MAP(CTreeOptionsTimeCtrl)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CString CTreeOptionsTimeCtrl::GetDisplayText(const SYSTEMTIME &st)
{
	TCHAR sTime[100];
	sTime[0] = _T('\0');
	GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, sTime, 100);
	return sTime;
}

DWORD CTreeOptionsTimeCtrl::GetWindowStyle()
{
	return WS_CHILD | WS_VISIBLE | DTS_TIMEFORMAT;
}



IMPLEMENT_DYNCREATE(CTreeOptionsIPAddressCtrl, CIPAddressCtrl)

CTreeOptionsIPAddressCtrl::CTreeOptionsIPAddressCtrl()
	: m_pTreeCtrl()
	, m_hTreeCtrlItem()
	, m_bDoNotDestroyUponLoseFocus()
	, m_dwAddress()
{
}

CTreeOptionsIPAddressCtrl::~CTreeOptionsIPAddressCtrl()
{
}

BEGIN_MESSAGE_MAP(CTreeOptionsIPAddressCtrl, CIPAddressCtrl)
	//{{AFX_MSG_MAP(CTreeOptionsIPAddressCtrl)
	ON_WM_CHAR()
	ON_WM_GETDLGCODE()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

UINT CTreeOptionsIPAddressCtrl::OnGetDlgCode()
{
	return CIPAddressCtrl::OnGetDlgCode() | DLGC_WANTTAB;
}

void CTreeOptionsIPAddressCtrl::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_TAB) {
		ASSERT(m_pTreeCtrl);
		m_pTreeCtrl->SetFocus();
	} else {
		//Pass on to the parent since we didn't handle it
		CIPAddressCtrl::OnChar(nChar, nRepCnt, nFlags);
	}
}

DWORD CTreeOptionsIPAddressCtrl::GetWindowStyle()
{
	return WS_CHILD | WS_VISIBLE;
}

void CTreeOptionsIPAddressCtrl::OnKillFocus(CWnd *pNewWnd)
{
	//Let the parent class do its thing
	CIPAddressCtrl::OnKillFocus(pNewWnd);

	if (!IsRelatedWnd(pNewWnd)) {
		//update the list control and close this window
		ASSERT(m_pTreeCtrl);
		m_pTreeCtrl->HandleChildControlLosingFocus();
	}
}

CString CTreeOptionsIPAddressCtrl::GetDisplayText(DWORD dwAddress)
{
	CString sAddress;
	sAddress.Format(_T("%lu.%lu.%lu.%lu"), (dwAddress >> 24) & 0xFF,
		(dwAddress >> 16) & 0xFF, (dwAddress >> 8) & 0xFF,
		(dwAddress & 0xFF));
	return sAddress;
}

BOOL CTreeOptionsIPAddressCtrl::IsRelatedWnd(CWnd *pChild)
{
	BOOL bAncestor = FALSE;
	if (pChild) {
		CWnd *pWnd = pChild;
		while (pWnd && !bAncestor) {
			pWnd = pWnd->GetParent();
			bAncestor = (pWnd == this);
		}
	}

	return bAncestor;
}










IMPLEMENT_DYNCREATE(CTreeOptionsBooleanCombo, CTreeOptionsCombo)

CTreeOptionsBooleanCombo::CTreeOptionsBooleanCombo()
{
}

CTreeOptionsBooleanCombo::~CTreeOptionsBooleanCombo()
{
}

BEGIN_MESSAGE_MAP(CTreeOptionsBooleanCombo, CTreeOptionsCombo)
	//{{AFX_MSG_MAP(CTreeOptionsBooleanCombo)
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

int CTreeOptionsBooleanCombo::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	//Let the parent class do its thing
	if (CTreeOptionsCombo::OnCreate(lpCreateStruct) == -1)
		return -1;
	//Add the two boolean strings
	CString sText;
	VERIFY(sText.LoadString(IDS_TREEOPTIONS_TRUE));
	AddString(sText);
	VERIFY(sText.LoadString(IDS_TREEOPTIONS_FALSE));
	AddString(sText);
	return 0;
}








IMPLEMENT_DYNCREATE(CTreeOptionsEdit, CEdit)

CTreeOptionsEdit::CTreeOptionsEdit()
	:m_pTreeCtrl()
	, m_pButtonCtrl()
	, m_hTreeCtrlItem()
	, m_bDoNotDestroyUponLoseFocus()
{
}

CTreeOptionsEdit::~CTreeOptionsEdit()
{
}

BEGIN_MESSAGE_MAP(CTreeOptionsEdit, CEdit)
	//{{AFX_MSG_MAP(CTreeOptionsEdit)
	ON_WM_CHAR()
	ON_WM_GETDLGCODE()
	ON_WM_KILLFOCUS()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

UINT CTreeOptionsEdit::OnGetDlgCode()
{
	return CEdit::OnGetDlgCode() | DLGC_WANTTAB;
}

void CTreeOptionsEdit::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_TAB) {
		ASSERT(m_pTreeCtrl);
		if (m_pTreeCtrl->m_pButton)
			m_pTreeCtrl->m_pButton->SetFocus();
		else
			m_pTreeCtrl->SetFocus();
	} else {
		//Pass on to the parent since we didn't handle it
		CEdit::OnChar(nChar, nRepCnt, nFlags);
	}
}

DWORD CTreeOptionsEdit::GetWindowStyle()
{
	return WS_VISIBLE | WS_CHILD | ES_LEFT | ES_AUTOHSCROLL;
}

int CTreeOptionsEdit::GetHeight(int nItemHeight)
{
	return max(nItemHeight, 20);
}

void CTreeOptionsEdit::OnKillFocus(CWnd *pNewWnd)
{
	//Let the parent class do its thing
	CEdit::OnKillFocus(pNewWnd);

	//update the tree control and close this window
	ASSERT(m_pTreeCtrl);
	if (pNewWnd != m_pButtonCtrl) {
		ASSERT(m_pTreeCtrl);
		m_pTreeCtrl->HandleChildControlLosingFocus();
	}
}

CString CTreeOptionsEdit::GetBrowseForFolderCaption()
{
	return _T("Please specify a folder");
}

CString CTreeOptionsEdit::GetBrowseForFileCaption()
{
	return _T("Please specify a file");
}

CString CTreeOptionsEdit::GetFileExtensionFilter()
{
	return _T("All Files (*.*)|*.*||");
}

int CALLBACK CTreeOptionsEdit::SHBrowseSetSelProc(HWND hWnd, UINT uMsg, LPARAM, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED)
		::SendMessage(hWnd, BFFM_SETSELECTION, TRUE, lpData);

	return 0;
}

void CTreeOptionsEdit::BrowseForFolder(const CString &sInitialFolder)
{
	ASSERT(m_pTreeCtrl);

	//Bring up a standard directory chooser dialog
	TCHAR sDisplayName[_MAX_PATH];
	BROWSEINFO bi;
	bi.hwndOwner = m_pTreeCtrl->GetSafeHwnd();
	bi.pidlRoot = NULL;
	const CString &sCaption = GetBrowseForFolderCaption();
	bi.lpszTitle = sCaption;
	bi.pszDisplayName = sDisplayName;
	bi.ulFlags = BIF_RETURNONLYFSDIRS;
	bi.lpfn = SHBrowseSetSelProc;
	bi.lParam = (LPARAM)(LPCTSTR)sInitialFolder;
	LPITEMIDLIST pItemIDList = SHBrowseForFolder(&bi);

	if (pItemIDList) {
		//Retrieve the path and update on screen
		TCHAR sPath[_MAX_PATH];
		if (SHGetPathFromIDList(pItemIDList, sPath))
			SetWindowText(sPath);

		//avoid memory leaks by deleting the PIDL
		//using the shells task allocator
		IMalloc *pMalloc;
		if (SUCCEEDED(SHGetMalloc(&pMalloc))) {
			pMalloc->Free(pItemIDList);
			pMalloc->Release();
		}
	}
}

void CTreeOptionsEdit::BrowseForFile(const CString &sInitialFile)
{
	ASSERT(m_pTreeCtrl);

	//Create the special file save dialog
	CTreeOptionsFileDialog dlg(TRUE, NULL, sInitialFile, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, GetFileExtensionFilter(), m_pTreeCtrl);

	//Modify the title to the desired value
	const CString &sCaption = GetBrowseForFileCaption();
	dlg.m_ofn.lpstrTitle = sCaption;

	//bring up the dialog and if hit OK set the text in this control to the new filename
	if (dlg.DoModal() == IDOK)
		SetWindowText(dlg.GetPathName());
}




IMPLEMENT_DYNCREATE(CTreeOptionsSpinCtrl, CSpinButtonCtrl)

CTreeOptionsSpinCtrl::CTreeOptionsSpinCtrl()
	: m_pTreeCtrl()
	, m_hTreeCtrlItem()
	, m_pEdit()
{
}

BEGIN_MESSAGE_MAP(CTreeOptionsSpinCtrl, CSpinButtonCtrl)
	//{{AFX_MSG_MAP(CTreeOptionsSpinCtrl)
	ON_WM_KILLFOCUS()
	ON_WM_CHAR()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CTreeOptionsSpinCtrl::SetTreeBuddy(CTreeOptionsCtrl *pTreeCtrl)
{
	m_pTreeCtrl = pTreeCtrl;
}

void CTreeOptionsSpinCtrl::SetEditBuddy(CTreeOptionsEdit *pEdit)
{
	m_pEdit = pEdit;
}

DWORD CTreeOptionsSpinCtrl::GetWindowStyle()
{
	return WS_VISIBLE | WS_CHILD | UDS_ARROWKEYS | UDS_SETBUDDYINT | UDS_NOTHOUSANDS | UDS_ALIGNRIGHT;
}

void CTreeOptionsSpinCtrl::GetDefaultRange(int &lower, int &upper)
{
	lower = 0;
	upper = 100;
}

void CTreeOptionsSpinCtrl::OnKillFocus(CWnd *pNewWnd)
{
	//Let the parent class do its thing
	CSpinButtonCtrl::OnKillFocus(pNewWnd);

	//update the tree control and close this window
	ASSERT(m_pTreeCtrl);
	if (pNewWnd != m_pEdit)
		m_pTreeCtrl->HandleChildControlLosingFocus();
}




IMPLEMENT_DYNCREATE(CTreeOptionsBrowseButton, CButton)

CTreeOptionsBrowseButton::CTreeOptionsBrowseButton()
	: m_Color()
	, m_Font()
	, m_pTreeCtrl()
	, m_pEdit()
	, m_pCombo()
	, m_hTreeCtrlItem()
{
}

CTreeOptionsBrowseButton::~CTreeOptionsBrowseButton()
{
}

BEGIN_MESSAGE_MAP(CTreeOptionsBrowseButton, CButton)
	//{{AFX_MSG_MAP(CTreeOptionsBrowseButton)
	ON_WM_KILLFOCUS()
	ON_WM_CHAR()
	ON_CONTROL_REFLECT(BN_CLICKED, OnClicked)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CTreeOptionsBrowseButton::SetTreeBuddy(CTreeOptionsCtrl *pTreeCtrl)
{
	m_pTreeCtrl = pTreeCtrl;
}

void CTreeOptionsBrowseButton::SetEditBuddy(CTreeOptionsEdit *pEdit)
{
	m_pEdit = pEdit;
}

void CTreeOptionsBrowseButton::SetComboBuddy(CTreeOptionsCombo *pCombo)
{
	m_pCombo = pCombo;
}

DWORD CTreeOptionsBrowseButton::GetWindowStyle()
{
	return WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON;
}

CString CTreeOptionsBrowseButton::GetCaption()
{
	return _T("...");
}

int CTreeOptionsBrowseButton::GetWidth()
{
	ASSERT(m_pTreeCtrl);

	CDC *pDC = m_pTreeCtrl->GetDC();
	int nButtonWidth = 0;
	if (pDC) {
		//Get the button width
		CSize SizeText = pDC->GetTextExtent(_T("    "), 4); //We add space around text button
		pDC->LPtoDP(&SizeText);
		nButtonWidth = SizeText.cx;
		const CString &sText = GetCaption();
		SizeText = pDC->GetTextExtent(sText, sText.GetLength());
		pDC->LPtoDP(&SizeText);
		nButtonWidth += SizeText.cx;

		m_pTreeCtrl->ReleaseDC(pDC);
	} else
		nButtonWidth = 20;

	return nButtonWidth;
}

void CTreeOptionsBrowseButton::OnKillFocus(CWnd *pNewWnd)
{
	//Let the parent class do its thing
	CButton::OnKillFocus(pNewWnd);

	//update the tree control and close this window
	ASSERT(m_pTreeCtrl);
	if (m_pEdit) {
		if ((pNewWnd != m_pTreeCtrl->m_pEdit) && !m_pEdit->m_bDoNotDestroyUponLoseFocus) {
			ASSERT(m_pTreeCtrl);
			m_pTreeCtrl->HandleChildControlLosingFocus();
		}
	}
}

void CTreeOptionsBrowseButton::OnClicked()
{
	ASSERT(m_pTreeCtrl);

	//Get the text currently in the control
	HTREEITEM hSelItem = m_pTreeCtrl->GetSelectedItem();
	ASSERT(hSelItem);

	//Pull out the item data associated with the selected item
	CTreeOptionsItemData *pItemData = reinterpret_cast<CTreeOptionsItemData*>(m_pTreeCtrl->GetItemData(hSelItem));
	ASSERT(pItemData);
	if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsEdit))) {
		ASSERT(m_pEdit);

		//Decide on what dialog to bring up, and call the appropriate virtual function
		if (pItemData->m_Type == CTreeOptionsItemData::FileBrowser) {
			m_pEdit->m_bDoNotDestroyUponLoseFocus = TRUE;
			CString sText;
			m_pEdit->GetWindowText(sText);
			m_pEdit->BrowseForFile(sText);
			m_pEdit->m_bDoNotDestroyUponLoseFocus = FALSE;
		} else if (pItemData->m_Type == CTreeOptionsItemData::FolderBrowser) {
			m_pEdit->m_bDoNotDestroyUponLoseFocus = TRUE;
			CString sText;
			m_pEdit->GetWindowText(sText);
			m_pEdit->BrowseForFolder(sText);
			m_pEdit->m_bDoNotDestroyUponLoseFocus = FALSE;
		} else if (pItemData->m_Type == CTreeOptionsItemData::OpaqueBrowser)
			BrowseForOpaque();
		else
			ASSERT(0);
	} else if (pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsCombo))) {
		ASSERT(m_pCombo);

		if (pItemData->m_Type == CTreeOptionsItemData::OpaqueBrowser)
			BrowseForOpaque();
	} else {
		ASSERT(pItemData->m_pRuntimeClass1->IsDerivedFrom(RUNTIME_CLASS(CTreeOptionsBrowseButton)));

		if (pItemData->m_Type == CTreeOptionsItemData::ColorBrowser)
			BrowseForColor();
		else if (pItemData->m_Type == CTreeOptionsItemData::FontBrowser)
			BrowseForFont();
		else if (pItemData->m_Type == CTreeOptionsItemData::OpaqueBrowser)
			BrowseForOpaque();
		else
			ASSERT(0);
	}
}

void CTreeOptionsBrowseButton::BrowseForColor()
{
	ASSERT(m_pTreeCtrl);

	//Bring up a standard color selector dialog
	CColorDialog dialog(GetColor());
	dialog.m_cc.Flags |= CC_FULLOPEN;
	if (dialog.DoModal() == IDOK) {
		SetColor(dialog.GetColor());
		m_pTreeCtrl->SetColor(m_pTreeCtrl->GetSelectedItem(), m_Color);

		//Ask the tree control to reposition the button if need be
		m_pTreeCtrl->PostMessage(WM_TOC_REPOSITION_CHILD_CONTROL);
	}
}

void CTreeOptionsBrowseButton::BrowseForFont()
{
	ASSERT(m_pTreeCtrl);

	//Bring up a standard color selector dialog
	CFontDialog dialog(&m_Font);
	if (dialog.DoModal() == IDOK) {
		dialog.GetCurrentFont(&m_Font);
		m_pTreeCtrl->SetFontItem(m_pTreeCtrl->GetSelectedItem(), &m_Font);

		//Ask the tree control to reposition the button if need be
		m_pTreeCtrl->PostMessage(WM_TOC_REPOSITION_CHILD_CONTROL);
	}
}

void CTreeOptionsBrowseButton::BrowseForOpaque()
{
	ASSERT(0); //Derived classes must implement this function if we are editing
	//an opaque item. The code which "normally" display some CDialog
	//derived class to allow the item to be edited and then hive the
	//data away somehow so that it can show the new value when the
	//dialog is brought up again. Following is some pseudo code which
	//would do this.

	/*
	ASSERT(m_pTreeCtrl);

	//Bring up our custom opaque editor dialog
	CMyOpaque *pQpaque = (CMyOpaque*) m_pTreeCtrl->GetOpaque(m_hTreeCtrlItem);
	CMyOpaqueDialog dialog;
	dialog.SetOpaque(pOpaque);
	if (dialog.DoModal() == IDOK) {
		pOpaque = dialog.GetOpaque();
		m_pTreeCtrl->SetOpaque(m_hTreeCtrlItem, (DWORD) pOpaque);
		m_pTreeCtrl->SetEditText(m_hTreeCtrlItem, pOpaque->m_sSomeName);
	}
	*/
}

void CTreeOptionsBrowseButton::SetColor(COLORREF color)
{
	m_Color = color;
}

void CTreeOptionsBrowseButton::GetFontItem(LOGFONT *pLogFont)
{
	ASSERT(pLogFont);
	memcpy(pLogFont, &m_Font, sizeof(LOGFONT));
}

void CTreeOptionsBrowseButton::SetFontItem(const LOGFONT *pLogFont)
{
	ASSERT(pLogFont);
	memcpy(&m_Font, pLogFont, sizeof(LOGFONT));
}



IMPLEMENT_DYNAMIC(CTreeOptionsFileDialog, CFileDialog)

CTreeOptionsFileDialog::CTreeOptionsFileDialog(BOOL bOpenFileDialog, LPCTSTR lpszDefExt, LPCTSTR lpszFileName, DWORD dwFlags, LPCTSTR lpszFilter,
	CWnd *pParentWnd) : CFileDialog(bOpenFileDialog, lpszDefExt, lpszFileName, dwFlags, lpszFilter, pParentWnd)
{
}

BEGIN_MESSAGE_MAP(CTreeOptionsFileDialog, CFileDialog)
	//{{AFX_MSG_MAP(CTreeOptionsFileDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

void CTreeOptionsFileDialog::OnInitDone()
{
	CString sText;
	if (!sText.LoadString(IDS_TREEOPTIONS_OK))
		ASSERT(0);
	LPTSTR pszBuffer = sText.GetBuffer(sText.GetLength());

	//modify the text on the IDOK button to OK
	CommDlg_OpenSave_SetControlText(GetParent()->m_hWnd, IDOK, pszBuffer);

	sText.ReleaseBuffer();
}





void DDX_TreeCheck(CDataExchange *pDX, int nIDC, HTREEITEM hItem, BOOL &bCheck)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		VERIFY(pCtrlTreeOptions->GetCheckBox(hItem, bCheck));
	else
		VERIFY(pCtrlTreeOptions->SetCheckBox(hItem, bCheck));
}

void DDX_TreeRadio(CDataExchange *pDX, int nIDC, HTREEITEM hParent, int &nIndex)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate) {
		HTREEITEM hCheckItem;
		VERIFY(pCtrlTreeOptions->GetRadioButton(hParent, nIndex, hCheckItem));
	} else
		VERIFY(pCtrlTreeOptions->SetRadioButton(hParent, nIndex));
}

void DDX_TreeEdit(CDataExchange *pDX, int nIDC, HTREEITEM hItem, CString &sText)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		sText = pCtrlTreeOptions->GetEditText(hItem);
	else
		pCtrlTreeOptions->SetEditText(hItem, sText);
}

void DDX_TreeEdit(CDataExchange *pDX, int nIDC, HTREEITEM hItem, int &nValue)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		nValue = _ttoi(pCtrlTreeOptions->GetEditText(hItem));
	else {
		CString sText;
		sText.Format(_T("%d"), nValue);
		pCtrlTreeOptions->SetEditText(hItem, sText);
	}
}

void DDX_TreeCombo(CDataExchange *pDX, int nIDC, HTREEITEM hItem, CString &sText)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		sText = pCtrlTreeOptions->GetComboText(hItem);
	else
		pCtrlTreeOptions->SetComboText(hItem, sText);
}

void DDX_TreeFileEdit(CDataExchange *pDX, int nIDC, HTREEITEM hItem, CString &sText)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		sText = pCtrlTreeOptions->GetFileEditText(hItem);
	else
		pCtrlTreeOptions->SetFileEditText(hItem, sText);
}

void DDX_TreeFolderEdit(CDataExchange *pDX, int nIDC, HTREEITEM hItem, CString &sText)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		sText = pCtrlTreeOptions->GetFolderEditText(hItem);
	else
		pCtrlTreeOptions->SetFolderEditText(hItem, sText);
}

void DDX_TreeColor(CDataExchange *pDX, int nIDC, HTREEITEM hItem, COLORREF &color)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		color = pCtrlTreeOptions->GetColor(hItem);
	else
		pCtrlTreeOptions->SetColor(hItem, color);
}

void DDX_TreeFont(CDataExchange *pDX, int nIDC, HTREEITEM hItem, LOGFONT *pLogFont)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		pCtrlTreeOptions->GetFontItem(hItem, pLogFont);
	else
		pCtrlTreeOptions->SetFontItem(hItem, pLogFont);
}

void DDX_TreeDateTime(CDataExchange *pDX, int nIDC, HTREEITEM hItem, SYSTEMTIME &st)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		pCtrlTreeOptions->GetDateTime(hItem, st);
	else
		pCtrlTreeOptions->SetDateTime(hItem, st);
}

void DDX_TreeIPAddress(CDataExchange *pDX, int nIDC, HTREEITEM hItem, DWORD &dwAddress)
{
	HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
	CTreeOptionsCtrl *pCtrlTreeOptions = static_cast<CTreeOptionsCtrl*>(CWnd::FromHandlePermanent(hWndCtrl));
	ASSERT(pCtrlTreeOptions);
	ASSERT(pCtrlTreeOptions->IsKindOf(RUNTIME_CLASS(CTreeOptionsCtrl)));

	if (pDX->m_bSaveAndValidate)
		dwAddress = pCtrlTreeOptions->GetIPAddress(hItem);
	else
		pCtrlTreeOptions->SetIPAddress(hItem, dwAddress);
}

void DDX_TreeBoolean(CDataExchange *pDX, int nIDC, HTREEITEM hItem, BOOL &bValue)
{
	//Convert from the boolean to a string if we are transferring to the control
	CString sText;
	if (!pDX->m_bSaveAndValidate) {
		if (bValue)
			VERIFY(sText.LoadString(IDS_TREEOPTIONS_TRUE));
		else
			VERIFY(sText.LoadString(IDS_TREEOPTIONS_FALSE));
	}

	//Pass the buck to the combo DDX function
	DDX_TreeCombo(pDX, nIDC, hItem, sText);

	//Convert from the string to the boolean if we are transferring from the control
	if (pDX->m_bSaveAndValidate) {
		CString sCompare;
		VERIFY(sCompare.LoadString(IDS_TREEOPTIONS_TRUE));
		if (sText == sCompare)
			bValue = TRUE;
		else
			bValue = FALSE;
	}
}

HTREEITEM CTreeOptionsCtrl::CopyItem(HTREEITEM hItem, HTREEITEM htiNewParent, HTREEITEM htiAfter)
{
	//Get the details of the item to copy
	TVINSERTSTRUCT tvstruct;
	tvstruct.item.hItem = hItem;
	tvstruct.item.mask = TVIF_CHILDREN | TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
	GetItem(&tvstruct.item);
	CString sText = GetItemText(hItem);
	tvstruct.item.cchTextMax = sText.GetLength();
	tvstruct.item.pszText = sText.GetBuffer(tvstruct.item.cchTextMax);
	if (tvstruct.item.lParam)
		tvstruct.item.lParam = (LPARAM)new CTreeOptionsItemData(*(reinterpret_cast<CTreeOptionsItemData*>(tvstruct.item.lParam)));

	//Insert the item at the proper location
	tvstruct.hParent = htiNewParent;
	tvstruct.hInsertAfter = htiAfter;
	tvstruct.item.mask |= TVIF_TEXT;
	HTREEITEM hNewItem = InsertItem(&tvstruct);

	//Don't forget to release the CString buffer
	sText.ReleaseBuffer();

	return hNewItem;
}

HTREEITEM CTreeOptionsCtrl::CopyBranch(HTREEITEM htiBranch, HTREEITEM htiNewParent, HTREEITEM htiAfter)
{
	HTREEITEM hNewItem = CopyItem(htiBranch, htiNewParent, htiAfter);
	HTREEITEM hChild = GetChildItem(htiBranch);
	while (hChild != NULL) {
		//recursively transfer all the items
		CopyBranch(hChild, hNewItem);
		hChild = GetNextSiblingItem(hChild);
	}
	return hNewItem;
}