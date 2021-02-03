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
#include "Preferences.h"

class CPPgNotify : public CPropertyPage
{
	DECLARE_DYNAMIC(CPPgNotify)

	enum
	{
		IDD = IDD_PPG_NOTIFY
	};

public:
	CPPgNotify();
	virtual	~CPPgNotify() = default;

	void Localize();

protected:
	EmailSettings m_mail;
	HICON m_icoBrowse;

	void UpdateControls();
	void ApplyNotifierSoundType();

	virtual void DoDataExchange(CDataExchange *pDX);
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnHelp();
	afx_msg BOOL OnHelpInfo(HELPINFO*);
	afx_msg void OnSettingsChange()					{ SetModified(); }
	afx_msg void OnBnClickedNoSound();
	afx_msg void OnBnClickedUseSound();
	afx_msg void OnBnClickedUseSpeech();
	afx_msg void OnBnClickedOnChat();
	afx_msg void OnBnClickedBrowseAudioFile();
	afx_msg void OnBnClickedTestNotification();
	afx_msg void OnBnClickedCbEnablenotifications();
	afx_msg void OnBnClickedSMTPserver();
	afx_msg void OnDestroy();
};