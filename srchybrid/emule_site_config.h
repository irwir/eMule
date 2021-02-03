#pragma once

// This file provides a way for local compiler site configurations (e.g. installed SDKs).

// By default, assume that we have all the SDKs which we need and enable all optional features.
#define	HAVE_SAPI_H
#define HAVE_QEDIT_H
#define HAVE_WMSDK_H
#define HAVE_WIN7_SDK_H
#define HAVE_VISTA_SDK

#ifdef XP_BUILD
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#define NTDDI_VERSION NTDDI_WINXP
#endif

//////////////////////////////////////////////////////////////////////////////
// Visual Studio 2003
//////////////////////////////////////////////////////////////////////////////
#if _MSC_VER==1310

// 'sapi.h' is not shipped with VS2003.
// Uncomment the following line if you get compile errors due to missing 'sapi.h'
//#undef HAVE_SAPI_H

// 'qedit.h' is shipped with VS2003.
//#undef HAVE_QEDIT_H

// 'wmsdk.h' is not shipped with VS2003.
//
// 'wmsdk.h' is shipped with Windows Media Format SDK 9 (http://download.microsoft.com/download/3/0/4/30451651-9e47-4313-89a3-5bb1db003c26/WMFormatSDK.exe)
//		Compiles fine with VS2003 (has a few missing declarations which where
//		resolved manually). However, this is the *PREFERRED* Windows Media Format SDK
//		for building eMule with VS2003.
//
// 'wmsdk.h' is shipped with Windows Media Format SDK 9.5 (http://download.microsoft.com/download/9/f/d/9fdfb288-b4bf-45fa-959c-1cc6d909aa92/wmformat95sdk.exe)
//		Compiles fine with VS2003 but the SDK itself can *not* get installed
//		under Windows Vista. It installs only under Windows XP.
//
// 'wmsdk.h' is shipped with Windows Media Format SDK 11 (http://download.microsoft.com/download/a/c/3/ac367925-39e7-4451-a175-a224f94fbdce/wmformat11sdk.exe)
//		Does not compile with VS2003 because of a clash in 'shlobj.h' which is
//		not that easy to resolve, if at all. It might compile fine when used in
//		combination with the latest non-Vista Platform SDK (Windows Server 2003 R2).
//
// Uncomment the following line if you get compile errors due to missing 'wmsdk.h'
//#undef HAVE_WMSDK_H

#endif


//////////////////////////////////////////////////////////////////////////////
// Visual Studio 2005
//////////////////////////////////////////////////////////////////////////////
#if _MSC_VER==1400

// NOTE: eMule can not get compiled with VS2005 out of the box because the SDK
// which is shipped with VS2005 does not contain the ‘upnp.h’ header file - and
// this feature is not yet optional for compiling eMule. Thus you need to install
// an additional more recent SDK when compiling with VS2005.
//
// It is supposed that eMule can get compiled with VS2005 and the latest
// "Windows Server 2003 (Windows XP) SDK" - but it was not yet verified.
//
// It is known that eMule can get compiled with VS2005 and the "Vista SDK 6.0/6.1".
// To compile eMule with the "Vista SDK 6.0/6.1", define 'HAVE_VISTA_SDK'.

#define HAVE_VISTA_SDK		// Preferred SDK for VS2005
#define HAVE_WMF_SDK		// WMF SDK is part of the Vista SDK
#define HAVE_DIRECTX_SDK	// DirectX 9(!) SDK

// 'sapi.h' is not shipped with VS2005.
// 'sapi.h' is shipped with Vista SDK 6.1
// You need to install the Speech SDK to enable this feature.
#ifndef HAVE_VISTA_SDK
#undef HAVE_SAPI_H
#endif//HAVE_VISTA_SDK

// 'qedit.h' is not shipped with VS2005.
// 'qedit.h' is shipped with Vista SDK 6.1, but it needs an additional file ('ddraw.h') which
// is only shipped with the DirectX 9 SDK.
// You need to install the DirectX 9 SDK to enable this feature.
#if !defined(HAVE_VISTA_SDK) || !defined(HAVE_DIRECTX_SDK)
//#undef HAVE_QEDIT_H
#endif//!defined(HAVE_VISTA_SDK) || !defined(HAVE_DIRECTX_SDK)

// 'wmsdk.h' is not shipped with VS2005.
// 'wmsdk.h' is shipped with Vista SDK 6.1 and with Windows Media Format SDK 9+
#if !defined(HAVE_VISTA_SDK) && !defined(HAVE_WMF_SDK)
#undef HAVE_WMSDK_H
#endif//!defined(HAVE_VISTA_SDK) && !defined(HAVE_WMF_SDK)

#endif


//////////////////////////////////////////////////////////////////////////////
// Visual Studio 2008
//////////////////////////////////////////////////////////////////////////////
#if _MSC_VER==1500

#define HAVE_VISTA_SDK		// VS2008 is already shipped with a Vista SDK
#define HAVE_WMF_SDK		// WMF SDK is part of the Vista SDK
#define HAVE_DIRECTX_SDK	// DirectX 9(!) SDK

// 'sapi.h' is shipped with VS2008 as part of the Vista SDK
#ifndef HAVE_VISTA_SDK
#undef HAVE_SAPI_H
#endif//HAVE_VISTA_SDK

// 'qedit.h' file is shipped with VS2008 as part of the Vista SDK, but it needs
// an additional file ('ddraw.h') which is only shipped with the DirectX 9 SDK.
// You need to install the DirectX 9 SDK to enable this feature.
#if !defined(HAVE_VISTA_SDK) || !defined(HAVE_DIRECTX_SDK)
#undef HAVE_QEDIT_H
#endif//!defined(HAVE_VISTA_SDK) || !defined(HAVE_DIRECTX_SDK)

// 'wmsdk.h' is shipped with VS2008 as part of the Vista SDK
#if !defined(HAVE_VISTA_SDK) && !defined(HAVE_WMF_SDK)
#undef HAVE_WMSDK_H
#endif//!defined(HAVE_VISTA_SDK) && !defined(HAVE_WMF_SDK)

#endif

//////////////////////////////////////////////////////////////////////////////
// Visual Studio 2012, 2013, 2015, 2017, 2019
//////////////////////////////////////////////////////////////////////////////
//  VS2012==1700, VS2013==1800, VS2015==1900, VS2017==1910-1916, VS2019==????
#if _MSC_VER>=1700

#define HAVE_VISTA_SDK		// shipped with Windows SDK
#define HAVE_WMF_SDK		// WMF SDK is part of the Windows SDK
#define HAVE_DIRECTX_SDK	// DirectX 9(!) SDK

// 'sapi.h' is shipped as part of the Windows SDK
#ifndef HAVE_VISTA_SDK
#undef HAVE_SAPI_H
#endif//HAVE_VISTA_SDK

// 'qedit.h' file is not shipped after VS200
#if !defined(HAVE_VISTA_SDK) || !defined(HAVE_DIRECTX_SDK)
#undef HAVE_QEDIT_H
#endif//!defined(HAVE_VISTA_SDK) || !defined(HAVE_DIRECTX_SDK)

// 'wmsdk.h' is shipped as part of the Windows SDK
#if !defined(HAVE_VISTA_SDK) && !defined(HAVE_WMF_SDK)
#undef HAVE_WMSDK_H
#endif//!defined(HAVE_VISTA_SDK) && !defined(HAVE_WMF_SDK)

#include <sdkddkver.h>

#endif