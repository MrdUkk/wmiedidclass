/*
	Windows COM-server EDID monitor information provider by dUkk
	
	mainly developed for organization internal MS SCCM inventorization processes
	
	(c) 2013-2014
*/
#include <objbase.h>
#include <initguid.h>
#include <strsafe.h>

#include "sample.h"

HMODULE ghModule;

DEFINE_GUID(CLSID_instprovider, 0x8d177bab, 0x6f31, 0x45ad, 0xa3, 0xd5, 0x76, 0x54, 0x27, 0x9a, 0x54, 0xbb);
//{8D177BAB-6F31-45AD-A3D5-7654279A54BB}

//Count number of objects and number of locks.

long       g_cObj=0;
long       g_cLock=0;

//***************************************************************************
//
// LibMain32
//
// Purpose: Entry point for DLL.
//
// Return: TRUE if OK.
//
//***************************************************************************


BOOL WINAPI LibMain32(HINSTANCE hInstance, ULONG ulReason
    , LPVOID pvReserved)
{
    if (DLL_PROCESS_ATTACH==ulReason)
        ghModule = hInstance;
    return TRUE;
}

//***************************************************************************
//
//  DllGetClassObject
//
//  Purpose: Called by Ole when some client wants a class factory.  Return 
//           one only if it is the sort of class this DLL supports.
//
//***************************************************************************


STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, PPVOID ppv)
{
    HRESULT hr;
    CProvFactory *pObj;

    if (CLSID_instprovider!=rclsid)
        return E_FAIL;

    pObj=new CProvFactory();

    if (NULL==pObj)
        return E_OUTOFMEMORY;

    hr=pObj->QueryInterface(riid, ppv);

    if (FAILED(hr))
        delete pObj;

    return hr;
}

//***************************************************************************
//
// DllCanUnloadNow
//
// Purpose: Called periodically by Ole in order to determine if the
//          DLL can be freed.
//
// Return:  S_OK if there are no objects in use and the class factory 
//          isn't locked.
//
//***************************************************************************

STDAPI DllCanUnloadNow(void)
{
    SCODE   sc;

    //It is OK to unload if there are no objects or locks on the 
    // class factory.
    
    sc=(0L==g_cObj && 0L==g_cLock) ? S_OK : S_FALSE;
    return sc;
}

//***************************************************************************
//
// DllRegisterServer
//
// Purpose: Called during setup or by regsvr32.
//
// Return:  NOERROR if registration successful, error otherwise.
//***************************************************************************

STDAPI DllRegisterServer(void)
{   
	WCHAR		wcID[128];
    WCHAR       szCLSID[128];
    TCHAR       szModule[MAX_PATH + 1];
    const WCHAR * pName = L"WMI EDID Monitor Provider";
    const WCHAR * pModel = L"Both";
    HKEY hKey1, hKey2;
	size_t * intReturnValue = NULL;


    // Create the path.

	memset(wcID, NULL, sizeof(wcID));
    StringFromGUID2(CLSID_instprovider, wcID, sizeof(wcID)/sizeof(WCHAR));
	StringCbCopy(szCLSID, sizeof(szCLSID), L"Software\\classes\\CLSID\\");
	StringCbCat(szCLSID, sizeof(szCLSID), wcID);

    // Create entries under CLSID

	LONG lRet = RegCreateKeyEx( HKEY_LOCAL_MACHINE, szCLSID, 0, NULL,
                    REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, 
                    &hKey1, NULL );
	if (lRet != ERROR_SUCCESS)
	{
		return E_FAIL;
	}
	

    lRet = RegSetValueEx(hKey1, NULL, 0, REG_SZ, (BYTE *)pName, wcslen(pName)*sizeof(WCHAR)+1);
	if (lRet != ERROR_SUCCESS)
	{
		RegCloseKey(hKey1);
		return E_FAIL;
	}

	lRet = RegCreateKeyEx( hKey1, L"InprocServer32", 0, NULL,
                    REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, 
                    &hKey2, NULL );
	if (lRet != ERROR_SUCCESS)
	{
		RegCloseKey(hKey1);
		return E_FAIL;
	}

	memset(&szModule, NULL, sizeof(szModule));
	GetModuleFileName(ghModule, szModule, sizeof(szModule)/sizeof(TCHAR) - 1);

    lRet = RegSetValueEx(hKey2, NULL, 0, REG_SZ, (BYTE *)szModule, 
                                        wcslen(szModule)*sizeof(WCHAR)+1);
	if (lRet != ERROR_SUCCESS)
	{
	    RegCloseKey(hKey2);
		RegCloseKey(hKey1);
		return E_FAIL;
	}
    
	lRet = RegSetValueEx(hKey2, L"ThreadingModel", 0, REG_SZ, 
                                        (BYTE *)pModel, wcslen(pModel)*sizeof(WCHAR)+1);
	if (lRet != ERROR_SUCCESS)
	{
	    RegCloseKey(hKey2);
		RegCloseKey(hKey1);
		return E_FAIL;
	}
    RegCloseKey(hKey1);
    RegCloseKey(hKey2);
    return NOERROR;
}

//***************************************************************************
//
// DllUnregisterServer
//
// Purpose: Called when it is time to remove the registry entries.
//
// Return:  NOERROR if registration successful, error otherwise.
//***************************************************************************

STDAPI DllUnregisterServer(void)
{
	WCHAR	wcID[128];
    TCHAR	szCLSID[128];
    HKEY	hKey;
	size_t * intReturnValue = NULL;

    // Create the path using the CLSID

	memset(wcID, NULL, sizeof(wcID));	
    StringFromGUID2(CLSID_instprovider, wcID, sizeof(wcID)/sizeof(WCHAR));
	StringCbCopy(szCLSID, sizeof(szCLSID), L"Software\\classes\\CLSID\\");
	StringCbCat(szCLSID, sizeof(szCLSID), wcID);


    // First delete the InProcServer subkey.

    DWORD dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szCLSID, 0, KEY_WRITE, &hKey);
    if(dwRet == NO_ERROR)
    {
        RegDeleteKey(hKey, L"InProcServer32");
        RegCloseKey(hKey);
    }

    dwRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\classes\\CLSID", 0, KEY_WRITE, &hKey);
    if(dwRet == NO_ERROR)
    {
        RegDeleteKey(hKey,wcID);
        RegCloseKey(hKey);
    }

    return NOERROR;
}


