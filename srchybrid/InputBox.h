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

class InputBox : public CDialog
{
	DECLARE_DYNAMIC(InputBox)

	enum
	{
		IDD = IDD_INPUTBOX
	};

public:
	explicit InputBox(CWnd *pParent = NULL);   // standard constructor
	virtual	~InputBox();

	void	SetLabels(const CString &title, const CString &label, const CString &defaultStr);
	const CString& GetInput() const							{ return m_return; }
	bool	WasCancelled() const							{ return m_cancel; }
	void	SetEditFilenameMode(bool isfilenamemode = true)	{ m_bFilenameMode = isfilenamemode; }

protected:
	CString m_label;
	CString m_title;
	CString m_default;
	CString m_return;
	bool	m_cancel;
	bool	m_bFilenameMode;
	HICON	m_icMain;

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	afx_msg void OnOK();
	afx_msg void OnCleanFilename();
	DECLARE_MESSAGE_MAP()
};