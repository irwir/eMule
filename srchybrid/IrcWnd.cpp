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
#include "emuleDlg.h"
#include "IrcWnd.h"
#include "IrcMain.h"
#include "otherfunctions.h"
#include "MenuCmds.h"
#include "HTRichEditCtrl.h"
#include "ClosableTabCtrl.h"
#include "HelpIDs.h"
#include "opcodes.h"
#include "InputBox.h"
#include "UserMsgs.h"
#include "ColourPopup.h"
#include "SmileySelector.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Request from IRC-folks: don't use similar colors for Status and Info messages
#define	INFO_MSG_COLOR		RGB(127,  0,  0)	// dark red
#define	STATUS_MSG_COLOR	RGB(  0,147,  0)	// dark green
#define	EVENT_MSG_COLOR		RGB(  0,  0,127)	// dark blue

#define	TIME_STAMP_FORMAT		_T("[%H:%M:%S] ")

/*
#pragma warning(push)
#pragma warning(disable:4125) // decimal digit terminates octal escape sequence
static LPCTSTR const s_szTimeStampColorPrefix = _T("\00302");		// dark blue
#pragma warning(pop)
*/
static LPCTSTR const s_szTimeStampColorPrefix = _T("");	// default foreground color
LPCTSTR const CIrcWnd::sBadCharsIRC = _T(" \t!@#$%^&*():;<>,.?~+=");

#define	SPLITTER_HORZ_MARGIN	0
#define	SPLITTER_HORZ_WIDTH		4
#define	SPLITTER_HORZ_RANGE_MIN	170
#define	SPLITTER_HORZ_RANGE_MAX	400

IMPLEMENT_DYNAMIC(CIrcWnd, CDialog)

BEGIN_MESSAGE_MAP(CIrcWnd, CResizableDialog)
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_CONTEXTMENU()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_CTLCOLOR()
	ON_WM_HELPINFO()
	ON_MESSAGE(UM_CLOSETAB, OnCloseTab)
	ON_MESSAGE(UM_QUERYTAB, OnQueryTab)
	ON_MESSAGE(UM_CPN_SELENDOK, OnSelEndOK)
	ON_MESSAGE(UM_CPN_SELENDCANCEL, OnSelEndCancel)
	ON_NOTIFY(EN_REQUESTRESIZE, IDC_TITLEWINDOW, OnEnRequestResizeTitle)
END_MESSAGE_MAP()

CIrcWnd::CIrcWnd(CWnd *pParent)
	: CResizableDialog(CIrcWnd::IDD, pParent)
	, m_pIrcMain()
	, m_pwndSmileySel()
	, m_bLoggedIn()
	, m_bConnected()
{
	m_wndNicks.m_pParent = this;
	m_wndChanList.m_pParent = this;
	m_wndChanSel.m_bClosable = true;
	m_wndChanSel.m_pParent = this;
}

CIrcWnd::~CIrcWnd()
{
	if (m_bConnected)
		m_pIrcMain->Disconnect(true);
	delete m_pIrcMain;
	if (m_pwndSmileySel != NULL) {
		m_pwndSmileySel->DestroyWindow();
		delete m_pwndSmileySel;
	}
}

void CIrcWnd::OnSysColorChange()
{
	CResizableDialog::OnSysColorChange();
	SetAllIcons();
}

void CIrcWnd::SetAllIcons()
{
	CImageList iml;
	iml.Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1);
	iml.Add(CTempIconLoader(_T("Smiley_Smile")));
	iml.Add(CTempIconLoader(_T("Bold")));
	iml.Add(CTempIconLoader(_T("Italic")));
	iml.Add(CTempIconLoader(_T("Underline")));
	iml.Add(CTempIconLoader(_T("Colour")));
	iml.Add(CTempIconLoader(_T("ResetFormat")));
	CImageList *pImlOld = m_wndFormat.SetImageList(&iml);
	iml.Detach();
	if (pImlOld)
		pImlOld->DeleteImageList();
}

void CIrcWnd::Localize()
{
	SetDlgItemText(IDC_BN_IRCCONNECT, GetResString(m_bConnected ? IDS_IRC_DISCONNECT : IDS_IRC_CONNECT));
	SetDlgItemText(IDC_CHATSEND, GetResString(IDS_IRC_SEND));
	SetDlgItemText(IDC_CLOSECHAT, GetResString(IDS_FD_CLOSE));
	m_wndChanList.Localize();
	m_wndChanSel.Localize();
	m_wndNicks.Localize();

	m_wndFormat.SetBtnText(IDC_SMILEY, _T("Smileys"));
	m_wndFormat.SetBtnText(IDC_BOLD, GetResString(IDS_BOLD));
	m_wndFormat.SetBtnText(IDC_ITALIC, GetResString(IDS_ITALIC));
	m_wndFormat.SetBtnText(IDC_UNDERLINE, GetResString(IDS_UNDERLINE));
	m_wndFormat.SetBtnText(IDC_COLOUR, GetResString(IDS_COLOUR));
	m_wndFormat.SetBtnText(IDC_RESET, GetResString(IDS_RESETFORMAT));
}

BOOL CIrcWnd::OnInitDialog()
{
	CResizableDialog::OnInitDialog();

	m_bConnected = false;
	m_bLoggedIn = false;
	m_pIrcMain = new CIrcMain();
	m_pIrcMain->SetIRCWnd(this);

	UpdateFonts(&theApp.m_fontHyperText);
	InitWindowStyles(this);
	SetAllIcons();

	m_wndInput.SetLimitText(MAX_IRC_MSG_LEN);
	if (theApp.m_fontChatEdit.m_hObject) {
		m_wndInput.SendMessage(WM_SETFONT, (WPARAM)theApp.m_fontChatEdit.m_hObject, FALSE);
		RECT rcEdit;
		m_wndInput.GetWindowRect(&rcEdit);
		ScreenToClient(&rcEdit);
		rcEdit.top -= 2;
		rcEdit.bottom += 2;
		m_wndInput.MoveWindow(&rcEdit, FALSE);
	}

	RECT rcSpl;
	m_wndNicks.GetWindowRect(&rcSpl);
	ScreenToClient(&rcSpl);
	rcSpl.left = rcSpl.right + SPLITTER_HORZ_MARGIN;
	rcSpl.right = rcSpl.left + SPLITTER_HORZ_WIDTH;
	m_wndSplitterHorz.Create(WS_CHILD | WS_VISIBLE, rcSpl, this, IDC_SPLITTER_IRC);

	AddAnchor(IDC_BN_IRCCONNECT, BOTTOM_LEFT);
	AddAnchor(IDC_CLOSECHAT, BOTTOM_LEFT);
	AddAnchor(IDC_CHATSEND, BOTTOM_RIGHT);
	AddAnchor(m_wndFormat, BOTTOM_LEFT);
	AddAnchor(m_wndInput, BOTTOM_LEFT, BOTTOM_RIGHT);
	AddAnchor(m_wndNicks, TOP_LEFT, BOTTOM_LEFT);
	AddAnchor(m_wndChanList, TOP_LEFT, BOTTOM_RIGHT);
	AddAnchor(m_wndChanSel, TOP_LEFT, TOP_RIGHT);
	AddAnchor(m_wndSplitterHorz, TOP_LEFT, BOTTOM_LEFT);

	// Vista: Remove the TBSTYLE_TRANSPARENT to avoid flickering (can be done only after the toolbar was initially created with TBSTYLE_TRANSPARENT !?)
	m_wndFormat.ModifyStyle((theApp.m_ullComCtrlVer >= MAKEDLLVERULL(6, 16, 0, 0)) ? TBSTYLE_TRANSPARENT : 0, TBSTYLE_TOOLTIPS);
	m_wndFormat.SetExtendedStyle(m_wndFormat.GetExtendedStyle() | TBSTYLE_EX_MIXEDBUTTONS);

	TBBUTTON atb[6] = {};
	atb[0].iBitmap = 0;
	atb[0].idCommand = IDC_SMILEY;
	atb[0].fsState = TBSTATE_ENABLED;
	atb[0].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb[0].iString = -1;

	atb[1].iBitmap = 1;
	atb[1].idCommand = IDC_BOLD;
	atb[1].fsState = TBSTATE_ENABLED;
	atb[1].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb[1].iString = -1;

	atb[2].iBitmap = 2;
	atb[2].idCommand = IDC_ITALIC;
	atb[2].fsState = TBSTATE_ENABLED;
	atb[2].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb[2].iString = -1;

	atb[3].iBitmap = 3;
	atb[3].idCommand = IDC_UNDERLINE;
	atb[3].fsState = TBSTATE_ENABLED;
	atb[3].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb[3].iString = -1;

	atb[4].iBitmap = 4;
	atb[4].idCommand = IDC_COLOUR;
	atb[4].fsState = TBSTATE_ENABLED;
	atb[4].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb[4].iString = -1;

	atb[5].iBitmap = 5;
	atb[5].idCommand = IDC_RESET;
	atb[5].fsState = TBSTATE_ENABLED;
	atb[5].fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE;
	atb[5].iString = -1;
	m_wndFormat.AddButtons(_countof(atb), atb);

	SIZE size;
	m_wndFormat.GetMaxSize(&size);
	::SetWindowPos(m_wndFormat, NULL, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

	int iPosStatInit = rcSpl.left;
	int iPosStatNew = thePrefs.GetSplitterbarPositionIRC();
	if (iPosStatNew > SPLITTER_HORZ_RANGE_MAX)
		iPosStatNew = SPLITTER_HORZ_RANGE_MAX;
	else if (iPosStatNew < SPLITTER_HORZ_RANGE_MIN)
		iPosStatNew = SPLITTER_HORZ_RANGE_MIN;
	rcSpl.left = iPosStatNew;
	rcSpl.right = iPosStatNew + SPLITTER_HORZ_WIDTH;
	if (iPosStatNew != iPosStatInit) {
		m_wndSplitterHorz.MoveWindow(&rcSpl);
		DoResize(iPosStatNew - iPosStatInit);
	}

	Localize();
	m_wndChanList.Init();
	m_wndNicks.Init();
	m_wndNicks.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	m_wndChanSel.Init();
	GetDlgItem(IDC_CLOSECHAT)->EnableWindow(false);
	OnChatTextChange();

	return true;
}

void CIrcWnd::DoResize(int iDelta)
{
	CSplitterControl::ChangeWidth(&m_wndNicks, iDelta);
	m_wndNicks.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	CSplitterControl::ChangeWidth(&m_wndChanList, -iDelta, CW_RIGHTALIGN);
	CSplitterControl::ChangeWidth(&m_wndChanSel, -iDelta, CW_RIGHTALIGN);

	if (m_wndChanSel.m_pCurrentChannel && m_wndChanSel.m_pCurrentChannel->m_wndLog.m_hWnd) {
		RECT rcChannelPane;
		m_wndChanList.GetWindowRect(&rcChannelPane);
		ScreenToClient(&rcChannelPane);
		Channel *pChannel = m_wndChanSel.m_pCurrentChannel;
		if (pChannel->m_wndTopic.m_hWnd)
			CSplitterControl::ChangeWidth(&pChannel->m_wndTopic, -iDelta, CW_RIGHTALIGN);
		if (pChannel->m_wndSplitter.m_hWnd)
			CSplitterControl::ChangeWidth(&pChannel->m_wndSplitter, -iDelta, CW_RIGHTALIGN);
		if (pChannel->m_wndLog.m_hWnd)
			CSplitterControl::ChangeWidth(&pChannel->m_wndLog, -iDelta, CW_RIGHTALIGN);
	}

	RECT rcSpl;
	m_wndSplitterHorz.GetWindowRect(&rcSpl);
	ScreenToClient(&rcSpl);
	thePrefs.SetSplitterbarPositionIRC(rcSpl.left);

	RemoveAnchor(IDC_BN_IRCCONNECT);
	AddAnchor(IDC_BN_IRCCONNECT, BOTTOM_LEFT);
	RemoveAnchor(IDC_CLOSECHAT);
	AddAnchor(IDC_CLOSECHAT, BOTTOM_LEFT);
	RemoveAnchor(m_wndFormat);
	AddAnchor(m_wndFormat, BOTTOM_LEFT);
	RemoveAnchor(m_wndInput);
	AddAnchor(m_wndInput, BOTTOM_LEFT, BOTTOM_RIGHT);
	RemoveAnchor(m_wndNicks);
	AddAnchor(m_wndNicks, TOP_LEFT, BOTTOM_LEFT);
	RemoveAnchor(m_wndChanList);
	AddAnchor(m_wndChanList, TOP_LEFT, BOTTOM_RIGHT);
	RemoveAnchor(m_wndChanSel);
	AddAnchor(m_wndChanSel, TOP_LEFT, TOP_RIGHT);
	RemoveAnchor(m_wndSplitterHorz);
	AddAnchor(m_wndSplitterHorz, TOP_LEFT, BOTTOM_LEFT);

	RECT rcWnd;
	GetWindowRect(&rcWnd);
	ScreenToClient(&rcWnd);
	m_wndSplitterHorz.SetRange(rcWnd.left + SPLITTER_HORZ_RANGE_MIN + SPLITTER_HORZ_WIDTH / 2
		, rcWnd.left + SPLITTER_HORZ_RANGE_MAX - SPLITTER_HORZ_WIDTH / 2);

	Invalidate();
	UpdateWindow();
}

LRESULT CIrcWnd::DefWindowProc(UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	switch (uMessage) {
	case WM_PAINT:
		if (m_wndSplitterHorz) {
			CRect rcWnd;
			GetWindowRect(rcWnd);
			if (rcWnd.Width() > 0) {
				RECT rcSpl;
				m_wndNicks.GetWindowRect(&rcSpl);
				ScreenToClient(&rcSpl);
				rcSpl.left = rcSpl.right + SPLITTER_HORZ_MARGIN;
				rcSpl.right = rcSpl.left + SPLITTER_HORZ_WIDTH;
				m_wndSplitterHorz.MoveWindow(&rcSpl, TRUE);
			}
		}
		break;
	case WM_NOTIFY:
		if (wParam == IDC_SPLITTER_IRC) {
			SPC_NMHDR *pHdr = reinterpret_cast<SPC_NMHDR*>(lParam);
			DoResize(pHdr->delta);
		} else if (wParam == IDC_SPLITTER_IRC_CHANNEL) {
			SPC_NMHDR *pHdr = reinterpret_cast<SPC_NMHDR*>(lParam);
			if (m_wndChanSel.m_pCurrentChannel) {
				CSplitterControl::ChangeHeight(&m_wndChanSel.m_pCurrentChannel->m_wndTopic, pHdr->delta);
				m_wndChanSel.m_pCurrentChannel->m_wndTopic.ScrollToFirstLine();
				CSplitterControl::ChangeHeight(&m_wndChanSel.m_pCurrentChannel->m_wndLog, -pHdr->delta, CW_BOTTOMALIGN);
			}
		}
		break;
	case WM_SIZE:
		if (m_wndSplitterHorz) {
			RECT rcWnd;
			GetWindowRect(&rcWnd);
			ScreenToClient(&rcWnd);
			m_wndSplitterHorz.SetRange(rcWnd.left + SPLITTER_HORZ_RANGE_MIN + SPLITTER_HORZ_WIDTH / 2,
				rcWnd.left + SPLITTER_HORZ_RANGE_MAX - SPLITTER_HORZ_WIDTH / 2);
		}
	}
	return CResizableDialog::DefWindowProc(uMessage, wParam, lParam);
}

void CIrcWnd::UpdateFonts(CFont *pFont)
{
	TCITEM tci;
	tci.mask = TCIF_PARAM;
	for (int iIndex = 0; m_wndChanSel.GetItem(iIndex, &tci); ++iIndex) {
		Channel *pChannel = reinterpret_cast<Channel*>(tci.lParam);
		if (pChannel->m_wndTopic.m_hWnd != NULL) {
			pChannel->m_wndTopic.SetFont(pFont);
			pChannel->m_wndTopic.ScrollToFirstLine();
		}
		if (pChannel->m_wndLog.m_hWnd != NULL)
			pChannel->m_wndLog.SetFont(pFont);
	}
}

void CIrcWnd::UpdateChannelChildWindowsSize()
{
	if (m_wndChanSel.m_pCurrentChannel) {
		Channel *pChannel = m_wndChanSel.m_pCurrentChannel;
		CRect rcChannelPane;
		m_wndChanList.GetWindowRect(rcChannelPane);
		ScreenToClient(rcChannelPane);

		if (pChannel->m_wndTopic.m_hWnd) {
			CRect rcTopic;
			pChannel->m_wndTopic.GetWindowRect(rcTopic);
			pChannel->m_wndTopic.SetWindowPos(NULL, rcChannelPane.left, rcChannelPane.top, rcChannelPane.Width(), rcTopic.Height(), SWP_NOZORDER);
			rcChannelPane.top += rcTopic.Height();
			pChannel->m_wndTopic.ScrollToFirstLine();

			if (pChannel->m_wndSplitter.m_hWnd) {
				CRect rcSplitter;
				pChannel->m_wndSplitter.GetWindowRect(rcSplitter);
				pChannel->m_wndSplitter.SetWindowPos(NULL, rcChannelPane.left, rcChannelPane.top, rcChannelPane.Width(), rcSplitter.Height(), SWP_NOZORDER);
				rcChannelPane.top += rcSplitter.Height();
			}
		}

		if (pChannel->m_wndLog.m_hWnd)
			pChannel->m_wndLog.SetWindowPos(NULL, rcChannelPane.left, rcChannelPane.top, rcChannelPane.Width(), rcChannelPane.Height(), SWP_NOZORDER);
	}
}

void CIrcWnd::OnSize(UINT uType, int iCx, int iCy)
{
	CResizableDialog::OnSize(uType, iCx, iCy);
	UpdateChannelChildWindowsSize();
}

int CIrcWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	return CResizableDialog::OnCreate(lpCreateStruct);
}

void CIrcWnd::DoDataExchange(CDataExchange *pDX)
{
	CResizableDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_NICKLIST, m_wndNicks);
	DDX_Control(pDX, IDC_INPUTWINDOW, m_wndInput);
	DDX_Control(pDX, IDC_SERVERCHANNELLIST, m_wndChanList);
	DDX_Control(pDX, IDC_TAB2, m_wndChanSel);
	DDX_Control(pDX, IDC_TEXT_FORMAT, m_wndFormat);
}

BOOL CIrcWnd::OnCommand(WPARAM wParam, LPARAM)
{
	switch (wParam) {
	case IDC_BN_IRCCONNECT:
		OnBnClickedIrcConnect();
		break;
	case IDC_CLOSECHAT:
		OnBnClickedCloseChannel();
		break; // return TRUE;
	case IDC_CHATSEND:
		OnBnClickedIrcSend();
		break;
	case IDC_BOLD:
		OnBnClickedBold();
		break;
	case IDC_ITALIC:
		OnBnClickedItalic();
		break;
	case IDC_COLOUR:
		OnBnClickedColour();
		break;
	case IDC_UNDERLINE:
		OnBnClickedUnderline();
		break;
	case IDC_RESET:
		OnBnClickedReset();
		break;
	case IDC_SMILEY:
		OnBnClickedSmiley();
	default:
		return TRUE;
	}
	m_wndInput.SetFocus(); //set focus to edit control
	return TRUE;
}

BOOL CIrcWnd::PreTranslateMessage(MSG *pMsg)
{
	if (theApp.emuledlg->m_pSplashWnd)
		return FALSE;
	if (pMsg->message == WM_KEYDOWN) {
		// Don't handle Ctrl+Tab in this window. It will be handled by main window.
		if (pMsg->wParam == VK_TAB && GetKeyState(VK_CONTROL) < 0)
			return FALSE;

		if (pMsg->hwnd == m_wndInput)
			switch (pMsg->wParam) {
			case VK_RETURN: //treat enter key as "Send" button
				OnBnClickedIrcSend();
				return TRUE;
			case VK_UP: //line up/line down scroll through input history
			case VK_DOWN:
				m_wndChanSel.ScrollHistory(pMsg->wParam == VK_DOWN);
				return TRUE;
			case VK_TAB:
				m_wndChanSel.AutoComplete();
				return TRUE;
			}
	}
	OnChatTextChange();
	return CResizableDialog::PreTranslateMessage(pMsg);
}

void CIrcWnd::OnBnClickedIrcConnect()
{
	if (!m_bConnected) {
		//close all channels, private conversations and channel list
		for (POSITION pos = m_wndChanSel.m_lstChannels.GetHeadPosition(); pos != NULL;) {
			Channel *pToDel = m_wndChanSel.m_lstChannels.GetNext(pos);
			if (pToDel->m_eType != Channel::ctStatus)
				m_wndChanSel.RemoveChannel(pToDel);
		}

		CString sInput(thePrefs.GetIRCNick());
		sInput = sInput.Trim().SpanExcluding(sBadCharsIRC).Left(25);
		while (sInput.IsEmpty() || sInput.CompareNoCase(_T("emule")) == 0 || stristr(sInput, _T("emuleirc")) != NULL) {
			InputBox inputBox;
			inputBox.SetLabels(GetResString(IDS_IRC_NEWNICK), GetResString(IDS_IRC_NEWNICKDESC), sInput);
			if (inputBox.DoModal() != IDOK)
				return;
			sInput = inputBox.GetInput();
			sInput = sInput.Trim().SpanExcluding(sBadCharsIRC).Left(25);
			if (sInput[0] < _T('A'))
				sInput.Empty();
		}
		thePrefs.SetIRCNick(sInput);
		//if not connected, connect.
		m_pIrcMain->Connect();
	} else
		//If connected, disconnect.
		m_pIrcMain->Disconnect();
}

void CIrcWnd::OnBnClickedCloseChannel(int iItem)
{
	//Remove a channel.
	TCITEM item;
	item.mask = TCIF_PARAM;
	if (iItem == -1)
		//If no item was send, get our current channel.
		iItem = m_wndChanSel.GetCurSel();

	if (iItem == -1)
		//We have no channel, abort.
		return;

	if (!m_wndChanSel.GetItem(iItem, &item))
		//We had no valid item here. Something isn't right.
		//TODO: this should never happen, so maybe we should remove this tab?
		return;

	Channel *pPartChannel = reinterpret_cast<Channel*>(item.lParam);
	if (pPartChannel->m_eType == Channel::ctNormal && !pPartChannel->m_bDetached && m_bConnected)
		//If this was a channel and we were connected, do not just delete the channel!!
		//Send 'part' command and the server must respond with a successful part which will remove the channel!
		m_pIrcMain->SendString(_T("PART ") + pPartChannel->m_sName);
	else if (pPartChannel->m_eType == Channel::ctNormal || pPartChannel->m_eType == Channel::ctPrivate || pPartChannel->m_eType == Channel::ctChannelList)
		//If this is a channel list, just remove it
		//If this is a private room, we just remove it as the server doesn't track this.
		//If this was a channel, but we are disconnected, remove the channel.
		m_wndChanSel.RemoveChannel(pPartChannel);
}

bool CIrcWnd::UpdateModes(const CString &sAllModes, CString &sModes, TCHAR cDir, TCHAR cCommand)
{
	if (sAllModes.Find(cCommand) < 0)
		return false; //mode not in the list

	//Remove the setting. This takes care of "-" and makes sure we don't add the same symbol twice.
	sModes.Remove(cCommand);
	if (cDir == _T('+')) {
		//Update mode.
		sModes += cCommand;
		//There may have been other modes. Lets make sure we put things in order.
		const CString nModes(sModes);
		int i = sModes.GetLength();
		for (int iIndex = sAllModes.GetLength(); --iIndex >= 0;)
			if (nModes.Find(sAllModes[iIndex]) >= 0)
				sModes.SetAt(--i, sAllModes[iIndex]);
	}
	return true;
}

CString make_time_stamp()
{
	return thePrefs.GetIRCAddTimeStamp() ? s_szTimeStampColorPrefix + CTime::GetCurrentTime().Format(TIME_STAMP_FORMAT) : CString();
}

void CIrcWnd::AddStatus(const CString &sLine, bool bShowActivity, UINT uStatusCode)
{
	Channel *pStatusChannel = m_wndChanSel.m_pChanStatus;
	if (!pStatusChannel)
		return;
	// This allows for us to add blank lines to the status.
	if (sLine.IsEmpty())
		pStatusChannel->m_wndLog.AppendText(_T("\r\n"));
	else {
		CString cs(make_time_stamp());
		COLORREF clr;
		if (sLine[0] == _T('*'))
			clr = STATUS_MSG_COLOR;
		else if (uStatusCode >= 400) {
			if (sLine[0] != _T('-'))
				cs += _T("-Error- ");
			clr = INFO_MSG_COLOR;
		} else
			clr = CLR_DEFAULT;
		cs.AppendFormat(_T("%s\r\n"), (LPCTSTR)sLine);
		AddColorLine(cs, pStatusChannel->m_wndLog, clr);
	}
	if (m_wndChanSel.m_pCurrentChannel != pStatusChannel) {
		if (bShowActivity)
			m_wndChanSel.SetActivity(pStatusChannel, true);
		if (uStatusCode >= 400 && m_wndChanSel.m_pCurrentChannel != m_wndChanSel.m_pChanList && !m_wndChanSel.m_pCurrentChannel->m_bDetached) {
			CString s;
			s.Format(&_T("-Error- %s\r\n")[sLine[0] != _T('-') ? 0 : 8], (LPCTSTR)sLine);
			AddInfoMessage(m_wndChanSel.m_pCurrentChannel, s);
		}
	}
}

void CIrcWnd::AddStatusF(LPCTSTR sLine, ...)
{
	va_list argptr;
	va_start(argptr, sLine);
	CString sTemp;
	sTemp.FormatV(sLine, argptr);
	va_end(argptr);
	AddStatus(sTemp);
}

void CIrcWnd::AddInfoMessage(Channel *pChannel, const CString &sLine)
{
	CString cs = make_time_stamp() + sLine;
	if (sLine[0] == _T('*'))
		AddColorLine(cs, pChannel->m_wndLog, STATUS_MSG_COLOR);
	else if (sLine[0] == _T('-') && sLine.Find(_T('-'), 1) >= 0)
		AddColorLine(cs, pChannel->m_wndLog, INFO_MSG_COLOR);
	else
		AddColorLine(cs, pChannel->m_wndLog);
	if (pChannel != m_wndChanSel.m_pCurrentChannel)
		m_wndChanSel.SetActivity(pChannel, true);
}

void CIrcWnd::AddInfoMessage(const CString &sChannel, const CString &sLine, const bool bShowChannel)
{
	Channel *pChannel = m_wndChanSel.FindOrCreateChannel(sChannel);
	if (pChannel) {
		if (bShowChannel)
			m_wndChanSel.SelectChannel(pChannel);
		AddInfoMessage(pChannel, sLine + _T("\r\n"));
	}
}

void CIrcWnd::AddInfoMessageC(Channel *pChannel, const COLORREF &msgcolour, LPCTSTR sLine)
{
	if (pChannel) {
		CString cs;
		cs.Format(_T("%s%s\r\n"), (LPCTSTR)make_time_stamp(), (LPCTSTR)sLine);
		AddColorLine(cs, pChannel->m_wndLog, msgcolour);
		if (pChannel != m_wndChanSel.m_pCurrentChannel)
			m_wndChanSel.SetActivity(pChannel, true);
	}
}

void CIrcWnd::AddInfoMessageC(const CString &sChannel, const COLORREF &msgcolour, LPCTSTR sLine)
{
	AddInfoMessageC(m_wndChanSel.FindOrCreateChannel(sChannel), msgcolour, sLine);
}

void CIrcWnd::AddInfoMessageCF(const CString &sChannel, const COLORREF &msgcolour, LPCTSTR sLine, ...)
{
	if (!sChannel.IsEmpty()) {
		va_list argptr;
		va_start(argptr, sLine);
		CString sTemp;
		sTemp.FormatV(sLine, argptr);
		va_end(argptr);
		AddInfoMessageC(sChannel, msgcolour, sTemp);
	}
}

void CIrcWnd::AddInfoMessageF(const CString &sChannel, LPCTSTR sLine, ...)
{
	if (!sChannel.IsEmpty()) {
		va_list argptr;
		va_start(argptr, sLine);
		CString sTemp;
		sTemp.FormatV(sLine, argptr);
		va_end(argptr);
		AddInfoMessage(sChannel, sTemp);
	}
}

#pragma warning(push)
#pragma warning(disable:4125) // decimal digit terminates octal escape sequence
void CIrcWnd::AddMessage(const CString &sChannel, const CString &sTargetName, const CString &sLine)
{
	if (sChannel.IsEmpty() || sTargetName.IsEmpty())
		return;
	Channel *pChannel = m_wndChanSel.FindChannelByName(sChannel);
	CString sOp;
	if (!pChannel)
		pChannel = m_wndChanSel.NewChannel(sChannel, (sChannel[0] == _T('#') ? Channel::ctNormal : Channel::ctPrivate));
	else {
		const Nick *pNick = m_wndNicks.FindNickByName(pChannel, sTargetName);
		if (pNick)
			sOp = pNick->m_sSymbols;
	}
	LPCTSTR fmt = (m_pIrcMain->GetNick() == sTargetName) ? _T("%s<\00310%s%s\003> %s\r\n") : _T("%s<\002\00310%s%s\003\002> %s\r\n");
	CString cs;
	cs.Format(fmt, (LPCTSTR)make_time_stamp(), (LPCTSTR)sOp, (LPCTSTR)sTargetName, (LPCTSTR)sLine);
	AddColorLine(cs, pChannel->m_wndLog);
	if (m_wndChanSel.m_pCurrentChannel != pChannel)
		m_wndChanSel.SetActivity(pChannel, true);
}
#pragma warning(pop)

void CIrcWnd::AddMessageF(const CString &sChannel, const CString &sTargetName, LPCTSTR sLine, ...)
{
	if (!sChannel.IsEmpty() && !sTargetName.IsEmpty()) {
		va_list argptr;
		va_start(argptr, sLine);
		CString sTemp;
		sTemp.FormatV(sLine, argptr);
		va_end(argptr);
		AddMessage(sChannel, sTargetName, sTemp);
	}
}

static const COLORREF s_aColors[16] =
{
	RGB(0xff, 0xff, 0xff),	// 00: white
	RGB(0,		 0,    0),	// 01: black
	RGB(0,		 0, 0x7f),	// 02: dark blue
	RGB(0,	  0x93,    0),	// 03: dark green
	RGB(0xff,    0,    0),	// 04: red
	RGB(0x7f,    0,    0),	// 05: dark red
	RGB(0x9c,    0, 0x9c),	// 06: purple
	RGB(0xfc, 0x7f,    0),	// 07: orange
	RGB(0xff, 0xff,    0),	// 08: yellow
	RGB(0,	  0xff,    0),	// 09: green
	RGB(0,	  0x7f, 0x7f),	// 10: dark cyan
	RGB(0,	  0xff, 0xff),	// 11: cyan
	RGB(0,		 0, 0xff),	// 12: blue
	RGB(0xff,    0, 0xff),	// 13: pink
	RGB(0x7f, 0x7f, 0x7f),	// 14: dark grey
	RGB(0xd2, 0xd2, 0xd2)	// 15: light grey
};

bool IsValidURLTerminationChar(TCHAR ch)
{
	// truncate some special chars from end (and only from end), those
	// are the same chars which are supported (not supported actually)
	// by rich edit control auto url detection.
	return _tcschr(_T("^!\"&()=?´`{}[]@+*~#,.-;:_"), ch) == NULL;
}

void CIrcWnd::AddColorLine(const CString &line, CHTRichEditCtrl &wnd, COLORREF crForeground)
{
	DWORD dwMask = 0;
	int index = 0;
	int linkfoundat = 0; //This variable is to save needless costly string manipulation
	COLORREF foregroundColour = crForeground;
	COLORREF cr = foregroundColour; //set start foreground colour
	COLORREF backgroundColour = wnd.GetBackgroundColor(); //CLR_DEFAULT;
	COLORREF bgcr = backgroundColour; //set start background colour COMMENTED left for possible future use
	CString text;
	while (line.GetLength() > index) {
		TCHAR aChar = line[index];

		// find any hyperlinks and send them to AppendColoredText
		if (index == linkfoundat) { //only run the link finding code once in a line with no links
			for (unsigned iScheme = 0; iScheme < _countof(s_apszSchemes);) {
				const CString &strLeft = line.Right(line.GetLength() - index); //make a string of what we have left
				int foundat = strLeft.Find(s_apszSchemes[iScheme].pszScheme); //get position of link; -1 if not found
				if (foundat == 0) { //link starts at this character
					if (!text.IsEmpty()) {
						wnd.AppendColoredText(text, cr, bgcr, dwMask);
						text.Empty();
					}

					// search next space or EOL or control code
					int iLen = strLeft.FindOneOf(_T(" \t\r\n\x02\x03\x0F\x11\x16\x1d\x1F"));
					if (iLen == -1) {
						// truncate some special chars from end of URL (and only from end)
						iLen = strLeft.GetLength();
						while (iLen > 0 && !IsValidURLTerminationChar(strLeft[iLen - 1]))
							--iLen;
						wnd.AddLine(strLeft.Left(iLen), iLen, true);
						index += iLen;
						if (index >= line.GetLength())
							return;

						aChar = line[index]; // get a new char
						break;
					}
					// truncate some special chars from end of URL (and only from end)
					while (iLen > 0 && !IsValidURLTerminationChar(strLeft[iLen - 1]))
						--iLen;
					wnd.AddLine(strLeft.Left(iLen), iLen, true);
					index += iLen;
					if (index >= line.GetLength())
						return;

					iScheme = 0; // search from the new position
					foundat = -1; // do not record this processed location as a future target location
					linkfoundat = index; // reset previous finds as iScheme=0 we re-search
					aChar = line[index]; // get a new char

				} else {
					++iScheme; //only increment if not found at this position so if we find http at this position we check for further http occurrences
					//foundat is a valid position && (no valid position recorded || a farther position previously recorded)
					if (foundat > 0 && (linkfoundat == index || (index + foundat) < linkfoundat))
						linkfoundat = index + foundat; //set the next closest link to process
				}
			}
		}

		switch (aChar) {
		case 0x02: // Bold
			if (!text.IsEmpty()) {
				wnd.AppendColoredText(text, cr, bgcr, dwMask);
				text.Empty();
			}
			++index;
			dwMask ^= CFM_BOLD;
			break;
		case 0x03: // foreground & background colour
			if (!text.IsEmpty()) {
				wnd.AppendColoredText(text, cr, bgcr, dwMask);
				text.Empty();
			}
			++index;
			if (_istdigit(line[index])) {
				int iColour = (int)(line[index] - _T('0'));
				if (iColour == 1 && line[index + 1] >= _T('0') && line[index + 1] <= _T('5')) { //is there a second digit
					// make a two digit number
					++index;
					iColour = 10 + (int)(line[index] - _T('0'));
				} else if (iColour == 0 && _istdigit(line[index + 1])) { //if first digit is zero and there is a second digit e.g.: 3 in 03
					// make a two digit number
					++index;
					iColour = (int)(line[index] - _T('0'));
				}

				if (iColour >= 0 && iColour < _countof(s_aColors)) {
					// If the first colour is not valid, don't look for a second background colour!
					cr = s_aColors[iColour]; //if the number is a valid colour index, set new foreground colour
					++index;
					if (line[index] == _T(',') && _istdigit(line[index + 1])) { //is there a background colour
						++index;
						iColour = (int)(line[index] - _T('0'));
						if (iColour == 1 && line[index + 1] >= _T('0') && line[index + 1] <= _T('5')) { // is there a second digit
							// make a two digit number
							++index;
							iColour = 10 + (int)(line[index] - _T('0'));
						} else if (iColour == 0 && _istdigit(line[index + 1])) { // if first digit is zero and there is a second digit e.g.: 3 in 03
							// make a two digit number
							++index;
							iColour = (int)(line[index] - _T('0'));
						}
						++index;
						if (iColour >= 0 && iColour < _countof(s_aColors))
							bgcr = s_aColors[iColour]; //if the number is a valid colour index, set new foreground colour
					}
				}
			} else {
				// reset
				cr = foregroundColour;
				bgcr = backgroundColour;
			}
			break;
		case 0x0F: // attributes reset
			if (!text.IsEmpty()) {
				wnd.AppendColoredText(text, cr, bgcr, dwMask);
				text.Empty();
			}
			++index;
			dwMask = 0;
			cr = foregroundColour;
			bgcr = backgroundColour;
			break;
		case 0x16: // Reverse (as per Mirc) toggle
			// NOTE:This does not reset the bold/underline (dwMask) attributes, but does reset colours 'As per mIRC 6.16!!'
			if (!text.IsEmpty()) {
				wnd.AppendColoredText(text, cr, bgcr, dwMask);
				text.Empty();
			}
			++index;
			if (cr != backgroundColour || bgcr != foregroundColour) {
				// set inverse
				cr = backgroundColour;
				bgcr = foregroundColour;
			} else {
				// reset fg/bk colours
				cr = foregroundColour;
				bgcr = backgroundColour;
			}
			break;
		case 0x1d: // Italic toggle
			if (!text.IsEmpty()) {
				wnd.AppendColoredText(text, cr, bgcr, dwMask);
				text.Empty();
			}
			++index;
			dwMask ^= CFM_ITALIC;
			break;
		case 0x1f: // Underlined toggle
			if (!text.IsEmpty()) {
				wnd.AppendColoredText(text, cr, bgcr, dwMask);
				text.Empty();
			}
			++index;
			dwMask ^= CFM_UNDERLINE;
			break;
		default:
			text += aChar;
			++index;
		}
	}
	if (!text.IsEmpty())
		wnd.AppendColoredText(text, cr, bgcr, dwMask);
}

void CIrcWnd::SetConnectStatus(bool bFlag)
{
	m_bConnected = bFlag;
	SetDlgItemText(IDC_BN_IRCCONNECT, GetResString(bFlag ? IDS_IRC_DISCONNECT : IDS_IRC_CONNECT));
	if (bFlag)
		AddStatus(GetResString(IDS_CONNECTED));
	else {
		m_bLoggedIn = false;
		//detach all channels
		for (POSITION pos = m_wndChanSel.m_lstChannels.GetHeadPosition(); pos != NULL;) {
			Channel *pChannel = m_wndChanSel.m_lstChannels.GetNext(pos);
			if (pChannel->m_eType > Channel::ctChannelList)
				m_wndChanSel.DetachChannel(pChannel);
			if (pChannel->m_eType != Channel::ctChannelList)
				AddInfoMessageC(pChannel, EVENT_MSG_COLOR, _T("* ") + GetResString(IDS_DISCONNECTED));
		}
	}
}

void CIrcWnd::NoticeMessage(const CString &sSource, const CString &sTarget, const CString &sMessage)
{
	if (sTarget.CompareNoCase(thePrefs.GetIRCNick()) == 0) {
		AddInfoMessageF(sTarget, _T("-%s- %s"), (LPCTSTR)sSource,/* sTarget,*/ (LPCTSTR)sMessage);
		return;
	}

	if (m_wndChanSel.FindChannelByName(sTarget) != NULL)
		AddInfoMessageF(sTarget, _T("-%s:%s- %s"), (LPCTSTR)sSource, (LPCTSTR)sTarget, (LPCTSTR)sMessage);
	else {
		bool bFlag = false;
		for (POSITION pos = m_wndChanSel.m_lstChannels.GetHeadPosition(); pos != NULL;) {
			const Channel *pChannel = m_wndChanSel.m_lstChannels.GetNext(pos);
			if (pChannel) {
				const Nick *pNick = m_wndNicks.FindNickByName(pChannel->m_sName, sSource);
				if (pNick) { //is it correct to send notice to every channel?
					AddInfoMessageF(pChannel->m_sName, _T("-%s:%s- %s"), (LPCTSTR)sSource, (LPCTSTR)sTarget, (LPCTSTR)sMessage);
					bFlag = true;
				}
			}
		}
		if (!bFlag) {
			CString cs;
			cs.Format(_T("%s-%s- %s\r\n"), (LPCTSTR)make_time_stamp(), (LPCTSTR)sSource, (LPCTSTR)sMessage);
			Channel *pStatusChannel = m_wndChanSel.m_lstChannels.GetHead();
			if (pStatusChannel)
				AddColorLine(cs, pStatusChannel->m_wndLog, INFO_MSG_COLOR);
		}
	}
}

CString CIrcWnd::StripMessageOfColorCodes(const CString &sTemp)
{
	if (!sTemp.IsEmpty()) {
		int iTest = sTemp.Find(_T('\003'));
		if (iTest >= 0) {
			int iTestLength = sTemp.GetLength() - iTest;
			if (iTestLength < 2)
				return sTemp;
			CString sTemp1 = sTemp.Left(iTest);
			CString sTemp2 = sTemp.Mid(iTest + 2);
			if (iTestLength < 4)
				return sTemp1 + sTemp2;
			if (sTemp2[0] == _T(',') && sTemp2.GetLength() > 2) {
				sTemp2.Delete(0, 2);
				while (_istdigit(sTemp2[0]))
					sTemp2.Delete(0, 1);
			} else
				while (_istdigit(sTemp2[0])) {
					sTemp2.Delete(0, 1);
					if (sTemp2[0] == _T(',') && sTemp2.GetLength() > 2) {
						sTemp2.Delete(0, 2);
						while (_istdigit(sTemp2[0]))
							sTemp2.Delete(0, 1);
					}
				}
			return StripMessageOfColorCodes(sTemp1 + sTemp2);
		}
	}
	return sTemp;
}

CString CIrcWnd::StripMessageOfFontCodes(const CString &sTemp)
{
	CString sRet(StripMessageOfColorCodes(sTemp));
	sRet.Remove(_T('\x02')); // BOLD
	//sRet.Remove(_T('\x03')); // COLOUR
	sRet.Remove(_T('\x0f')); // RESET
	sRet.Remove(_T('\x16')); // REVERSE/INVERSE
	sRet.Remove(_T('\x1d')); // ITALIC
	sRet.Remove(_T('\x1f')); // UNDERLINE
	return sRet;
}

void CIrcWnd::SetTopic(const CString &sChannel, const CString &sTopic)
{
	Channel *pChannel = m_wndChanSel.FindChannelByName(sChannel);
	if (!pChannel)
		return;

	pChannel->m_wndTopic.SetWindowText(_T(""));
	pChannel->m_wndTopic.SetEventMask(pChannel->m_wndTopic.GetEventMask() | ENM_REQUESTRESIZE);
	AddColorLine(sTopic, pChannel->m_wndTopic);
	pChannel->m_wndTopic.SetEventMask(pChannel->m_wndTopic.GetEventMask() & ~ENM_REQUESTRESIZE);
	pChannel->m_wndTopic.ScrollToFirstLine();

	RECT rcTopic;
	pChannel->m_wndTopic.GetWindowRect(&rcTopic);
	ScreenToClient(&rcTopic);

	pChannel->m_wndSplitter.SetWindowPos(NULL, rcTopic.left, rcTopic.bottom, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

	RECT rcSplitter;
	pChannel->m_wndSplitter.GetWindowRect(&rcSplitter);
	ScreenToClient(&rcSplitter);

	CRect rcLog;
	pChannel->m_wndLog.GetWindowRect(rcLog);
	ScreenToClient(rcLog);
	rcLog.top = rcSplitter.bottom;
	pChannel->m_wndLog.SetWindowPos(NULL, rcLog.left, rcLog.top, rcLog.Width(), rcLog.Height(), SWP_NOZORDER);
}

void CIrcWnd::OnBnClickedIrcSend()
{
	CString sSend;
	m_wndInput.GetWindowText(sSend);
	m_wndInput.SetWindowText(_T(""));
	m_wndChanSel.ChatSend(sSend);
}

void CIrcWnd::SendString(const CString &sSend)
{
	if (m_bConnected)
		m_pIrcMain->SendString(sSend);
}

BOOL CIrcWnd::OnHelpInfo(HELPINFO*)
{
	theApp.ShowHelp(eMule_FAQ_GUI_IRC);
	return TRUE;
}

void CIrcWnd::OnChatTextChange()
{
	GetDlgItem(IDC_CHATSEND)->EnableWindow(m_wndInput.GetWindowTextLength() > 0);
}

void CIrcWnd::ParseChangeMode(const CString &sChannel, const CString &sChanger, CString sCommands, const CString &sParams)
{
	if (sChanger.IsEmpty())
		return;
	const CString sCommandsOrig(sCommands);
	const CString sParamsOrig(sParams);
	try {
		if (sCommands.GetLength() >= 2) {
			TCHAR cDir = _T('\0'); //no +/- modes specified
			int iParamIndex = 0;
			while (!sCommands.IsEmpty()) {
				if (sCommands[0] == _T('+') || sCommands[0] == _T('-')) {
					cDir = sCommands[0];
					sCommands.Delete(0, 1);
				}
				if (!sCommands.IsEmpty() && cDir) {
					const TCHAR cCommand(sCommands[0]);
					sCommands.Delete(0, 1);

					if (m_wndNicks.m_sUserModeSettings.Find(cCommand) >= 0) {
						//This is a user mode change and must have a param!
						const CString &sParam(sParams.Tokenize(_T(" "), iParamIndex));
						m_wndNicks.ChangeNickMode(sChannel, sParam, cDir, cCommand);
					}
					if (m_wndChanSel.m_sChannelModeSettingsTypeA.Find(cCommand) >= 0) {
						//We do not use these messages yet. But we can display them for the user to see
						//These modes always have a param and will add or remove a user from some type of list.
						const CString &sParam(sParams.Tokenize(_T(" "), iParamIndex));
						m_wndChanSel.ChangeChanMode(sChannel, sParam, cDir, cCommand);
					}
					if (m_wndChanSel.m_sChannelModeSettingsTypeB.Find(cCommand) >= 0) {
						//We do not use these messages yet. But we can display them for the user to see
						//These modes will always have a param.
						const CString &sParam(sParams.Tokenize(_T(" "), iParamIndex));
						m_wndChanSel.ChangeChanMode(sChannel, sParam, cDir, cCommand);
					}
					if (m_wndChanSel.m_sChannelModeSettingsTypeC.Find(cCommand) >= 0) {
						//We do not use these messages yet. But we can display them for the user to see
						//These modes will only have a param if your setting it!
						CString sParam;
						if (cDir == _T('+'))
							sParam = sParams.Tokenize(_T(" "), iParamIndex);
						m_wndChanSel.ChangeChanMode(sChannel, sParam, cDir, cCommand);
					}
					if (m_wndChanSel.m_sChannelModeSettingsTypeD.Find(cCommand) >= 0) {
						//We do not use these messages yet. But we can display them for the user to see
						//These modes will never have a param for it!
						m_wndChanSel.ChangeChanMode(sChannel, CString(), cDir, cCommand);
					}
				}
			}
			if (!thePrefs.GetIRCIgnoreMiscMessages())
				AddInfoMessageF(sChannel, GetResString(IDS_IRC_SETSMODE), (LPCTSTR)sChanger, (LPCTSTR)sCommandsOrig, (LPCTSTR)sParamsOrig);
		}
	} catch (...) {
		AddInfoMessage(sChannel, GetResString(IDS_IRC_NOTSUPPORTED));
		ASSERT(0);
	}
}

void CIrcWnd::AddCurrent(const CString &sLine, bool bShowActivity, UINT uStatusCode)
{
	if (uStatusCode >= 400 || m_wndChanSel.m_pCurrentChannel->m_eType < Channel::ctNormal || m_wndChanSel.m_pCurrentChannel->m_bDetached)
		AddStatus(sLine, bShowActivity, uStatusCode);
	else
		AddInfoMessage(m_wndChanSel.m_pCurrentChannel, sLine + _T("\r\n"));
}

LRESULT CIrcWnd::OnCloseTab(WPARAM wParam, LPARAM)
{
	OnBnClickedCloseChannel((int)wParam);
	return 1;
}

LRESULT CIrcWnd::OnQueryTab(WPARAM wParam, LPARAM)
{
	TCITEM item;
	item.mask = TCIF_PARAM;
	m_wndChanSel.GetItem((int)wParam, &item);
	const Channel *pPartChannel = reinterpret_cast<Channel*>(item.lParam);
	return !pPartChannel || pPartChannel->m_eType < Channel::ctChannelList;
}

bool CIrcWnd::GetLoggedIn() const
{
	return m_bLoggedIn;
}

void CIrcWnd::SetLoggedIn(bool bFlag)
{
	m_bLoggedIn = bFlag;
}

void CIrcWnd::SetSendFileString(const CString &sInFile)
{
	m_sSendString = sInFile;
}

const CString& CIrcWnd::GetSendFileString() const
{
	return m_sSendString;
}

bool CIrcWnd::IsConnected() const
{
	return m_bConnected;
}

void CIrcWnd::OnBnClickedColour()
{
	int iColor = 0;
	RECT rDraw;
	m_wndFormat.GetWindowRect(&rDraw);
	new CColourPopup(CPoint(rDraw.left + 1, rDraw.bottom - 92)	// Point to display popup
					, iColor									// Selected colour
					, this										// parent
					, GetResString(IDS_DEFAULT)					// "Default" text area
					, NULL										// Custom Text
					, (LPCOLORREF)s_aColors						// Pointer to a COLORREF array
					, _countof(s_aColors));						// Size of the array

	CWnd *pParent = GetParent();
	if (pParent)
		pParent->SendMessage(UM_CPN_DROPDOWN, (WPARAM)GetDlgCtrlID(), (LPARAM)iColor);
}

LRESULT CIrcWnd::OnSelEndOK(WPARAM wParam, LPARAM)
{
	// Treat any unknown colour as CLR_DEFAULT
	int iColour = _countof(s_aColors);
	while (--iColour >= 0 && (COLORREF)wParam != s_aColors[iColour]);

	CString sAddAttribute;
	int	iSelStart, iSelEnd;
	m_wndInput.GetSel(iSelStart, iSelEnd); //get selection area
	m_wndInput.GetWindowText(sAddAttribute); //get the whole line

	if (iColour >= 0) { //iColour is in valid range
		if (iSelEnd > iSelStart)
			sAddAttribute.Insert(iSelEnd, _T("\003") _T("01")); //if a selection add default black colour tag

		iColour += '0';
		//a number greater than 9
		if (iColour > '9') {
			sAddAttribute.Insert(iSelStart, (TCHAR)(iColour - 10)); //add second half of colour tag 1 for range 10 to 15
			sAddAttribute.Insert(iSelStart, _T('1')); //add first half of colour tag
		} else {
			sAddAttribute.Insert(iSelStart, (TCHAR)(iColour)); //add second half of colour tag 1 for range 0 to 9
			sAddAttribute.Insert(iSelStart, _T('0')); //add first half of colour tag
		}
		//if this is the start of the line, not a selection in the line and a colour has already just been set, allow background to be set
		bool bSelEnd3Char = (iSelEnd > 2 && sAddAttribute[iSelEnd - 3] == _T('\003'));
		bool bSelEnd6Char = (iSelEnd > 5 && sAddAttribute[iSelEnd - 6] == _T('\003'));

		if (iSelEnd == iSelStart && bSelEnd3Char && !bSelEnd6Char)
			sAddAttribute.Insert(iSelStart, _T(',')); //separator for background colour
		else
			sAddAttribute.Insert(iSelStart, _T('\003')); //add start tag
	} else {
		//Default button clicked or unknown colour requested - set black

		//if this is the start of the line, not a selection in the line
		// and a colour has already been set, then allow background to be set
		bool bSelEnd3Char = (iSelEnd > 2 && sAddAttribute[iSelEnd - 3] == _T('\003'));
		bool bSelEnd6Char = (iSelEnd > 5 && sAddAttribute[iSelEnd - 6] == _T('\003'));

		if (iSelEnd == iSelStart && bSelEnd3Char && !bSelEnd6Char) //Set DEFAULT white background
			sAddAttribute.Insert(iSelStart, _T(",00")); //separator for background colour and colour tag
		else //Set DEFAULT black foreground
			sAddAttribute.Insert(iSelStart, _T("\003") _T("01")); //add start tag and colour tag
	}
	if (!sAddAttribute.IsEmpty()) {
		m_wndInput.SetWindowText(sAddAttribute); //write new line to edit control
		m_wndInput.SetSel(iSelStart + 3, iSelEnd + 3); //update selection info
	}
	CWnd *pParent = GetParent();
	if (pParent) {
		pParent->SendMessage(UM_CPN_CLOSEUP, wParam, (LPARAM)GetDlgCtrlID());
		pParent->SendMessage(UM_CPN_SELENDOK, wParam, (LPARAM)GetDlgCtrlID());
	}
	m_wndInput.SetFocus(); //move focus to edit control

	return 1;
}

LRESULT CIrcWnd::OnSelEndCancel(WPARAM wParam, LPARAM)
{
	CWnd *pParent = GetParent();
	if (pParent) {
		pParent->SendMessage(UM_CPN_CLOSEUP, wParam, (LPARAM)GetDlgCtrlID());
		pParent->SendMessage(UM_CPN_SELENDCANCEL, wParam, (LPARAM)GetDlgCtrlID());
	}
	return 1;
}

void CIrcWnd::OnBnClickedSmiley()
{
	if (m_pwndSmileySel) {
		m_pwndSmileySel->DestroyWindow();
		delete m_pwndSmileySel;
		m_pwndSmileySel = NULL;
	}
	m_pwndSmileySel = new CSmileySelector;

	RECT rcBtn;
	m_wndFormat.GetWindowRect(&rcBtn);
	rcBtn.top -= 2;

	if (!m_pwndSmileySel->Create(this, &rcBtn, &m_wndInput)) {
		delete m_pwndSmileySel;
		m_pwndSmileySel = NULL;
	}
}

void CIrcWnd::SetFontStyle(TCHAR c)
{
	int	iSelStart;
	int	iSelEnd;
	m_wndInput.GetSel(iSelStart, iSelEnd); //get selection area

	CString sAddAttribute;
	m_wndInput.GetWindowText(sAddAttribute); //get the whole line
	if (iSelEnd > iSelStart)
		sAddAttribute.Insert(iSelEnd, c); //if a selection, add end tag
	sAddAttribute.Insert(iSelStart, c); //add start tag
	++iSelStart; //increment start position
	++iSelEnd; //increment end position
	m_wndInput.SetWindowText(sAddAttribute); //write new line to edit control
	m_wndInput.SetSel(iSelStart, iSelEnd); //update selection info
}

void CIrcWnd::OnBnClickedBold()
{
	SetFontStyle(_T('\x02'));
}

void CIrcWnd::OnBnClickedItalic()
{
	SetFontStyle(_T('\x1d'));
}

void CIrcWnd::OnBnClickedUnderline()
{
	SetFontStyle(_T('\x1f'));
}

void CIrcWnd::OnBnClickedReset()
{
	int iSelStart;
	int	iSelEnd;
	m_wndInput.GetSel(iSelStart, iSelEnd); //get selection area
	if (!iSelStart)
		return; //reset is not the first character

	CString sAddAttribute;
	m_wndInput.GetWindowText(sAddAttribute); //get the whole line
	//Note the 'else' below! this tag resets all attribute so only one tag needed at current position or end of selection
	if (iSelEnd > iSelStart)
		sAddAttribute.Insert(iSelEnd, _T('\x0f')); //if a selection add end tag
	else
		sAddAttribute.Insert(iSelStart, _T('\x0f')); //add start tag
	++iSelStart; //increment start position
	++iSelEnd; //increment end position
	m_wndInput.SetWindowText(sAddAttribute); //write new line to edit control
	m_wndInput.SetSel(iSelStart, iSelEnd); //update selection info
}

void CIrcWnd::OnEnRequestResizeTitle(LPNMHDR pNMHDR, LRESULT *pResult)
{
	REQRESIZE *pReqResize = reinterpret_cast<REQRESIZE*>(pNMHDR);
	ASSERT(pReqResize->nmhdr.hwndFrom);

	CRect rcTopic;
	::GetWindowRect(pReqResize->nmhdr.hwndFrom, rcTopic);
	ScreenToClient(rcTopic);

	CRect rcResizeAdjusted(pReqResize->rc);
	::AdjustWindowRectEx(&rcResizeAdjusted
		, static_cast<DWORD>(::GetWindowLongPtr(pReqResize->nmhdr.hwndFrom, GWL_STYLE)), FALSE
		, static_cast<DWORD>(::GetWindowLongPtr(pReqResize->nmhdr.hwndFrom, GWL_EXSTYLE)));
	rcTopic.bottom = rcTopic.top + rcResizeAdjusted.Height() + 1/*!?!*/;

	// Don't allow too large title windows
	if (rcTopic.Height() <= IRC_TITLE_WND_MAX_HEIGHT)
		::SetWindowPos(pReqResize->nmhdr.hwndFrom, NULL, 0, 0, rcTopic.Width(), rcTopic.Height(), SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);

	*pResult = 0;
}

HBRUSH CIrcWnd::OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor)
{
	HBRUSH hbr = theApp.emuledlg->GetCtlColor(pDC, pWnd, nCtlColor);
	return hbr ? hbr : __super::OnCtlColor(pDC, pWnd, nCtlColor);
}