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

class CPPgMessages : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgMessages)

	enum
	{
		IDD = IDD_PPG_MESSAGES
	};

public:
	CPPgMessages();
	virtual	~CPPgMessages() = default;

	void Localize();

protected:
	void LoadSettings();

	virtual void DoDataExchange(CDataExchange *pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnSettingsChange()		{ SetModified(); }
	afx_msg void OnSpamFilterChange();
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnDestroy();
};