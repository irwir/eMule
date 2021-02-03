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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#include "stdafx.h"

#ifdef HAVE_SAPI_H
// NOTE: If you get a compile error due to missing 'sapi.h', look at "emule_site_config.h" for further information.
#include <sapi.h>
#endif//HAVE_SAPI_H
#include "emule.h"
#include "emuleDlg.h"
#include "TextToSpeech.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


#ifdef HAVE_SAPI_H
///////////////////////////////////////////////////////////////////////////////
// CTextToSpeech

class CTextToSpeech
{
public:
	~CTextToSpeech();
	CTextToSpeech();

	bool CreateTTS();
	void ReleaseTTS();
	bool IsActive() const { return m_pISpVoice != NULL; }
	bool Speak(LPCTSTR pwsz);

protected:
	long m_lTTSLangID;
	CComPtr<ISpVoice> m_pISpVoice;
};

CTextToSpeech::CTextToSpeech()
{
	m_lTTSLangID = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
}

CTextToSpeech::~CTextToSpeech()
{
	ASSERT(m_pISpVoice == NULL);
	ReleaseTTS();
}

bool CTextToSpeech::CreateTTS()
{
	if (m_pISpVoice == NULL && SUCCEEDED(m_pISpVoice.CoCreateInstance(CLSID_SpVoice))) {
		m_lTTSLangID = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
		return true;
	}
	return false;
}

void CTextToSpeech::ReleaseTTS()
{
	m_lTTSLangID = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
	m_pISpVoice.Release();
}

bool CTextToSpeech::Speak(LPCTSTR pwsz)
{
	return IsActive() && SUCCEEDED(m_pISpVoice->Speak(pwsz, SPF_ASYNC | SPF_IS_NOT_XML, NULL));
}

///////////////////////////////////////////////////////////////////////////////
CTextToSpeech theTextToSpeech;
static bool s_bTTSDisabled = false;
static bool s_bInitialized = false;
#else//HAVE_SAPI_H
#pragma message("WARNING: Missing 'sapi.h' header file - some features will get disabled. See the file 'emule_site_config.h' for more information.")
#endif//HAVE_SAPI_H

bool Speak(LPCTSTR pszSay)
{
#ifdef HAVE_SAPI_H
	if (theApp.IsClosing())
		return false;
	if (s_bTTSDisabled)
		return false;

	if (!s_bInitialized) {
		s_bInitialized = true;
		if (!theTextToSpeech.CreateTTS())
			return false;
	}
	return theTextToSpeech.Speak(pszSay);
#else//HAVE_SAPI_H
	UNREFERENCED_PARAMETER(pszSay);
	return false;
#endif//HAVE_SAPI_H
}

void ReleaseTTS()
{
#ifdef HAVE_SAPI_H
	theTextToSpeech.ReleaseTTS();
	s_bInitialized = false;
#endif//HAVE_SAPI_H
}

void CloseTTS()
{
#ifdef HAVE_SAPI_H
	ReleaseTTS();
	s_bTTSDisabled = true;
#endif//HAVE_SAPI_H
}

bool IsSpeechEngineAvailable()
{
#ifdef HAVE_SAPI_H
	if (s_bTTSDisabled)
		return false;

	static bool _bIsAvailable = false;
	static bool _bCheckedAvailable = false;
	if (!_bCheckedAvailable) {
		_bCheckedAvailable = true;
		if (theTextToSpeech.IsActive())
			_bIsAvailable = true;
		else {
			_bIsAvailable = theTextToSpeech.CreateTTS();
			theTextToSpeech.ReleaseTTS();
		}
	}
	return _bIsAvailable;
#else//HAVE_SAPI_H
	return false;
#endif//HAVE_SAPI_H
}