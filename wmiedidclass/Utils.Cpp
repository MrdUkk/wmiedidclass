/*
	Windows COM-server EDID monitor information provider by dUkk
	
	mainly developed for organization internal MS SCCM inventorization processes
	
	(c) 2013-2014
*/
#include <objbase.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <tchar.h>

#include "sample.h"
#include "edid.h"

#pragma comment(lib, "Setupapi.lib")


/******************************************************************************
 *
 *	Name:	GetCurrentImpersonationLevel
 *
 *	
 *  Description:
 *
 *			Get COM impersonation level of caller.	
 *
 *****************************************************************************/

DWORD GetCurrentImpersonationLevel ()
{
	DWORD t_ImpersonationLevel = RPC_C_IMP_LEVEL_ANONYMOUS ;

    HANDLE t_ThreadToken = NULL ;

    BOOL t_Status = OpenThreadToken (

		GetCurrentThread() ,
		TOKEN_QUERY,
		TRUE,
		&t_ThreadToken
	) ;

    if ( t_Status )
    {
		SECURITY_IMPERSONATION_LEVEL t_Level = SecurityAnonymous ;
		DWORD t_Returned = 0 ;

		t_Status = GetTokenInformation (

			t_ThreadToken ,
			TokenImpersonationLevel ,
			& t_Level ,
			sizeof ( SECURITY_IMPERSONATION_LEVEL ) ,
			& t_Returned
		) ;

		CloseHandle ( t_ThreadToken ) ;

		if ( t_Status == FALSE )
		{
			t_ImpersonationLevel = RPC_C_IMP_LEVEL_ANONYMOUS ;
		}
		else
		{
			switch ( t_Level )
			{
				case SecurityAnonymous:
				{
					t_ImpersonationLevel = RPC_C_IMP_LEVEL_ANONYMOUS ;
				}
				break ;

				case SecurityIdentification:
				{
					t_ImpersonationLevel = RPC_C_IMP_LEVEL_IDENTIFY ;
				}
				break ;

				case SecurityImpersonation:
				{
					t_ImpersonationLevel = RPC_C_IMP_LEVEL_IMPERSONATE ;
				}
				break ;

				case SecurityDelegation:
				{
					t_ImpersonationLevel = RPC_C_IMP_LEVEL_DELEGATE ;
				}
				break ;

				default:
				{
					t_ImpersonationLevel = RPC_C_IMP_LEVEL_ANONYMOUS ;
				}
				break ;
			}
		}
	}
	else
	{
        ULONG t_LastError = GetLastError () ;

        if ( t_LastError == ERROR_NO_IMPERSONATION_TOKEN || t_LastError == ERROR_NO_TOKEN )
        {
            t_ImpersonationLevel = RPC_C_IMP_LEVEL_DELEGATE ;
        }
        else 
		{
			if ( t_LastError == ERROR_CANT_OPEN_ANONYMOUS )
			{
				t_ImpersonationLevel = RPC_C_IMP_LEVEL_ANONYMOUS ;
			}
			else
			{
				t_ImpersonationLevel = RPC_C_IMP_LEVEL_ANONYMOUS ;
			}
		}
    }

	return t_ImpersonationLevel ;
}


SCODE EnumConnectedMonitors(IWbemServices *pNamespace, const BSTR RefStr, IWbemContext *pCtx, IWbemObjectSink FAR* pHandler)
{
	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	DWORD i;
	IWbemClassObject FAR* pNewInst;
	edid_info mon_info;

	// Create a HDEVINFO with all present devices.
	hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_MONITOR, 0, 0, DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		return WBEM_E_FAILED;
	}

	// Enumerate through all devices in Set.
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (i=0;SetupDiEnumDeviceInfo(hDevInfo,i,&DeviceInfoData);i++)
	{
		HKEY hDevRegKey = SetupDiOpenDevRegKey(hDevInfo,&DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
		if(!hDevRegKey || (hDevRegKey == INVALID_HANDLE_VALUE))
			continue;

		if(GetMonitorSizeFromEDID(hDevRegKey, &mon_info) ==true)
			//returned with info
		{
			RegCloseKey(hDevRegKey);
			//store data in WMI
			SCODE sc;
			IWbemClassObject * pClass = NULL;
			sc = pNamespace->GetObject(RefStr, 0, pCtx, &pClass, NULL);
			if(sc != S_OK) 
			{
				continue;
				//return WBEM_E_FAILED;
			}
			sc = pClass->SpawnInstance(0, &pNewInst);
			pClass->Release();
			if(FAILED(sc)) 
			{
				continue;
				//return sc;
			}
			VARIANT v;
			WCHAR string[64*2];

			// Set the key property value.
			v.vt = VT_I4;
			v.lVal = i;
			sc = pNewInst->Put(L"ID", 0, &v, 0);
			VariantClear(&v);

			// Set the number property value.
			v.vt = VT_I4;
			v.lVal = mon_info.monsizeh;
			sc = pNewInst->Put(L"MonSizeH", 0, &v, 0);
			VariantClear(&v);
			v.vt = VT_I4;
			v.lVal = mon_info.monsizev;
			sc = pNewInst->Put(L"MonSizeV", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_I4;
			v.lVal = mon_info.dispsizeh;
			sc = pNewInst->Put(L"DispSizeH", 0, &v, 0);
			VariantClear(&v);
			v.vt = VT_I4;
			v.lVal = mon_info.dispsizev;
			sc = pNewInst->Put(L"DispSizeV", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_I4;
			v.lVal = mon_info.mfgyear;
			sc = pNewInst->Put(L"MfgYear", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_I4;
			v.lVal = mon_info.mfgweek;
			sc = pNewInst->Put(L"MfgWeek", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_I4;
			v.lVal = mon_info.mfgmodelyear;
			sc = pNewInst->Put(L"MfgModelYear", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.identifier, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = pNewInst->Put(L"Identifier", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.vendor, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = pNewInst->Put(L"Vendor", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.modelname, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = pNewInst->Put(L"ModelName", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.mode, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = pNewInst->Put(L"Mode", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.serial, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = pNewInst->Put(L"Serial", 0, &v, 0);
			VariantClear(&v);

			pHandler->Indicate(1,&pNewInst);
			pNewInst->Release();
		}
		else
			RegCloseKey(hDevRegKey);
	}

	/*
	if ( GetLastError()!=NO_ERROR &&
	GetLastError()!=ERROR_NO_MORE_ITEMS )
	{
	// Insert error handling here.
	return FALSE;
	}
	*/
	//  Cleanup
	SetupDiDestroyDeviceInfoList(hDevInfo);
	
	return S_OK;
}

SCODE FindConnectedMonitors(IWbemServices * pNamespace, IWbemClassObject ** pNewInst, WCHAR * pwcClassName, IWbemContext *pCtx, int key)
{
	HDEVINFO hDevInfo;
	SP_DEVINFO_DATA DeviceInfoData;
	SCODE sc=S_OK;
	DWORD i;
	edid_info mon_info;

	// Create a HDEVINFO with all present devices.
	hDevInfo = SetupDiGetClassDevs(&GUID_DEVCLASS_MONITOR, 0, 0, DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		// Insert error handling here.
		return WBEM_E_FAILED;
	}

	// Enumerate through all devices in Set.
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	for (i=0;SetupDiEnumDeviceInfo(hDevInfo,i,&DeviceInfoData);i++)
	{
		HKEY hDevRegKey = SetupDiOpenDevRegKey(hDevInfo,&DeviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
		if(!hDevRegKey || (hDevRegKey == INVALID_HANDLE_VALUE))
			continue;

		if(GetMonitorSizeFromEDID(hDevRegKey, &mon_info) ==true && i==key)
			//returned with info
		{
			RegCloseKey(hDevRegKey);
			//store data in WMI
			IWbemClassObject * pClass = NULL;
			sc = pNamespace->GetObject(pwcClassName, 0, pCtx, &pClass, NULL);
			if(sc != S_OK) 
			{
				continue;
				//return WBEM_E_FAILED;
			}
			sc = pClass->SpawnInstance(0, pNewInst);
			pClass->Release();
			if(FAILED(sc)) 
			{
				continue;
				//return sc;
			}
			VARIANT v;
			WCHAR string[64*2];

			// Set the key property value.
			v.vt = VT_I4;
			v.lVal = i;
			sc = (*pNewInst)->Put(L"ID", 0, &v, 0);
			VariantClear(&v);

			// Set the number property value.
			v.vt = VT_I4;
			v.lVal = mon_info.monsizeh;
			sc = (*pNewInst)->Put(L"MonSizeH", 0, &v, 0);
			VariantClear(&v);
			v.vt = VT_I4;
			v.lVal = mon_info.monsizev;
			sc = (*pNewInst)->Put(L"MonSizeV", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_I4;
			v.lVal = mon_info.dispsizeh;
			sc = (*pNewInst)->Put(L"DispSizeH", 0, &v, 0);
			VariantClear(&v);
			v.vt = VT_I4;
			v.lVal = mon_info.dispsizev;
			sc = (*pNewInst)->Put(L"DispSizeV", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_I4;
			v.lVal = mon_info.mfgyear;
			sc = (*pNewInst)->Put(L"MfgYear", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_I4;
			v.lVal = mon_info.mfgweek;
			sc = (*pNewInst)->Put(L"MfgWeek", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_I4;
			v.lVal = mon_info.mfgmodelyear;
			sc = (*pNewInst)->Put(L"MfgModelYear", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.identifier, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = (*pNewInst)->Put(L"Identifier", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.vendor, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = (*pNewInst)->Put(L"Vendor", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.modelname, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = (*pNewInst)->Put(L"ModelName", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.mode, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = (*pNewInst)->Put(L"Mode", 0, &v, 0);
			VariantClear(&v);
			//
			v.vt = VT_BSTR;
			mbstowcs(string, mon_info.serial, 64);
			v.bstrVal = SysAllocString(string);
			if (!v.bstrVal) return WBEM_E_OUT_OF_MEMORY;
			sc = (*pNewInst)->Put(L"Serial", 0, &v, 0);
			VariantClear(&v);
			
			break;
		}
		else 
			RegCloseKey(hDevRegKey);
	}

	/*
	if ( GetLastError()!=NO_ERROR &&
	GetLastError()!=ERROR_NO_MORE_ITEMS )
	{
	// Insert error handling here.
	return FALSE;
	}
	*/
	//  Cleanup
	SetupDiDestroyDeviceInfoList(hDevInfo);

	return sc;
}

// Assumes hDevRegKey is valid
bool GetMonitorSizeFromEDID(const HKEY hDevRegKey, edid_info *ptr_to_edid)
{
	DWORD dwType, AcutalValueNameLength = 128;
	TCHAR valueName[128];

	BYTE EDIDdata[1024];
	DWORD edidsize=sizeof(EDIDdata);

	memset(ptr_to_edid, 0, sizeof(edid_info));

	for (LONG i = 0, retValue = ERROR_SUCCESS; retValue != ERROR_NO_MORE_ITEMS; ++i)
	{
		retValue = RegEnumValue ( hDevRegKey, i, &valueName[0], &AcutalValueNameLength, NULL, &dwType, EDIDdata, &edidsize);
		if (retValue != ERROR_SUCCESS || 0 != _tcscmp(valueName,_T("EDID")))
			continue;

		//grabbed data! so now try parse IT!
		return parse_edid(EDIDdata, ptr_to_edid);
	}

	return false; // EDID not found
}

