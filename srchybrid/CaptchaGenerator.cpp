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
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "StdAfx.h"
#include "CaptchaGenerator.h"
#include "CxImage/xImage.h"
#include "OtherFunctions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define LETTERSIZE  32
#define CROWDEDSIZE 18

// fairly simple captcha generator, might be improved if spammers think it's really worth it to solve captchas in eMule

static TCHAR const schCaptchaContent[] = _T("ABCDEFGHIJKLMNPQRSTUVWXYZ123456789");

CCaptchaGenerator::CCaptchaGenerator(uint32 nLetterCount)
	: m_pimgCaptcha()
{
	ReGenerateCaptcha(nLetterCount);
}

CCaptchaGenerator::~CCaptchaGenerator()
{
	Clear();
}

void CCaptchaGenerator::ReGenerateCaptcha(uint32 nLetterCount)
{
	Clear();
	//CUpDownClient::ProcessCaptchaRequest verifies that height is between 11 and 49, width between 11 and 149
	CxImage *pimgResult = new CxImage(nLetterCount > 1 ? ((LETTERSIZE) + nLetterCount * (CROWDEDSIZE)) : (LETTERSIZE), 48, 1, CXIMAGE_FORMAT_BMP);
	pimgResult->SetPaletteColor(0, 255, 255, 255);
	pimgResult->SetPaletteColor(1, 0, 0, 0, 0);
	pimgResult->Clear();
	CxImage imgBlank(LETTERSIZE, LETTERSIZE, 1, CXIMAGE_FORMAT_BMP);
	imgBlank.SetPaletteColor(0, 255, 255, 255);
	imgBlank.SetPaletteColor(1, 0, 0, 0, 0);
	imgBlank.Clear();
	TCHAR strLetter[2] = {};
	for (uint32 i = 0; i < nLetterCount; ++i) {
		CxImage imgLetter(imgBlank);

		strLetter[0] = schCaptchaContent[GetRandomUInt16() % (_countof(schCaptchaContent) - 1)];
		m_strCaptchaText += strLetter[0];

		uint16 nRandomSize = GetRandomUInt16() % 10;
		uint16 nRandomOffset = 3 + GetRandomUInt16() % 11;
		imgLetter.DrawString(NULL, nRandomOffset, 32, strLetter, imgLetter.RGBtoRGBQUAD(RGB(0, 0, 0)), _T("Arial"), 40 - nRandomSize, 1000);
		//imgLetter.DrawTextA(NULL, nRandomOffset, 32, strLetter, imgLetter.RGBtoRGBQUAD(RGB(0, 0, 0)), "Arial", 40 - nRandomSize, 1000);
		float fRotate = 35.0f - (GetRandomUInt16() % 71);
		imgLetter.Rotate2(fRotate, NULL, CxImage::IM_BILINEAR, CxImage::OM_BACKGROUND, 0, false, false);
		uint32 nOffset = i * CROWDEDSIZE;
		ASSERT(pimgResult->GetHeight() >= imgLetter.GetHeight() && pimgResult->GetWidth() >= nOffset + imgLetter.GetWidth());
		for (uint32 j = 0; j < imgLetter.GetHeight(); ++j)
			for (uint32 k = 0; k < imgLetter.GetWidth(); ++k)
				if (pimgResult->GetPixelIndex(nOffset + k, j) != 1)
					pimgResult->SetPixelIndex(nOffset + k, j, imgLetter.GetPixelIndex(k, j));
	}
	pimgResult->Jitter(1);
	//pimgResult->Save(_T("D:\\CaptchaTest.bmp"), CXIMAGE_FORMAT_BMP);
	m_pimgCaptcha = pimgResult;
}

void CCaptchaGenerator::Clear()
{
	delete m_pimgCaptcha;
	m_pimgCaptcha = NULL;
	m_strCaptchaText.Empty();
}

bool CCaptchaGenerator::WriteCaptchaImage(CFileDataIO &file)
{
	if (m_pimgCaptcha == NULL)
		return false;
	BYTE *pbyBuffer = NULL;
	int32_t ulSize = 0;
	if (m_pimgCaptcha->Encode(pbyBuffer, ulSize, CXIMAGE_FORMAT_BMP)) {
		file.Write(pbyBuffer, ulSize);
		ASSERT(ulSize > 100 && ulSize < 1000);
		free(pbyBuffer);
		return true;
	}
	return false;
}