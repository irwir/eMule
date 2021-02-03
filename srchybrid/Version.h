#ifndef __VERSION_H__
#define __VERSION_H__

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef _T
#define _T(x)	x
#endif

#define _chSTR(x)	_T(#x)
#define chSTR(x)	_chSTR(x)

// *) Specify the version of emule only here with the following defines.
// *) When changing any of those version nr. defines you also have to rebuild the language DLLs.
//
// General format:
//	<major>.<minor>.<update>.<build>
//
// Fields:
//	<major>		major number (e.g. 0)
//	<minor>		minor number (e.g. 30)
//	<update>	update number (e.g. 0='a'  1='b'  2='c'  3='d'  4='e'  5='f' ...)
//	<build>		build number; currently not used
//
// Currently used:
//  <major>.<minor>.<update> is used for the displayed version (GUI) and the version check number
//	<major>.<minor>			 is used for the protocol(!) version
//
#define VERSION_MJR	0
#define VERSION_MIN	60
#define VERSION_UPDATE	0
#define VERSION_BUILD	8
#ifdef _M_X64
#define VERSION_X64 _T(" x64")
#else
#define VERSION_X64 _T("")
#endif // _M_X64

// NOTE: Do not forget to update file: res/eMule.manifest

// NOTE: This version string is also used by the language DLLs!
#define	SZ_VERSION_NAME		chSTR(VERSION_MJR) _T(".") chSTR(VERSION_MIN) _T(".") chSTR(VERSION_UPDATE)

#endif /* !__VERSION_H__ */
