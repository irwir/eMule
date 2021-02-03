// HighColorTab.hpp
//
// Author:  Yves Tkaczyk (yves@tkaczyk.net)
//
// This software is released into the public domain.  You are free to use it
// in any way you like BUT LEAVE THIS HEADER INTACT.
//
// This software is provided "as is" with no expressed or implied warranty.
// I accept no liability for any damage or loss of business that this software
// may cause.
//
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <memory>

#if _MSC_VER>=1400
#pragma warning(push)
#pragma warning(disable:4350) // behavior change: 'std::auto_ptr<_Ty>::auto_ptr(std::auto_ptr_ref<_Ty>) noexcept' called instead of 'std::auto_ptr<_Ty>::auto_ptr(std::auto_ptr<_Ty> &) noexcept'
#endif

namespace HighColorTab
{
  /*! \brief Policy class for creating image list.

	Policy for creating a high color (32 bits) image list. The policy
	ensure that there is a Win32 image list associated with the CImageList.
	If this is not the case, a NULL pointer shall be returned.

	Returned image list is wrapped in an std::auto_ptr.

	\sa UpdateImageListFull  */
	struct CHighColorListCreator
	{
	  /*! Create the image list.
		  \retval std::auto_ptr<CImageList> Not null if success. */
		static std::auto_ptr<CImageList> CreateImageList()
		{
			std::auto_ptr<CImageList> apILNew(new CImageList());
			if (FALSE == apILNew->Create(16, 16, theApp.m_iDfltImageListColorFlags | ILC_MASK, 0, 1)) {
			  // ASSERT: The image list (Win32) creation failed.
				ASSERT(0);
				return std::auto_ptr<CImageList>();
			}
			return apILNew;
		}
	};



	/*! \brief Change the image list of the provided control (property sheet interface)

	  This method provides full customization via policy over image list creation. The policy
	  must have a method with the signature:
	  <code>static std::auto_ptr<CImageList> CreateImageList()</code>

	  \author Yves Tkaczyk (yves@tkaczyk.net)
	  \date 02/2004 */
	template<typename TSheet,
		typename TListCreator>
		bool UpdateImageListFull(TSheet &rSheet)
	{
		// Get the tab control...
		CTabCtrl *pTab = rSheet.GetTabControl();
		if (!::IsWindow(pTab->GetSafeHwnd())) {
		// ASSERT: Tab control could not be retrieved or it is not a valid window.
			ASSERT(0);
			return false;
		}

	  // Create the replacement image list via policy.
		std::auto_ptr<CImageList> apILNew(TListCreator::CreateImageList());

		// Reload the icons from the property pages.
		int nTotalPageCount = rSheet.GetPageCount();
		for (int nCurrentPage = 0; nCurrentPage < nTotalPageCount; ++nCurrentPage) {
		  // Get the page.
			CPropertyPage *pPage = rSheet.GetPage(nCurrentPage);
			ASSERT(pPage);
			// Set the icon in the image list from the page properties.
			if (pPage && (pPage->m_psp.dwFlags & PSP_USEHICON))
				apILNew->Add(pPage->m_psp.hIcon);

			if (pPage && (pPage->m_psp.dwFlags & PSP_USEICONID)) {
				HICON hIcon = theApp.LoadIcon(pPage->m_psp.pszIcon, 16, 16);
				if (hIcon) {
					apILNew->Add(hIcon);
					::DestroyIcon(hIcon);
				}
			}
		}

		// Replace the image list from the tab control.
		CImageList *pilOld = pTab->SetImageList(CImageList::FromHandle(apILNew->Detach()));
		// Clean the old image list if there was one.
		if (pilOld)
			pilOld->DeleteImageList();

		return true;
	};

	/*! \brief Change the image list of the provided control (property sheet)

	  This method uses 32 bits image list creation default policy. */
	template<typename TSheet>
	bool UpdateImageList(TSheet &rSheet)
	{
		return UpdateImageListFull<TSheet, HighColorTab::CHighColorListCreator>(rSheet);
	};
};

#if _MSC_VER>=1400
#pragma warning(pop)
#endif