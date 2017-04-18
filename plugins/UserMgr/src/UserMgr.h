/**

  **/

#include <windows.h>
#include "pluginapi.h"
#include <stdio.h>
#include <lm.h>
#include <ntsecapi.h>
#include <windef.h>

#define if_break(x) if(!(x)); else switch(0) case 0: default:

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS  ((NTSTATUS)0x00000000L)
#endif

#define NSISFunction(funcname) void __declspec(dllexport) funcname(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters *extra)


BOOL InitLsaString(  PLSA_UNICODE_STRING pLsaString,  LPCWSTR pwszString );
LSA_HANDLE GetPolicyHandle();
NTSTATUS AddPrivileges(PSID AccountSID, LSA_HANDLE PolicyHandle, LSA_UNICODE_STRING lucPrivilege);
NTSTATUS RemovePrivileges(PSID AccountSID, LSA_HANDLE PolicyHandle, LSA_UNICODE_STRING lucPrivilege);
BOOL GetAccountSid(LPTSTR SystemName, LPTSTR AccountName, PSID *Sid);
