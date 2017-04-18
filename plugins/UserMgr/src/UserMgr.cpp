#include "UserMgr.h"
// JPR 123007: Added Userenv.h for the new BuiltAccountEnv function (Also Added Userenv.lib in the Link->Object/Library modules in the project settings)
// NOTE Platform SDK is needed for this header (The February 2003 build is the latest version which work with VC6)
#include <Userenv.h>
#include <winnls.h>
#include <AccCtrl.h>
#include <AclApi.h>
//#define _WIN32_WINNT 0x0501
#include <WinNT.h>
#include <Sddl.h>
#include <Lm.h>
#include <Dsgetdc.h>
#include <locale.h>
#include <memory.h>

#include "LogonUser.hpp"
#include "SSPLogon.h"
#include "Ping.hpp"


HINSTANCE g_hInstance = HINSTANCE();
HWND g_hwndParent = HWND();


void removeSubstringOnce(char * s, const char * toremove)
{
    const size_t len = strlen(toremove);
    if(s = strstr(s, toremove))
        memmove(s, s + len, 1 + strlen(s + len));
}

BOOL WINAPI DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    g_hInstance = (HINSTANCE)hInst;
    return TRUE;
}

static UINT_PTR PluginCallback(enum NSPIM msg)
{
  return 0;
}

NTSTATUS AddPrivileges(PSID AccountSID, LSA_HANDLE PolicyHandle, LSA_UNICODE_STRING lucPrivilege)
{
    NTSTATUS ntsResult;

    // Create an LSA_UNICODE_STRING for the privilege name(s).

    ntsResult = LsaAddAccountRights(PolicyHandle,    // An open policy handle.
                                    AccountSID,        // The target SID.
                                    &lucPrivilege,    // The privilege(s).
                                    1);                // Number of privileges.
    return ntsResult;
} 

NTSTATUS RemovePrivileges(PSID AccountSID, LSA_HANDLE PolicyHandle, LSA_UNICODE_STRING lucPrivilege)
{
    NTSTATUS ntsResult;

    // Create an LSA_UNICODE_STRING for the privilege name(s).

    ntsResult = LsaRemoveAccountRights( PolicyHandle,  // An open policy handle.
                                        AccountSID,    // The target SID.
                                        FALSE,         // Delete all rights? We should not even think about that...
                                        &lucPrivilege, // The privilege(s).
                                        1);            // Number of privileges.

    return ntsResult;

} 

NET_API_STATUS EnablePrivilege(LPCTSTR dwPrivilege)
{
   NET_API_STATUS nStatus = 0;
   HANDLE hProcessToken = NULL;
   TOKEN_PRIVILEGES tkp = TOKEN_PRIVILEGES();

    __try {
        if (!OpenProcessToken(GetCurrentProcess(), 
                                TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, 
                                &hProcessToken)) {
            nStatus = GetLastError();
            return nStatus;
        }

        tkp.PrivilegeCount = 1; 
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 

        if (!LookupPrivilegeValue(NULL, 
                                    dwPrivilege, 
                                    &tkp.Privileges[0].Luid)) {
            nStatus = GetLastError();
            return nStatus;
        }

        if (!AdjustTokenPrivileges(hProcessToken, 
                                    FALSE, 
                                    &tkp, 
                                    0, 
                                    NULL, 
                                    0))  {
            nStatus = GetLastError();
            return nStatus;
        }
    }
    __finally {
        if (hProcessToken) {
            CloseHandle(hProcessToken);
            hProcessToken = NULL; // just in case
        }
    }

    return nStatus;
}

LSA_HANDLE GetPolicyHandle()
{
    NTSTATUS ntsResult;
    LSA_OBJECT_ATTRIBUTES ObjectAttributes = LSA_OBJECT_ATTRIBUTES();
    LSA_HANDLE lsahPolicyHandle = LSA_HANDLE();

    // Get a handle to the Policy object.
    ntsResult = LsaOpenPolicy(NULL,               //only localhost
                            &ObjectAttributes, //Object attributes.
                            POLICY_ALL_ACCESS, //Desired access permissions.
                            &lsahPolicyHandle);//Receives the policy handle.
    if (ntsResult != STATUS_SUCCESS) {
        // An error occurred. Display it as a win32 error code.
        return NULL;
    }

    return lsahPolicyHandle;
}

BOOL InitLsaString(PLSA_UNICODE_STRING pLsaString, LPCWSTR pwszString)
{
    DWORD dwLen = 0;

    if (NULL == pLsaString)
        return FALSE;

    if (NULL != pwszString) {
        dwLen = wcslen(pwszString);
        if (dwLen > 0x7ffe)   // String is too large
        return FALSE;
    }

    // Store the string.
    pLsaString->Buffer = (WCHAR *)pwszString;
    pLsaString->Length =  (USHORT)dwLen * sizeof(WCHAR);
    pLsaString->MaximumLength= (USHORT)(dwLen+1) * sizeof(WCHAR);

    return TRUE;
}

BOOL GetAccountSid(LPTSTR SystemName, LPTSTR AccountName, PSID *Sid) 
{
    LPTSTR ReferencedDomain = NULL;
    DWORD cbSid = 128;    /* initial allocation attempt */
    DWORD cbReferencedDomain = 16; /* initial allocation size */
    SID_NAME_USE peUse;
    BOOL bSuccess = FALSE; /* assume this function will fail */

    __try {
        /*
         * initial memory allocations
         */
        if ((*Sid = HeapAlloc(GetProcessHeap(), 0, cbSid)) == NULL)
            __leave;

        if ((ReferencedDomain = (LPTSTR) HeapAlloc(GetProcessHeap(), 0,
                       cbReferencedDomain)) == NULL) __leave;

        /*
         * Obtain the SID of the specified account on the specified system.
         */
        while (!LookupAccountName(SystemName, AccountName, *Sid, &cbSid,
                      ReferencedDomain, &cbReferencedDomain,
                      &peUse))
        {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                /* reallocate memory */
                if ((*Sid = HeapReAlloc(GetProcessHeap(), 0,
                    *Sid, cbSid)) == NULL) __leave;

                if ((ReferencedDomain= (LPTSTR) HeapReAlloc(
                    GetProcessHeap(), 0, ReferencedDomain,
                    cbReferencedDomain)) == NULL)
                __leave;
            }
            else 
                __leave;
        }
        bSuccess = TRUE;
    } /* finally */
    __finally {

        /* Cleanup and indicate failure, if appropriate. */

        HeapFree(GetProcessHeap(), 0, ReferencedDomain);

        if (!bSuccess) {
            if (*Sid != NULL) {
                HeapFree(GetProcessHeap(), 0, *Sid);
                *Sid = NULL;
            }
        }

    }

    return (bSuccess);
}

extern "C" {

NSISFunction(CreateAccount)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        USER_INFO_1 ui = USER_INFO_1();
        DWORD dwLevel = 1;
        DWORD dwError = 0;

        // buffers at the end
        char userid[256] = {0};
        char passwd[256] = {0};
        char comment[1024] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_passwd[256] = {0};
        WCHAR u_comment[1024] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(passwd);
        swprintf(u_passwd, L"%S", passwd);

        popstring(comment);
        swprintf(u_comment, L"%S", comment);

        ui.usri1_name = u_userid;
        ui.usri1_password = u_passwd;
        ui.usri1_password_age = 0;
        ui.usri1_priv = USER_PRIV_USER;
        ui.usri1_home_dir = NULL;
        ui.usri1_comment = u_comment;
        ui.usri1_flags = UF_DONT_EXPIRE_PASSWD | UF_SCRIPT;

        nStatus = NetUserAdd(NULL,
                            dwLevel,
                            (LPBYTE)&ui,
                            &dwError);
        if (nStatus == NERR_Success) {
            pushstring("OK");
        }
        else {
            sprintf(userid, "ERROR %d", nStatus);
            pushstring(userid);
        }
    }
}


// JPR 123007: Added CreateAccountEx function
NSISFunction(CreateAccountEx)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        USER_INFO_2 ui = USER_INFO_2();
        DWORD dwLevel = 2;
        DWORD dwError = 0;

        // buffers at the end
        char userid[256] = {0};
        char passwd[256] = {0};
        char comment[1024] = {0};
        char fullname[256] = {0};
        char usr_comment[1024] = {0};
        char flags[1024] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_passwd[256] = {0};
        WCHAR u_comment[1024] = {0};
        WCHAR u_fullname[256] = {0};
        WCHAR u_usr_comment[1024] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(passwd);
        swprintf(u_passwd, L"%S", passwd);

        popstring(comment);
        swprintf(u_comment, L"%S", comment);

        popstring(fullname);
        swprintf(u_fullname, L"%S", fullname);

        popstring(usr_comment);
        swprintf(u_usr_comment, L"%S", usr_comment);

        popstring(flags);

        ui.usri2_name=u_userid;  
        ui.usri2_password=u_passwd;  
        ui.usri2_priv=USER_PRIV_USER;
        ui.usri2_home_dir=NULL;  
        ui.usri2_comment=u_comment;  
        ui.usri2_flags=UF_SCRIPT | UF_NORMAL_ACCOUNT;

        if(strstr(flags,"UF_ACCOUNTDISABLE")) ui.usri2_flags |= UF_ACCOUNTDISABLE;
        if(strstr(flags,"UF_PASSWD_NOTREQD")) ui.usri2_flags |= UF_PASSWD_NOTREQD;
        if(strstr(flags,"UF_PASSWD_CANT_CHANGE")) ui.usri2_flags |= UF_PASSWD_CANT_CHANGE;
        if(strstr(flags,"UF_DONT_EXPIRE_PASSWD")) ui.usri2_flags |= UF_DONT_EXPIRE_PASSWD;

        ui.usri2_script_path=NULL;
        ui.usri2_auth_flags=0;
        ui.usri2_full_name=u_fullname;
        ui.usri2_usr_comment=u_usr_comment;
        ui.usri2_parms=NULL;
        ui.usri2_workstations=NULL;
        ui.usri2_acct_expires=TIMEQ_FOREVER;
        ui.usri2_max_storage=USER_MAXSTORAGE_UNLIMITED;
        ui.usri2_logon_hours=NULL;
        ui.usri2_country_code=0;
        ui.usri2_code_page=0;

        nStatus = NetUserAdd(NULL,
                            dwLevel,
                            (LPBYTE)&ui,
                            &dwError);
        if (nStatus == NERR_Success) {
            pushstring("OK");
        }
        else {
            sprintf(userid, "ERROR %d", nStatus);
            pushstring(userid);
        }
    }
}

// Andrey Dibrov (andry at inbox dot ru): synchronious ping by IP
NSISFunction(Ping)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        HANDLE hIcmpFile = NULL;
        IPAddr ipaddr = INADDR_NONE;
        BOOL error_processed = FALSE;
        DWORD dwTimeout = 0;

        // buffers at the end
        char address[256] = {0};
        char data[256] = {0};
        char timeout[65] = {0};

        g_hwndParent = hwndParent;

        // drop return values
        g_ping_reply = PING_REPLY();

        popstring(address);
        popstring(data);
        popstring(timeout);

        dwTimeout = atoi(timeout);

        nStatus = _Ping(address, data, dwTimeout, g_ping_reply);
        if (nStatus) {
            sprintf(address, "ERROR %d", nStatus);
            pushstring(address);
        } else {
            pushstring("OK");
        }
    }
}


// Andrey Dibrov (andry at inbox dot ru): synchronious ping by IP status message
NSISFunction(GetPingStatusMessage)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        char * str = NULL;
        DWORD dwStatus;

        // buffers at the end
        char status[256] = {0};

        g_hwndParent = hwndParent;

        popstring(status);

        dwStatus = atoi(status);

        switch(dwStatus) {
            case IP_SUCCESS: str = "IP_SUCCESS"; break;
            case IP_BUF_TOO_SMALL: str = "IP_BUF_TOO_SMALL"; break;
            case IP_DEST_NET_UNREACHABLE: str = "IP_DEST_NET_UNREACHABLE"; break;
            case IP_DEST_HOST_UNREACHABLE: str = "IP_DEST_HOST_UNREACHABLE"; break;
            case IP_DEST_PROT_UNREACHABLE: str = "IP_DEST_PROT_UNREACHABLE"; break;
            case IP_DEST_PORT_UNREACHABLE: str = "IP_DEST_PORT_UNREACHABLE"; break;
            case IP_NO_RESOURCES: str = "IP_NO_RESOURCES"; break;
            case IP_BAD_OPTION: str = "IP_BAD_OPTION"; break;
            case IP_HW_ERROR: str = "IP_HW_ERROR"; break;
            case IP_PACKET_TOO_BIG: str = "IP_PACKET_TOO_BIG"; break;
            case IP_REQ_TIMED_OUT: str = "IP_REQ_TIMED_OUT"; break;
            case IP_BAD_REQ: str = "IP_BAD_REQ"; break;
            case IP_BAD_ROUTE: str = "IP_BAD_ROUTE"; break;
            case IP_TTL_EXPIRED_TRANSIT: str = "IP_TTL_EXPIRED_TRANSIT"; break;
            case IP_TTL_EXPIRED_REASSEM: str = "IP_TTL_EXPIRED_REASSEM"; break;
            case IP_PARAM_PROBLEM: str = "IP_PARAM_PROBLEM"; break;
            case IP_SOURCE_QUENCH: str = "IP_SOURCE_QUENCH"; break;
            case IP_OPTION_TOO_BIG: str = "IP_OPTION_TOO_BIG"; break;
            case IP_BAD_DESTINATION: str = "IP_BAD_DESTINATION"; break;

            case IP_ADDR_DELETED: str = "IP_ADDR_DELETED"; break;
            case IP_SPEC_MTU_CHANGE: str = "IP_SPEC_MTU_CHANGE"; break;
            case IP_MTU_CHANGE: str = "IP_MTU_CHANGE"; break;
            case IP_UNLOAD: str = "IP_UNLOAD"; break;
            case IP_GENERAL_FAILURE: str = "IP_GENERAL_FAILURE"; break;
            case IP_PENDING: str = "IP_PENDING"; break;
            default: str = ""; break;
        }

        pushstring(str);
    }
}


// Andrey Dibrov (andry at inbox dot ru): get last synchronious ping reply data
NSISFunction(GetLastPingReply)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        char * str = NULL;
        struct in_addr ReplyAddr = struct in_addr();

        // buffers at the end
        char buf[256] = {0};

        g_hwndParent = hwndParent;

        // reply
        g_ping_reply.reply[sizeof(g_ping_reply.reply) - 1] = '\0' ; // safe truncation
        pushstring(g_ping_reply.reply);

        // num responses
        str = itoa(g_ping_reply.num_responces, buf, 10);
        pushstring(str ? str : "");

        // reply from address
        ReplyAddr.S_un.S_addr = g_ping_reply.icmp_echo.Address;
        str = inet_ntoa(ReplyAddr);
        str = _tcscpy(buf, str ? str : "");
        pushstring(str ? str : "");

        // status
        str = itoa(g_ping_reply.icmp_echo.Status, buf, 10);
        str = _tcscpy(buf, str ? str : "");
        pushstring(str ? str : "");

        // Round Trip Time (ms)
        str = itoa(g_ping_reply.icmp_echo.RoundTripTime, buf, 10);
        str = _tcscpy(buf, str ? str : "");
        pushstring(str ? str : "");
    }
}


// Andrey Dibrov (andry at inbox dot ru): asynchronous ping
// CAUTION:
//  1. Calls for the same address+data from different thread is not safe and must be synchronized explicitly
NSISFunction(PingAsync)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        PingAsyncRequestHandle req_handle = PingAsyncRequestHandle();
        DWORD dwTimeout = 0;

        // buffers at the end
        char address[256] = {0};
        char data[256] = {0};
        char timeout[65] = {0};

        g_hwndParent = hwndParent;

        popstring(address);
        popstring(data);
        popstring(timeout);

        dwTimeout = atoi(timeout);

        req_handle = _PingAsync(address, data, dwTimeout);

        sprintf(address, "%u", req_handle);
        pushstring(address);
        pushstring("OK"); // reserved
    }
}


// Andrey Dibrov (andry at inbox dot ru): asynchronous ping status
NSISFunction(GetPingAsyncStatus)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        PingAsyncRequestHandle req_handle = PingAsyncRequestHandle();
        struct in_addr ReplyAddr = struct in_addr();
        DWORD req_status;
        DWORD req_last_error;
        char * str = NULL;

        // buffers at the end
        char req_handle_str[256] = {0};
        char buf[256] = {0};
        PING_REPLY ping_reply = PING_REPLY();

        g_hwndParent = hwndParent;

        popstring(req_handle_str);

        req_handle.handle = strtoul(req_handle_str, NULL, 10);

        req_status = _GetPingAsyncStatus(req_handle, &req_last_error, &ping_reply);
        if (req_status == ASYNC_REQUEST_STATUS_ACCOMPLISH) {
            // reply
            pushstring(ping_reply.reply);

            // num responses
            str = itoa(ping_reply.num_responces, buf, 10);
            pushstring(str ? str : "");

            // reply from address
            ReplyAddr.S_un.S_addr = ping_reply.icmp_echo.Address;
            str = inet_ntoa(ReplyAddr);
            pushstring(str ? str : "");

            // status
            str = itoa(ping_reply.icmp_echo.Status, buf, 10);
            pushstring(str ? str : "");

            // Round Trip Time (ms)
            str = itoa(g_ping_reply.icmp_echo.RoundTripTime, buf, 10);
            pushstring(str ? str : "");
        }
        else {
            pushstring("");
            pushstring("");
            pushstring("");
            pushstring("");
            pushstring("");
        }

        if (req_last_error != NO_ERROR) {
            sprintf(req_handle_str, "ERROR %d", req_last_error);
            pushstring(req_handle_str);
        }
        else {
            pushstring("OK");
        }

        str = itoa(req_status, req_handle_str, 10);
        pushstring(str ? str : "");
    }
}


// Andrey Dibrov (andry at inbox dot ru): cancel asynchronous ping
NSISFunction(CancelPingAsync)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        DWORD result;
        PingAsyncRequestHandle req_handle = PingAsyncRequestHandle();

        // buffers at the end
        char req_handle_str[256] = {0};

        g_hwndParent = hwndParent;

        popstring(req_handle_str);

        req_handle.handle = strtoul(req_handle_str, NULL, 10);

        result = _CancelPingAsyncRequest(req_handle);
        if (result) {
            sprintf(req_handle_str, "ERROR %d", result);
            pushstring(req_handle_str);
        }
        else {
            pushstring("OK");
        }
    }
}


// Andrey Dibrov (andry at inbox dot ru): synchronous user logon try with password
// Requirements:
//  1. The user must be already existed, otherwise will return OK
//  2. The password must be not empty, otherwise will return error
//     (to test on empty password do check on 1327 error code: https://www.codeproject.com/articles/19992/how-to-detect-empty-password-users)
NSISFunction(TryLogonUser)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        HANDLE hLogonToken = NULL;

        // buffers at the end
        char userid[256] = {0};
        char passwd[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        popstring(passwd);

        __try {
            SetLastError(0); // just in case

            if(!LogonUser(userid,
                        ".",
                        passwd,
                        LOGON32_LOGON_NETWORK,
                        LOGON32_PROVIDER_DEFAULT,
                        &hLogonToken)) {
                nStatus = GetLastError();
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
            }
            else {
                pushstring("OK");
            }
        }
        __finally {
            if (hLogonToken) {
                CloseHandle(hLogonToken);
                hLogonToken = NULL; // just in case
            }
        }

        return;
    }
}


void _LockWNetError()
{
    g_wnet_error_mutex.lock();
}

void _UnlockWNetError()
{
    g_wnet_error_mutex.unlock();
}

void _LogonSyncRequestWNetErrorUnlock()
{
    _UnlockWNetError();
}

class _LogonSyncRequestWNetErrorLocker : public _LogonWNetErrorLocker
{
private:
    virtual void operator()(DWORD stage, void (*& unlock)(), DWORD *& wnet_error_code_, char *& wnet_error_str_, DWORD & wnet_error_str_buf_size_) {
        _LogonWNetErrorLocker::operator()(stage, unlock, wnet_error_code_, wnet_error_str_, wnet_error_str_buf_size_);

        unlock = _LogonSyncRequestWNetErrorUnlock;

        wnet_error_code_ = &g_wnet_error_locker.wnet_error_code;
        wnet_error_str_ = g_wnet_error_locker.wnet_error_str;
        wnet_error_str_buf_size_ = g_wnet_error_locker.wnet_error_str_buf_size;

        _LockWNetError();
        if (!stage) { // initialization stage
            *wnet_error_code_ = 0;
            wnet_error_str_[0] = '\0';
        }
    }
};

// Andrey Dibrov (andry at inbox dot ru): synchronious net share logon try with password
// CAUTION:
//  1. Calls for the same remoteid+userid+passwd from different thread is not safe and must be synchronized explicitly
NSISFunction(TryLogonNetShare)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        DWORD result = 0;
        _LogonSyncRequestWNetErrorLocker wnet_error_locker;

        // buffers at the end
        char remoteid[256] = {0};
        char userid[256] = {0};
        char passwd[256] = {0};

        g_hwndParent = hwndParent;

        popstring(remoteid);
        popstring(userid);
        popstring(passwd);

        result = _LogonUser(remoteid, userid, passwd, wnet_error_locker);
        if (result != NO_ERROR) {
            sprintf(userid, "ERROR %d", result);
            pushstring(userid);
        }
        else {
            pushstring("OK");
        }
    }
}


// Andrey Dibrov (andry at inbox dot ru): return last error code after TryLogonNetShare call
NSISFunction(GetLogonNetShareError)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        char * str = NULL;

        // buffers at the end
        char buf[256] = {0};

        g_hwndParent = hwndParent;

        __try {
            _LockWNetError();
            str = itoa(g_wnet_error_locker.wnet_error_code, buf, 10);
            pushstring(g_wnet_error_locker.wnet_error_str);
        }
        __finally {
            _UnlockWNetError();
        }

        pushstring(str ? str : "");
    }
}


// Andrey Dibrov (andry at inbox dot ru): local policy password validate
NSISFunction(ValidateLogonPass)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS status;
        NET_VALIDATE_PASSWORD_CHANGE_INPUT_ARG InputArg = NET_VALIDATE_PASSWORD_CHANGE_INPUT_ARG();
        DWORD res = 0;
        WCHAR * wzServer = NULL;
        PDOMAIN_CONTROLLER_INFO DcInfo = PDOMAIN_CONTROLLER_INFO();
        NET_VALIDATE_OUTPUT_ARG * pOutputArg = NULL;
        LPTSTR pSiteName = NULL;
        LPWKSTA_USER_INFO_1 wksta_info = LPWKSTA_USER_INFO_1();
        HANDLE hLogonToken = NULL;

        // buffers at the end
        char remoteid[256] = {0};
        char passwd[256] = {0};

        WCHAR u_dc[256] = {0};
        WCHAR u_passwd[256] = {0};

        g_hwndParent = hwndParent;

        popstring(remoteid);
        popstring(passwd);

        __try {
            if (!remoteid[0]) {
                // get address of nearest DC as cached by OS at boot time
                status = DsGetDcName(NULL, NULL, NULL, NULL,
                    DS_DIRECTORY_SERVICE_REQUIRED | DS_RETURN_FLAT_NAME, &DcInfo);
                if(!status) {
#if _UNICODE
                    wzServer = DcInfo->DomainControllerName;
#else
                    swprintf(u_dc, L"%s", DcInfo->DomainControllerName);
                    wzServer = u_dc;
#endif
                }
                else {
                    SetLastError(0); // just in case

                    status = NetWkstaUserGetInfo(NULL, 1, (LPBYTE *)&wksta_info);
                    if (status == NERR_Success)
                    {
                        swprintf(u_dc, L"\\\\%s", wksta_info->wkui1_logon_domain); // domain w/o prefix
                        wzServer = u_dc;
                    }
                    else
                    {
                        status = GetLastError();
                        sprintf(remoteid, "ERROR %d", status);
                        pushstring(remoteid);
                        return;
                    }
                }
            }
            else {
                // use remoteid as DC name
                swprintf(u_dc, L"%hs", remoteid);
                wzServer = u_dc;
            }

            swprintf(u_passwd, L"%hs", passwd);
            InputArg.ClearPassword = u_passwd;
            InputArg.PasswordMatch = TRUE;
            status = NetValidatePasswordPolicy(wzServer, NULL, NetValidatePasswordChange, &InputArg, (void **)&pOutputArg);
            if(status != NERR_Success) {
                sprintf(remoteid, "ERROR %d", status);
                pushstring(remoteid);
            }
            else if(pOutputArg->ValidationStatus) {
                sprintf(remoteid, "ERROR %d", pOutputArg->ValidationStatus);
                pushstring(remoteid);
            } else {
                pushstring("OK");
            }
        }
        __finally {
            if (hLogonToken) {
                CloseHandle(hLogonToken);
                hLogonToken = NULL; // just in case
            }
            if (wksta_info) {
                NetApiBufferFree(wksta_info);
                wksta_info = LPWKSTA_USER_INFO_1(); // just in case
            }
            if (pSiteName) {
                NetApiBufferFree(pSiteName);
                pSiteName = NULL; // just in case
            }
            if (pOutputArg) {
                NetValidatePasswordPolicyFree((void **)&pOutputArg);
                pOutputArg = NULL; // just in case
            }
        }
    }
}


// Andrey Dibrov (andry at inbox dot ru): asynchronious net share logon try with password
// CAUTION:
//  1. Calls for the same remoteid+userid+passwd from different thread is not safe and must be synchronized explicitly
NSISFunction(TryLogonNetShareAsync)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        LogonAsyncRequestHandle req_handle = LogonAsyncRequestHandle();

        // buffers at the end
        char remoteid[256] = {0};
        char userid[256] = {0};
        char passwd[256] = {0};

        g_hwndParent = hwndParent;

        popstring(remoteid);
        popstring(userid);
        popstring(passwd);

        req_handle = _LogonNetShareAsync(remoteid, userid, passwd);

        sprintf(userid, "%u", req_handle);
        pushstring(userid);
        pushstring("OK"); // reserved
    }
}


// Andrey Dibrov (andry at inbox dot ru): get asynchronous net share logon status
NSISFunction(GetLogonNetShareAsyncStatus)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        LogonAsyncRequestHandle req_handle = LogonAsyncRequestHandle();
        DWORD req_status;
        DWORD req_last_error;
        DWORD wnet_error_code;
        char * str = NULL;

        // buffers at the end
        char req_handle_str[256] = {0};
        char wnet_error_str[256] = {0};

        g_hwndParent = hwndParent;

        popstring(req_handle_str);

        req_handle.handle = strtoul(req_handle_str, NULL, 10);

        req_status = _GetLogonNetShareAsyncStatus(req_handle, &req_last_error, &wnet_error_code, wnet_error_str);

        pushstring(wnet_error_str);
        str = itoa(wnet_error_code, req_handle_str, 10);
        pushstring(str ? str : "");
        if (req_last_error != NO_ERROR) {
            sprintf(req_handle_str, "ERROR %d", req_last_error);
            pushstring(req_handle_str);
        }
        else {
            pushstring("OK");
        }

        str = itoa(req_status, req_handle_str, 10);
        pushstring(str ? str : "");
    }
}


// Andrey Dibrov (andry at inbox dot ru): cancel asynchronous net share logon
NSISFunction(CancelLogonNetShareAsync)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        LogonAsyncRequestHandle req_handle = LogonAsyncRequestHandle();
        DWORD result;

        // buffers at the end
        char req_handle_str[256] = {0};

        g_hwndParent = hwndParent;

        popstring(req_handle_str);

        req_handle.handle = strtoul(req_handle_str, NULL, 10);

        result = _CancelLogonNetShareAsyncRequest(req_handle);
        if (result) {
            sprintf(req_handle_str, "ERROR %d", result );
            pushstring(req_handle_str);
        }
        else {
            pushstring("OK");
        }
    }
}


// Andrey Dibrov (andry at inbox dot ru): asynchronious request status message
NSISFunction(GetAsyncRequestStatusString)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        char * str = NULL;
        DWORD req_status;

        // buffers at the end
        char req_status_str[256] = {0};

        g_hwndParent = hwndParent;

        popstring(req_status_str);

        req_status = atoi(req_status_str);

        switch(req_status) {
            case ASYNC_REQUEST_STATUS_UNINIT: str = "STATUS_UNINIT"; break;
            case ASYNC_REQUEST_STATUS_PENDING: str = "STATUS_PENDING"; break;
            case ASYNC_REQUEST_STATUS_ACCOMPLISH: str = "STATUS_ACCOMPLISH"; break;
            case ASYNC_REQUEST_STATUS_ABORTED: str = "STATUS_ABORTED"; break;
            case ASYNC_REQUEST_STATUS_CANCELLED: str = "STATUS_CANCELLED"; break;
            case ASYNC_REQUEST_STATUS_NOT_FOUND: str = "STATUS_NOT_FOUND"; break;
            default: str = ""; break;
        }

        pushstring(str);
    }
}


// Andrey Dibrov (andry at inbox dot ru): to try logon with password to test it using SSPI
// Details:
//  How to validate user credentials on Microsoft operating systems
//  https://support.microsoft.com/en-us/kb/180548
NSISFunction(TrySSPLogonUser)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        HANDLE hLogonToken = NULL;

        // buffers at the end
        char domain[256] = {0};
        char userid[256] = {0};
        char passwd[256] = {0};
        char provider[256] = {0};

        g_hwndParent = hwndParent;

        popstring(domain);
        popstring(userid);
        popstring(passwd);
        popstring(provider);

        if(!_SSPLogonUser(domain, userid, passwd, provider)) {
            sprintf(userid, "ERROR %d", g_SSPLastError);
            pushstring(userid);
        }
        else {
            pushstring("OK");
        }
    }
}


NSISFunction(GetSSPLogonUserError)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        char * str = NULL;

        // buffers at the end
        char buf[256] = {0};

        g_hwndParent = hwndParent;

        str = itoa(g_SSPLastError, buf, 10);

        pushstring(str ? str : "");
        pushstring(g_SSPLastErrorStr);
    }
}


// JPR 123007: Added BuiltAccountEnv function
NSISFunction(BuiltAccountEnv)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        HANDLE hLogonToken = NULL;
        PROFILEINFO PI = PROFILEINFO();

        // buffers at the end
        char userid[256] = {0};
        char passwd[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        popstring(passwd);

        __try {
            nStatus = EnablePrivilege(SE_RESTORE_NAME);
            if (nStatus) {
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            if(!LogonUser(userid,
                        ".",
                        passwd,
                        LOGON32_LOGON_INTERACTIVE,
                        LOGON32_PROVIDER_DEFAULT,
                        &hLogonToken)) {
                nStatus = GetLastError();
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            PI.dwSize = sizeof(PROFILEINFO);
            PI.dwFlags = 0;
            PI.lpUserName = userid;
            PI.lpProfilePath = NULL;
            PI.lpDefaultPath = NULL;
            PI.lpServerName = NULL;
            PI.lpPolicyPath = NULL;
            PI.hProfile = HKEY_CURRENT_USER;

            if(!LoadUserProfile(hLogonToken, &PI)) {
                nStatus = GetLastError();
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            if(!UnloadUserProfile(hLogonToken, PI.hProfile)) {
                nStatus = GetLastError();
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            pushstring("OK");
        }
        __finally {
            if (hLogonToken) {
                CloseHandle(hLogonToken);
                hLogonToken = NULL; // just in case
            }
        }
    }
}


// JPR 123007: Added RegLoadUserHive function
NSISFunction(RegLoadUserHive)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        HKEY hKey = NULL;
        DWORD valueSize = 0;
        PSID user_sid = PSID();
        LPTSTR strSid = NULL;

        // buffers at the end
        char userid[256] = {0};
        char NTUser_dat[256] = {0};
        char DocumentsAndSettings[256] = {0};
        char DocumentsAndSettingsT[256] = {0};
        char SYSTEMDRIVE[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);

        __try {
            nStatus = EnablePrivilege(SE_RESTORE_NAME);
            if (nStatus) {
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            GetEnvironmentVariable("SYSTEMDRIVE", SYSTEMDRIVE, sizeof(SYSTEMDRIVE)/sizeof(SYSTEMDRIVE[0]));
            if (!GetAccountSid(NULL, userid, &user_sid)) {
                nStatus = GetLastError();
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            if (!ConvertSidToStringSid(user_sid, &strSid)) {
                nStatus = GetLastError();
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            sprintf(DocumentsAndSettings, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\%s", strSid);

            RegOpenKeyEx(HKEY_LOCAL_MACHINE, DocumentsAndSettings, 0, KEY_READ, &hKey);
            // JPR 011508 Get localized "Documents and Settings" string
            RegQueryValueEx(hKey, "ProfileImagePath", NULL, NULL, (LPBYTE)DocumentsAndSettingsT, &valueSize);
            // JPR 011508 Remove "%SystemDrive%\"
            sprintf(DocumentsAndSettings, "%s", &DocumentsAndSettingsT[14]);
            sprintf(NTUser_dat, "%s\\%s\\NTUSER.DAT", SYSTEMDRIVE,DocumentsAndSettings);

            nStatus = RegLoadKey(HKEY_USERS, userid, NTUser_dat);
            if (nStatus != NERR_Success) {
                sprintf(userid, "ERROR  %d", nStatus);
                pushstring(userid);
            }
            else {
                pushstring("OK");
            }
        }
        __finally {
            if (hKey) {
                RegCloseKey(hKey);
                hKey = NULL;
            }
        }
    }
}


// JPR 123007: Added RegUnLoadUserHive function
NSISFunction(RegUnLoadUserHive)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;

        // buffers at the end
        char userid[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);

        nStatus = RegUnLoadKey(HKEY_USERS, userid);
        if (nStatus == NERR_Success) {
            pushstring("OK");
        }
        else {
            sprintf(userid, "ERROR %d", nStatus);
            pushstring(userid);
        }
    }
}

NSISFunction(DeleteAccount)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;

        // buffers at the end
        char userid[256] = {0};
        WCHAR u_userid[256] = {0};

        g_hwndParent=hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        nStatus = NetUserDel(NULL, u_userid);
        if (nStatus == NERR_Success) {
            pushstring("OK");
        }
        else {
            sprintf(userid, "ERROR %d", nStatus);
            pushstring(userid);
        }
    }
}


// JPR 011208: Added GetCurrentUserName function
NSISFunction(GetCurrentUserName)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        DWORD Size = 256;

        // buffers at the end
        char userid[256] = {0};

        g_hwndParent = hwndParent;

        nStatus = GetUserName(userid, &Size);
        if (nStatus) {
            pushstring(userid);
        }
        else {
            sprintf(userid, "ERROR %d", GetLastError());
            pushstring(userid);
        }
    }
}


// JPR 012109: Added GetCurrentDomain function
NSISFunction(GetCurrentDomain)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        LPWKSTA_USER_INFO_1 wksta_info = NULL;

        // buffers at the end
        char userdomain[256] = {0};

        g_hwndParent = hwndParent;

        __try {
            nStatus = NetWkstaUserGetInfo(NULL, 1, (LPBYTE *)&wksta_info);
            if (nStatus == NERR_Success) {
                sprintf(userdomain, "%S", wksta_info->wkui1_logon_domain);
                pushstring(userdomain);
            }
            else {
                sprintf(userdomain, "ERROR %d", GetLastError());
                pushstring(userdomain);
            }
        }
        __finally {
            if (wksta_info) {
                NetApiBufferFree(wksta_info);
                wksta_info = NULL; // just in case
            }
        }
    }
}

// JPR 011208: Added GetLocalizedStdAccountName function
NSISFunction(GetLocalizedStdAccountName)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        PSID pSid = NULL;
        DWORD usize = 256;
        DWORD dsize = 256;
        DWORD SidSize = SECURITY_MAX_SID_SIZE;
        SID_NAME_USE snu = SID_NAME_USE();

        // buffers at the end
        char pid[256] = {0};
        char username[256] = {0};
        char domain[256] = {0};

        g_hwndParent = hwndParent;

        popstring(pid);

        __try {
            pSid = LocalAlloc(LMEM_FIXED, SidSize);
            if(!ConvertStringSidToSid(pid, &pSid)) {
                sprintf(pid, "ERROR");
                pushstring(pid);
                return;
            }

            if(!LookupAccountSid(NULL, pSid, username, &usize, domain, &dsize, &snu))
            {
                sprintf(pid, "ERROR");
                pushstring(pid);
                return;
            }

            sprintf(pid,"%s\\%s", domain, username);
            pushstring(pid);
        }
        __finally {
            if (pSid) {
                LocalFree(pSid);
                pSid = NULL; // just in case
            }
        }
    }
}

// JPR 020909: Added GetUserNameFromSID function
NSISFunction(GetUserNameFromSID)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        PSID pSid = NULL;
        DWORD usize = 256;
        DWORD dsize = 256;
        DWORD SidSize = SECURITY_MAX_SID_SIZE;
        SID_NAME_USE snu = SID_NAME_USE();

        // buffers at the end
        char pid[256] = {0};
        char username[256] = {0};
        char domain[256] = {0};

        g_hwndParent = hwndParent;

        popstring(pid);

        __try {
            pSid = LocalAlloc(LMEM_FIXED, SidSize);
            if(!ConvertStringSidToSid(pid, &pSid)) {
                sprintf(pid, "ERROR");
                pushstring(pid);
                return;
            }

            if(!LookupAccountSid(NULL, pSid, username, &usize, domain, &dsize, &snu)) {
                sprintf(pid, "ERROR");
                pushstring(pid);
                return;
            }

            sprintf(pid, "%s", domain);
            if (domain[0]) {
                sprintf(pid,"%s\\%s", domain, username);
            }
            else {
                sprintf(pid, "%s", username);
            }
            pushstring(pid);
        }
        __finally {
            if (pSid) {
                LocalFree(pSid);
                pSid = NULL; // just in case
            }
        }
    }
}

// Andrey Dibrov (andry at inbox dot ru): if empty domain - local system
// JPR 020909: Added GetSIDFromUserName function
NSISFunction(GetSIDFromUserName)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        LPWKSTA_USER_INFO_1 wksta_info = NULL;
        PSID user_sid = NULL;
        LPTSTR strSid = NULL;

        char domain[256] = {0};
        char userid[256] = {0};

        g_hwndParent = hwndParent;

        popstring(domain);
        popstring(userid);

        __try {
            if (!GetAccountSid(domain[0] ? domain : NULL, userid, &user_sid)) {
                pushstring("ERROR GetAccountSid");
                return;
            }

            if (!ConvertSidToStringSid(user_sid, &strSid)) {
                pushstring("ERROR ConvertSidToStringSid");
                return;
            }

            sprintf(userid, "%s", strSid);
            pushstring(userid);
        }
        __finally {
            if (wksta_info) {
                NetApiBufferFree(wksta_info);
                wksta_info = NULL; // just in case
            }
        }
    }
}


// Andrey Dibrov (andry at inbox dot ru): to check user existence
NSISFunction(GetUser)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        LPUSER_INFO_0 ui = NULL;

        // buffers at the end
        char userid[256] = {0};
        WCHAR u_userid[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        __try {
            nStatus = NetUserGetInfo(NULL,
                                    u_userid,
                                    0,
                                    (LPBYTE *)&ui);
            if (nStatus != NERR_Success) {
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
            }
            else {
                pushstring("OK");
            }
        }
        __finally {
            if (ui) {
                NetApiBufferFree(ui);
                ui = NULL; // just in case
            }
        }
    }
}

NSISFunction(GetUserInfo)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        LPUSER_INFO_2 ui = NULL;
        DWORD dwLevel = 2;
        DWORD dwError = 0;

        // buffers at the end
        char userid[256] = {0};
        char field[256] = {0};
        char response[1024] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_field[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(field);
        _strupr(field);

        swprintf(u_field, L"%S", field);

        __try {
            //
            //  Set up the USER_INFO_1 structure.
            //  USER_PRIV_USER: name identifies a user, 
            //  rather than an administrator or a guest.
            //  UF_SCRIPT: required for LAN Manager 2.0 and
            //  Windows NT and later.
            //
            nStatus = NetUserGetInfo(NULL,
                                    u_userid,
                                    dwLevel,
                                    (LPBYTE *)&ui);
            if (nStatus != NERR_Success) {
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            if (!strcmp(field, "EXISTS")) {
                pushstring("OK");
                return;
            }

            if (!strcmp(field, "FULLNAME")) {
                sprintf(response, "%S", ui->usri2_full_name);
                pushstring(response);
                return;
            }

            if (!strcmp(field, "COMMENT")) {
                sprintf(response, "%S", ui->usri2_comment);
                pushstring(response);
                return;
            }

            if (!strcmp(field, "NAME")) {
                sprintf(response, "%S", ui->usri2_name);
                pushstring(response);
                return;
            }

            if (!strcmp(field, "HOMEDIR")) {
                sprintf(response, "%S", ui->usri2_home_dir);
                pushstring(response);
                return;
            }

            if (!strcmp(field, "PASSWD_STATUS")) {
                if (ui->usri2_flags & UF_DONT_EXPIRE_PASSWD) {
                    pushstring("NEVEREXPIRES");
                }
                else if (ui->usri2_flags & UF_PASSWD_CANT_CHANGE) {
                    pushstring ("CANTCHANGE");
                }
                return;
            }

            pushstring("ERROR");
        }
        __finally {
            if (ui) {
                NetApiBufferFree(ui);
                ui = NULL; // just in case
            }
        }
    }
}

NSISFunction(SetUserInfo)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        LPUSER_INFO_2 ui = NULL;
        LPUSER_INFO_2 uiTemp = NULL;
        // JPR 123007: Needed to change a user password
        USER_INFO_1003 ui1003 = USER_INFO_1003();
        // JPR 020108: Use USER_INFO_1011 to change the users fullname instead of USER_INFO_1
        USER_INFO_1011 ui1011 = USER_INFO_1011();
        DWORD dwLevel = 2;
        DWORD dwError = 0;

        // buffers at the end
        char userid[256] = {0};
        char field[256] = {0};
        char newvalue[256] = {0};
        char response[1024] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_field[256] = {0};
        WCHAR u_pwd[256] = {0};
        WCHAR u_fullname[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(field);
        _strupr(field);

        popstring(newvalue);

        swprintf(u_field, L"%S", field);

        __try {
            nStatus = NetUserGetInfo(NULL, 
                                    u_userid, 
                                    dwLevel, 
                                    (LPBYTE *)&ui);
            if (nStatus != NERR_Success) {
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            // JPR 011208: Copy ui buffer to a temp buffer so original buffer will not be invalidated
            if (!(uiTemp = ui)) {
                sprintf(userid, "ERROR INVALID USERINFO");
                pushstring(userid);
                return;
            }

            if (!strcmp(field, "FULLNAME")) {
                swprintf(u_fullname, L"%S", newvalue);
                ui1011.usri1011_full_name = u_fullname;
                dwLevel = 1011;
            }

            // JPR 123007: Added PASSWORD field
            if (!strcmp(field, "PASSWORD")) {
                swprintf(u_pwd, L"%S", newvalue);
                ui1003.usri1003_password = u_pwd;
                dwLevel = 1003;
            }

            if (!strcmp(field, "COMMENT")) {
                swprintf(uiTemp->usri2_comment, L"%S", newvalue);
            }

            if (!strcmp(field, "NAME")) {
                swprintf(uiTemp->usri2_name, L"%S", newvalue);
            }

            if (!strcmp(field,"HOMEDIR")) {
                swprintf(uiTemp->usri2_home_dir, L"%S", newvalue);
            }

            if (!strcmp(field,"PASSWD_NEVER_EXPIRES")) {
                if (!strcmp(newvalue, "YES")) {
                    uiTemp->usri2_flags |= UF_DONT_EXPIRE_PASSWD;
                }
                else {
                    uiTemp->usri2_flags |=~ UF_DONT_EXPIRE_PASSWD;
                }
            }

            // JPR 123007: Different for changing a user password
            if(dwLevel == 1003) {
                nStatus = NetUserSetInfo(NULL,
                                        u_userid,
                                        dwLevel,
                                        (LPBYTE)&ui1003,
                                        NULL);
            }
            // JPR 020108: Different for changing a user fullname
            else if(dwLevel == 1011) {
                nStatus = NetUserSetInfo(NULL,
                                        u_userid,
                                        dwLevel,
                                        (LPBYTE)&ui1011,
                                        NULL);
            }
            else {
                nStatus = NetUserSetInfo(NULL,
                                        u_userid,
                                        dwLevel,
                                        (LPBYTE)uiTemp,
                                        NULL);
            }

            if (nStatus != NERR_Success) {
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            pushstring("OK");
        }
        __finally {
            if (ui) {
                NetApiBufferFree(ui);
                ui = NULL; // just in case
            }
        }
    }
}


// JPR 123007: Added ChangeUserPassword function
NSISFunction(ChangeUserPassword)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;

        // buffers at the end
        char userid[256] = {0};
        char oldpwd[256] = {0};
        char newpwd[256] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_oldpwd[256] = {0};
        WCHAR u_newpwd[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(oldpwd);
        swprintf(u_oldpwd, L"%S", oldpwd);

        popstring(newpwd);
        swprintf(u_newpwd, L"%S", newpwd);

        nStatus = NetUserChangePassword(NULL, u_userid, u_oldpwd, u_newpwd);
        if (nStatus != NERR_Success) {
            sprintf(userid, "ERROR %d", nStatus);
            pushstring(userid);
            return;
        }

        pushstring("OK");
    }
}

NSISFunction(DeleteGroup)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        DWORD dwError = 0;

        // buffers at the end
        char groupid[256] = {0};
        WCHAR u_groupid[256] = {0};

        g_hwndParent = hwndParent;

        popstring(groupid);
        swprintf(u_groupid, L"%S", groupid);

        nStatus = NetLocalGroupDel(NULL, u_groupid);
        if (nStatus != NERR_Success) {
            sprintf(groupid, "ERROR %d %d", nStatus, dwError);
            pushstring(groupid);
            return;
        }

        pushstring("OK");
    }
}

NSISFunction(CreateGroup)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        DWORD dwError = 0;
        LOCALGROUP_INFO_1 ginfo = LOCALGROUP_INFO_1();

        // buffers at the end
        char groupid[256] = {0};
        char comment[1024] = {0};

        WCHAR u_groupid[256] = {0};
        WCHAR u_comment[1024] = {0};

        g_hwndParent = hwndParent;

        popstring(groupid);
        popstring(comment);

        swprintf(u_groupid, L"%S", groupid);
        swprintf(u_comment, L"%S", comment);

        ginfo.lgrpi1_name = u_groupid;
        ginfo.lgrpi1_comment= u_comment;

        nStatus = NetLocalGroupAdd(NULL, 1, (LPBYTE)&ginfo, &dwError);
        if (nStatus != NERR_Success) {
            sprintf(groupid, "ERROR %d %d", nStatus, dwError);
            pushstring(groupid);
            return;
        }

        pushstring("OK");
    }
}

NSISFunction(AddToGroup)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;
        LOCALGROUP_MEMBERS_INFO_3 LMI = LOCALGROUP_MEMBERS_INFO_3();

        // buffers at the end
        char userid[256] = {0};
        char groupid[256] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_groupid[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        //swprintf(u_userid, L"%S", userid);
        MultiByteToWideChar(CP_ACP, 0, userid, -1, u_userid, strlen(userid));

        popstring(groupid);
        removeSubstringOnce(groupid, "BUILTIN\\"); // fix for UserMgr::GetUserNameFromSID where it can return prefixed group name
        //swprintf(u_groupid, L"%S", groupid);
        MultiByteToWideChar(CP_ACP, 0, groupid, -1, u_groupid, strlen(groupid));

        // JPR 123007: Changed to NetLocalGroupAddMembers to make this function work
        LMI.lgrmi3_domainandname = u_userid;
        nStatus = NetLocalGroupAddMembers(NULL, u_groupid, 3, (LPBYTE)&LMI, 1);
        if (nStatus != NERR_Success) {
            sprintf(userid, "ERROR %d", nStatus);
            pushstring(userid);
            return;
        }

        pushstring("OK");
    }
}


// JPR 011208: Added function IsMemberOfGroup
NSISFunction(IsMemberOfGroup)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
       NET_API_STATUS nStatus;
       LPLOCALGROUP_MEMBERS_INFO_1 pBuf = NULL;
       DWORD dwLevel = 1;
       DWORD dwPrefMaxLen = MAX_PREFERRED_LENGTH;
       DWORD dwEntriesRead = 0;
       DWORD dwTotalEntries = 0;
       DWORD dwResumeHandle = 0;

       // buffers at the end
       char userid[256] = {0};
       char userid2[256] = {0};
       char groupid[256] = {0};
       char groupid2[256] = {0};

       WCHAR u_groupid[256] = {0};

       g_hwndParent = hwndParent;

       popstring(userid);
       popstring(groupid);

       swprintf(u_groupid, L"%S", groupid);

       __try {
            nStatus = NetLocalGroupGetMembers(NULL,
                                                u_groupid,
                                                dwLevel,
                                                (LPBYTE *)&pBuf,
                                                dwPrefMaxLen,
                                                &dwEntriesRead,
                                                &dwTotalEntries,
                                                &dwResumeHandle);
            if (nStatus != NERR_Success)
            {
                sprintf(userid, "ERROR %d", nStatus);
                pushstring(userid);
                return;
            }

            LPLOCALGROUP_MEMBERS_INFO_1 pTmpBuf = NULL;
            DWORD i;
            DWORD dwTotalCount = 0;

            if (pTmpBuf = pBuf) {
                //
                // Loop through the entries and 
                //  print the names of the local groups 
                //  to which the user belongs. 
                //
                for (i = 0; i < dwEntriesRead; i++)
                {
                    if (!pTmpBuf) {
                        sprintf(userid, "ERROR: An access violation has occurred");
                        pushstring(userid);
                        return;
                    }

                    sprintf(userid2, "%S", pTmpBuf->lgrmi1_name);
                    if(!strcmp(userid2, userid))
                    {
                        pushstring("TRUE");
                        return;
                    }

                    pTmpBuf++;
                    dwTotalCount++;
                }
            }

            pushstring("FALSE");
       }
       __finally {
           if (pBuf) {
               NetApiBufferFree(pBuf);
               pBuf = NULL; // just in case
           }
       }
    }
}


NSISFunction(RemoveFromGroup)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NET_API_STATUS nStatus;

        // buffers at the end
        char userid[256] = {0};
        char groupid[256] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_groupid[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(groupid);
        swprintf(u_groupid, L"%S", groupid);

        nStatus = NetGroupDelUser(NULL, u_groupid, u_userid);
        if (nStatus != NERR_Success) {
            sprintf(userid, "ERROR %d", nStatus);
            pushstring(userid);
            return;
        }

        pushstring("OK");
    }
}

NSISFunction(AddPrivilege)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        DWORD dwLevel = 1;
        DWORD dwError = 0;
        PSID user_sid = NULL;
        LSA_HANDLE my_policy_handle = NULL;
        LSA_UNICODE_STRING lucPrivilege = LSA_UNICODE_STRING();

        // buffers at the end
        char tempbuf[1024] = {0};
        char userid[256] = {0};
        char privilege[256] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_privilege[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(privilege);
        swprintf(u_privilege, L"%S", privilege);

        __try {
            if (!GetAccountSid(NULL, userid, &user_sid)) {
                pushstring("ERROR GetAccountSid");
                return;
            }

            my_policy_handle = GetPolicyHandle();
            if (!my_policy_handle) {
                pushstring("ERROR GetPolicyHandle");
                return;
            }

            if (!InitLsaString(&lucPrivilege, u_privilege)) {
                pushstring("ERROR InitLsaString");
                return;
            }

            if (AddPrivileges(user_sid, my_policy_handle, lucPrivilege) != STATUS_SUCCESS) {
                pushstring("ERROR AddPrivileges");
                return;
            }

            pushstring("OK");
        }
        __finally {
            if (my_policy_handle) {
                LsaClose(my_policy_handle);
                my_policy_handle = NULL;
            }
        }
    }
}

NSISFunction(SetRegKeyAccess)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        unsigned int i = 0;
        ACCESS_MODE grant_or_revoke = GRANT_ACCESS;
        DWORD dwLevel = 1;
        DWORD dwError = 0;
        DWORD dwRes = 0;
        PSID user_sid = NULL;
        PACL pDacl = NULL;
        PACL pNewDacl = NULL;
        EXPLICIT_ACCESS ea = EXPLICIT_ACCESS();
        PSECURITY_DESCRIPTOR pSD = NULL;
        unsigned long accessrights = 0;
        unsigned long aclentries = 64;

        // buffers at the end
        char tempbuf[1024] = {0};
        char userid[256] = {0};
        char hive[128] = {0};
        char regkey[512] = {0};
        char rights[8] = {0};
        char myhive[32] = {0};
        char myregkey[512] = {0};

        WCHAR u_userid[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(hive);
        popstring(regkey);
        popstring(rights);

        strcpy(myhive, "");

        if (!strcmp(hive,"HKLM")) strcpy(myhive,"MACHINE");
        if (!strcmp(hive,"HKCU")) strcpy(myhive,"CURRENT_USER");
        if (!strcmp(hive,"HKU")) strcpy(myhive,"USERS");
        if (!strcmp(hive,"HKCR")) strcpy(myhive,"CLASSES_ROOT");
        if (!strcmp (myhive, "")) {
            pushstring("ERROR Illegal Root Key (use HKLM|HKCU|HKU|HKCR)");
            return;
        }

        _snprintf(myregkey, sizeof(myregkey)-1, "%s\\%s", myhive, regkey);
        if (strlen(rights) <= 0) {
            grant_or_revoke = REVOKE_ACCESS;
        }

        if (!GetAccountSid(NULL, userid, &user_sid)) {
            pushstring("ERROR GetAccountSid");
            return;
        }

        if(dwRes=GetNamedSecurityInfo(myregkey, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION,
                                    NULL, NULL, &pDacl, NULL, &pSD)!=ERROR_SUCCESS) {
            sprintf(tempbuf,"ERROR GetSecurityInfo %d", dwRes);
            pushstring(tempbuf);
            return;
        }

        for (i = 0; i <= strlen(rights); i++) {
            switch(rights[i]) {
                case '+': grant_or_revoke = GRANT_ACCESS; break;
                case '-': grant_or_revoke = DENY_ACCESS; break;
                case '=': grant_or_revoke = SET_ACCESS; break;
                case 'r': accessrights |= KEY_READ; break;
                case 'w': accessrights |= KEY_WRITE; break;
                case 'a': accessrights |= KEY_ALL_ACCESS; break;
                case 'x': accessrights |= KEY_EXECUTE; break;
                default: break;
            }
        }

        ea.grfAccessPermissions = accessrights;
        ea.grfAccessMode = grant_or_revoke;
        ea.grfInheritance= SUB_CONTAINERS_ONLY_INHERIT;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
        ea.Trustee.ptstrName = (LPSTR)user_sid;

        if(dwRes = SetEntriesInAcl(1, &ea, pDacl, &pNewDacl) != ERROR_SUCCESS) {
            sprintf(tempbuf, "ERROR SetEntriesInAcl Error %d", dwRes);
            pushstring(tempbuf);
            return;
        }

        if ((dwRes = SetNamedSecurityInfo(myregkey, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, NULL, NULL, pNewDacl, NULL)) != ERROR_SUCCESS) {
            sprintf(tempbuf, "ERROR SetNamedSecurityInfo %d", dwRes);
            pushstring( tempbuf);
            return;
        }

        sprintf(tempbuf, "OK");
        pushstring(tempbuf);
    }
}

NSISFunction(RemovePrivilege)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        DWORD dwLevel = 1;
        DWORD dwError = 0;
        PSID user_sid = NULL;
        LSA_HANDLE my_policy_handle = NULL;
        LSA_UNICODE_STRING lucPrivilege = LSA_UNICODE_STRING();

        // buffers at the end
        char tempbuf[1024] = {0};
        char userid[256] = {0};
        char privilege[256] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_privilege[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(privilege);
        swprintf(u_privilege, L"%S", privilege);

        __try {
            if (!GetAccountSid(NULL, userid, &user_sid)) {
                pushstring("ERROR GetAccountSid");
                return;
            }

            my_policy_handle = GetPolicyHandle();
            if (!my_policy_handle) {
                pushstring("ERROR GetPolicyHandle");
                return;
            }

            if (!InitLsaString(&lucPrivilege, u_privilege)) {
                pushstring("ERROR InitLsaString");
                return;
            }

            if (RemovePrivileges(user_sid, my_policy_handle, lucPrivilege) != STATUS_SUCCESS) {
                pushstring("ERROR RemovePrivileges");
                return;
            }

            pushstring("OK");
        }
        __finally {
            if (my_policy_handle) {
                LsaClose(my_policy_handle);
                my_policy_handle = NULL; // just in case
            }
        }
    }
}


// JPR 020108: Added function HasPrivilege
NSISFunction(HasPrivilege)
{
    EXDLL_INIT();
    extra->RegisterPluginCallback(g_hInstance, PluginCallback);
    {
        NTSTATUS ntStatus;
        DWORD dwLevel = 1;
        DWORD dwError = 0;
        PSID user_sid = NULL;
        LSA_HANDLE my_policy_handle = NULL;
        LSA_UNICODE_STRING *lucPrivilege = NULL;
        LSA_UNICODE_STRING *pTmpBuf = NULL;
        ULONG count = 0;
        DWORD i = 0;

        // buffers at the end
        char tempbuf[1024] = {0};
        char userid[256] = {0};
        char privilege[256] = {0};
        char privilege2[256] = {0};

        WCHAR u_userid[256] = {0};
        WCHAR u_privilege[256] = {0};

        g_hwndParent = hwndParent;

        popstring(userid);
        swprintf(u_userid, L"%S", userid);

        popstring(privilege);
        swprintf(u_privilege, L"%S", privilege);

        __try {
            if (EnablePrivilege(SE_RESTORE_NAME)) {
                pushstring("ERROR EnablePrivilege");
                return;
            }

            if (!GetAccountSid(NULL, userid, &user_sid)) {
                pushstring("ERROR GetAccountSid");
                return;
            }

            my_policy_handle = GetPolicyHandle();
            if (!my_policy_handle) {
                pushstring("ERROR GetPolicyHandle");
                return;
            }

            if ((ntStatus = LsaEnumerateAccountRights(my_policy_handle, user_sid, (LSA_UNICODE_STRING **)&lucPrivilege, &count)) != STATUS_SUCCESS) {
                dwError = LsaNtStatusToWinError(ntStatus);
                if(dwError == ERROR_FILE_NOT_FOUND) {
                    sprintf(tempbuf, "FALSE");
                }
                else if(dwError == ERROR_MR_MID_NOT_FOUND) {
                    sprintf(tempbuf, "ERROR LsaEnumerateAccountRights n%ld", ntStatus);
                }
                else {
                    sprintf(tempbuf, "ERROR LsaEnumerateAccountRights w%lu", dwError);
                }

                pushstring(tempbuf);
            }

            if (pTmpBuf = lucPrivilege) {
                for (i = 0; i < count; i++) {
                    if (!pTmpBuf) {
                        sprintf(userid, "ERROR: An access violation has occurred");
                        pushstring(userid);
                        return;
                    }

                    sprintf(privilege2, "%S", pTmpBuf->Buffer);
                    if(!strcmp(privilege2,privilege)) {
                        pushstring("TRUE");
                        return;
                    }

                    pTmpBuf++;
                }
            }

            pushstring("FALSE");
        }
        __finally {
            if (lucPrivilege) {
                LsaFreeMemory(&lucPrivilege);
                lucPrivilege = NULL; // just in case
            }
            if (my_policy_handle) {
                LsaClose(my_policy_handle);
                my_policy_handle = NULL; // just in case
            }
        }
    }
}

}
