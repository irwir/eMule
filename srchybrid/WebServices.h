#pragma once

#define	WEBSVC_GEN_URLS		0x0001
#define	WEBSVC_FILE_URLS	0x0002

class CTitleMenu;

class CWebServices
{
public:
	CWebServices();

	CString GetDefaultServicesFile() const;
	INT_PTR ReadAllServices();
	void RemoveAllServices();

	int GetFileMenuEntries(CTitleMenu *pMenu)		{ return GetAllMenuEntries(pMenu, WEBSVC_FILE_URLS); }
	int GetGeneralMenuEntries(CTitleMenu *pMenu)	{ return GetAllMenuEntries(pMenu, WEBSVC_GEN_URLS); }
	int GetAllMenuEntries(CTitleMenu *pMenu, DWORD dwFlags = WEBSVC_GEN_URLS | WEBSVC_FILE_URLS);
	bool RunURL(const CAbstractFile *file, UINT uMenuID);
	void Edit();

protected:
	struct SEd2kLinkService
	{
		CString strMenuLabel;
		CString strUrl;
		UINT uMenuID;
		BOOL bFileMacros;
	};
	CArray<SEd2kLinkService> m_aServices;
	time_t m_tDefServicesFileLastModified;
};

extern CWebServices theWebServices;