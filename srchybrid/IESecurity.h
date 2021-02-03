#pragma once

class CMuleBrowserControlSite : public CBrowserControlSite
{
public:
	CMuleBrowserControlSite(COleControlContainer *pCtrlCont, CDHtmlDialog *pHandler);

protected:
	URLZONE m_eUrlZone;
	void InitInternetSecurityZone();

	DECLARE_INTERFACE_MAP();

	BEGIN_INTERFACE_PART(InternetSecurityManager, IInternetSecurityManager)
		STDMETHOD(SetSecuritySite)(IInternetSecurityMgrSite*);
		STDMETHOD(GetSecuritySite)(IInternetSecurityMgrSite**);
		STDMETHOD(MapUrlToZone)(LPCWSTR, DWORD*, DWORD);
		STDMETHOD(GetSecurityId)(LPCWSTR,BYTE*, DWORD*, DWORD_PTR);
		STDMETHOD(ProcessUrlAction)(LPCWSTR pwszUrl, DWORD dwAction, BYTE __RPC_FAR *pPolicy, DWORD cbPolicy, BYTE __RPC_FAR *pContext, DWORD cbContext, DWORD dwFlags, DWORD dwReserved = 0);
		STDMETHOD(QueryCustomPolicy)(LPCWSTR, REFGUID, BYTE**, DWORD*, BYTE*, DWORD, DWORD);
		STDMETHOD(SetZoneMapping)(DWORD, LPCWSTR, DWORD);
		STDMETHOD(GetZoneMappings)(DWORD, IEnumString**, DWORD);
	END_INTERFACE_PART(InternetSecurityManager)

	BEGIN_INTERFACE_PART(ServiceProvider, IServiceProvider)
		STDMETHOD(QueryService)(REFGUID, REFIID, void**);
	END_INTERFACE_PART(ServiceProvider)
};