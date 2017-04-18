// Details:
//  How to validate user credentials on Microsoft operating systems:
//  https://support.microsoft.com/en-us/kb/180548
//

//SSP particular include
//#undef _T
//#define _T(x) x
//
//#define UNICODE 1
//#define _UNICODE 1
//#undef _T
//#define _T(x) x
//
//#define _T_WCHAR(x) L ## x

#include <SSPLogon.h>

// Function pointers
ACCEPT_SECURITY_CONTEXT_FN       _AcceptSecurityContext     = NULL;
ACQUIRE_CREDENTIALS_HANDLE_FN    _AcquireCredentialsHandle  = NULL;
COMPLETE_AUTH_TOKEN_FN           _CompleteAuthToken         = NULL;
DELETE_SECURITY_CONTEXT_FN       _DeleteSecurityContext     = NULL;
FREE_CONTEXT_BUFFER_FN           _FreeContextBuffer         = NULL;
FREE_CREDENTIALS_HANDLE_FN       _FreeCredentialsHandle     = NULL;
INITIALIZE_SECURITY_CONTEXT_FN   _InitializeSecurityContext = NULL;
QUERY_SECURITY_PACKAGE_INFO_FN   _QuerySecurityPackageInfo  = NULL;
QUERY_SECURITY_CONTEXT_TOKEN_FN  _QuerySecurityContextToken = NULL;


#define CheckAndLocalFree(ptr) \
            if (ptr != NULL) \
            { \
               LocalFree(ptr); \
               ptr = NULL; \
            }

//#pragma comment(lib, "netapi32.lib")

// Details: http://stackoverflow.com/questions/7111618/win32-how-to-validate-credentials-against-active-directory
//    sspiProviderName is the same of the Security Support Provider Package to use. Some possible choices are:
//            - Negotiate (Preferred)
//                        Introduced in Windows 2000 (secur32.dll)
//                        Selects Kerberos and if not available, NTLM protocol.
//                        Negotiate SSP provides single sign-on capability called as Integrated Windows Authentication.
//                        On Windows 7 and later, NEGOExts is introduced which negotiates the use of installed
//                        custom SSPs which are supported on the client and server for authentication.
//            - Kerberos
//                        Introduced in Windows 2000 and updated in Windows Vista to support AES) (secur32.dll)
//                        Preferred for mutual client-server domain authentication in Windows 2000 and later.
//            - NTLM
//                        Introduced in Windows NT 3.51 (Msv1_0.dll)
//                        Provides NTLM challenge/response authentication for client-server domains prior to
//                        Windows 2000 and for non-domain authentication (SMB/CIFS)
//            - Digest
//                        Introduced in Windows XP (wdigest.dll)
//                        Provides challenge/response based HTTP and SASL authentication between Windows and non-Windows systems where Kerberos is not available
//            - CredSSP
//                        Introduced in Windows Vista and available on Windows XP SP3 (credssp.dll)
//                        Provides SSO and Network Level Authentication for Remote Desktop Services
//            - Schannel
//                        Introduced in Windows 2000 and updated in Windows Vista to support stronger AES encryption and ECC (schannel.dll)
//                        Microsoft's implementation of TLS/SSL
//                        Public key cryptography SSP that provides encryption and secure communication for
//                        authenticating clients and servers over the internet. Updated in Windows 7 to support TLS 1.2.
//
DWORD g_SSPLastError = 0;
TCHAR g_SSPLastErrorStr[256] = {0};

LPVOID RetrieveTokenInformationClass(
      HANDLE hToken,
      TOKEN_INFORMATION_CLASS InfoClass,
      LPDWORD lpdwSize)
{
   LPVOID pInfo = NULL;
   BOOL fSuccess = FALSE;
   DWORD last_error = 0;

   __try
   {
      *lpdwSize = 0;

      //
      // Determine the size of the buffer needed
      //

      GetTokenInformation(
            hToken,
            InfoClass,
            NULL,
            *lpdwSize, lpdwSize);
      last_error = GetLastError();
      if (last_error != ERROR_INSUFFICIENT_BUFFER)
      {
         g_SSPLastError = last_error;
         _stprintf(g_SSPLastErrorStr, _T("GetTokenInformation: %08X (%d)"), last_error, last_error);
         __leave;
      }

      //
      // Allocate a buffer for getting token information
      //
      pInfo = LocalAlloc(LPTR, *lpdwSize);
      if (pInfo == NULL)
      {
         last_error = GetLastError();
         g_SSPLastError = last_error;
         _stprintf(g_SSPLastErrorStr, _T("LocalAlloc: %08X (%d)"), last_error, last_error);
         __leave;
      }

      if (!GetTokenInformation(
            hToken,
            InfoClass,
            pInfo,
            *lpdwSize, lpdwSize))
      {
         last_error = GetLastError();
         g_SSPLastError = last_error;
         _stprintf(g_SSPLastErrorStr, _T("GetTokenInformation: %08X (%d)"), last_error, last_error);
         __leave;
      }

      fSuccess = TRUE;
   }
   __finally
   {
      // Free pDomainAndUserName only if failed
      // Otherwise, the caller has to free after use
      if (fSuccess == FALSE)
      {
         CheckAndLocalFree(pInfo);
      }
   }

   return pInfo;
}

PSID GetUserSidFromWellKnownRid(DWORD Rid)
{
    PUSER_MODALS_INFO_2 umi2;
    NET_API_STATUS nas;

    UCHAR SubAuthorityCount;
    DWORD last_error;

    PSID pSid = NULL;

    BOOL bSuccess = FALSE; // assume failure

    nas = NetUserModalsGet(NULL, 2, (LPBYTE *)&umi2);

    if (nas != NERR_Success)
    {
        last_error = nas;
        g_SSPLastError = last_error;
        _stprintf(g_SSPLastErrorStr, _T("NetUserModalsGet: %08X (%d)"), last_error, last_error);
        SetLastError(last_error);
        return NULL;
    }

    SubAuthorityCount = *GetSidSubAuthorityCount
                       (umi2->usrmod2_domain_id);

    // 
    // Allocate storage for new Sid. account domain Sid + account Rid
    // 

    pSid = (PSID)LocalAlloc(LPTR,
          GetSidLengthRequired((UCHAR)(SubAuthorityCount + 1)));

    if (pSid != NULL)
    {
        if (InitializeSid(
              pSid,
              GetSidIdentifierAuthority(umi2->usrmod2_domain_id),
              (BYTE)(SubAuthorityCount+1)
              ))
        {
            DWORD SubAuthIndex = 0;

            // 
            // Copy existing subauthorities from account domain Sid into
            // new Sid
            // 

            for (; SubAuthIndex < SubAuthorityCount ; SubAuthIndex++)
            {
                *GetSidSubAuthority(pSid, SubAuthIndex) =
                *GetSidSubAuthority(umi2->usrmod2_domain_id,
                                  SubAuthIndex);
            }

            // 
            // Append Rid to new Sid
            // 

            *GetSidSubAuthority(pSid, SubAuthorityCount) = Rid;
        }
    }

    NetApiBufferFree(umi2);

    return pSid;
}

BOOL IsGuest(HANDLE hToken)
{
    BOOL fGuest = FALSE;
    PSID pGuestSid = NULL;
    PSID pUserSid = NULL;
    TOKEN_USER *pUserInfo = NULL;
    DWORD dwSize = 0;

    pGuestSid = GetUserSidFromWellKnownRid(DOMAIN_USER_RID_GUEST);
    if (pGuestSid == NULL)
        return fGuest;

    //
    // Get user information
    //

    pUserInfo = (TOKEN_USER *)RetrieveTokenInformationClass(hToken, TokenUser, &dwSize);
    if (pUserInfo != NULL)
    {
        if (EqualSid(pGuestSid, pUserInfo->User.Sid))
            fGuest = TRUE;
    }

    CheckAndLocalFree(pUserInfo);
    CheckAndLocalFree(pGuestSid);

    return fGuest;
}

///////////////////////////////////////////////////////////////////////////////


void UnloadSecurityDll(HMODULE hModule) {

   if (hModule)
      FreeLibrary(hModule);

   _AcceptSecurityContext      = NULL;
   _AcquireCredentialsHandle   = NULL;
   _CompleteAuthToken          = NULL;
   _DeleteSecurityContext      = NULL;
   _FreeContextBuffer          = NULL;
   _FreeCredentialsHandle      = NULL;
   _InitializeSecurityContext  = NULL;
   _QuerySecurityPackageInfo   = NULL;
   _QuerySecurityContextToken  = NULL;
}


///////////////////////////////////////////////////////////////////////////////


HMODULE LoadSecurityDll() {

   HMODULE hModule;
   BOOL    fAllFunctionsLoaded = FALSE;
   TCHAR   lpszDLL[MAX_PATH];
   OSVERSIONINFO VerInfo;

   //
   //  Find out which security DLL to use, depending on
   //  whether we are on Windows NT or Windows 95, Windows 2000, Windows XP, or Windows Server 2003
   //  We have to use security.dll on Windows NT 4.0.
   //  All other operating systems, we have to use Secur32.dll
   //
   VerInfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
   if (!GetVersionEx (&VerInfo))   // If this fails, something has gone wrong
   {
      return FALSE;
   }

   if (VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT &&
      VerInfo.dwMajorVersion == 4 &&
      VerInfo.dwMinorVersion == 0)
   {
       _tcscpy(lpszDLL, _T("security.dll"));
   }
   else
   {
      _tcscpy(lpszDLL, _T("secur32.dll"));
   }


   hModule = LoadLibrary(lpszDLL);
   if (!hModule)
      return NULL;

   __try {

      _AcceptSecurityContext = (ACCEPT_SECURITY_CONTEXT_FN)
            GetProcAddress(hModule, "AcceptSecurityContext");
      if (!_AcceptSecurityContext)
         __leave;

#ifdef UNICODE
      _AcquireCredentialsHandle = (ACQUIRE_CREDENTIALS_HANDLE_FN)
            GetProcAddress(hModule, "AcquireCredentialsHandleW");
#else
      _AcquireCredentialsHandle = (ACQUIRE_CREDENTIALS_HANDLE_FN)
            GetProcAddress(hModule, "AcquireCredentialsHandleA");
#endif
      if (!_AcquireCredentialsHandle)
         __leave;

      // CompleteAuthToken is not present on Windows 9x Secur32.dll
      // Do not check for the availablity of the function if it is NULL;
      _CompleteAuthToken = (COMPLETE_AUTH_TOKEN_FN)
            GetProcAddress(hModule, "CompleteAuthToken");

      _DeleteSecurityContext = (DELETE_SECURITY_CONTEXT_FN)
            GetProcAddress(hModule, "DeleteSecurityContext");
      if (!_DeleteSecurityContext)
         __leave;

      _FreeContextBuffer = (FREE_CONTEXT_BUFFER_FN)
            GetProcAddress(hModule, "FreeContextBuffer");
      if (!_FreeContextBuffer)
         __leave;

      _FreeCredentialsHandle = (FREE_CREDENTIALS_HANDLE_FN)
            GetProcAddress(hModule, "FreeCredentialsHandle");
      if (!_FreeCredentialsHandle)
         __leave;

#ifdef UNICODE
      _InitializeSecurityContext = (INITIALIZE_SECURITY_CONTEXT_FN)
            GetProcAddress(hModule, "InitializeSecurityContextW");
#else
      _InitializeSecurityContext = (INITIALIZE_SECURITY_CONTEXT_FN)
            GetProcAddress(hModule, "InitializeSecurityContextA");
#endif
      if (!_InitializeSecurityContext)
         __leave;

#ifdef UNICODE
      _QuerySecurityPackageInfo = (QUERY_SECURITY_PACKAGE_INFO_FN)
            GetProcAddress(hModule, "QuerySecurityPackageInfoW");
#else
      _QuerySecurityPackageInfo = (QUERY_SECURITY_PACKAGE_INFO_FN)
            GetProcAddress(hModule, "QuerySecurityPackageInfoA");
#endif
      if (!_QuerySecurityPackageInfo)
         __leave;


      _QuerySecurityContextToken = (QUERY_SECURITY_CONTEXT_TOKEN_FN)
            GetProcAddress(hModule, "QuerySecurityContextToken");
      if (!_QuerySecurityContextToken)
         __leave;

      fAllFunctionsLoaded = TRUE;

   } __finally {

      if (!fAllFunctionsLoaded) {
         UnloadSecurityDll(hModule);
         hModule = NULL;
      }

   }

   return hModule;
}


///////////////////////////////////////////////////////////////////////////////


#if defined(UNICODE) || defined(_UNICODE)
BOOL GenClientContext(LPTSTR sspiProviderName, PAUTH_SEQ pAS, PSEC_WINNT_AUTH_IDENTITY_W pAuthIdentity,
      PVOID pIn, DWORD cbIn, PVOID pOut, PDWORD pcbOut, PBOOL pfDone) {
#else
BOOL GenClientContext(LPTSTR sspiProviderName, PAUTH_SEQ pAS, PSEC_WINNT_AUTH_IDENTITY_A pAuthIdentity,
      PVOID pIn, DWORD cbIn, PVOID pOut, PDWORD pcbOut, PBOOL pfDone) {
#endif

/*++

 Routine Description:

   Optionally takes an input buffer coming from the server and returns
   a buffer of information to send back to the server.  Also returns
   an indication of whether or not the context is complete.

 Return Value:

   Returns TRUE if successful; otherwise FALSE.

--*/

   SECURITY_STATUS ss;
   TimeStamp       tsExpiry;
   SecBufferDesc   sbdOut;
   SecBuffer       sbOut;
   SecBufferDesc   sbdIn;
   SecBuffer       sbIn;
   ULONG           fContextAttr;

#if defined(UNICODE) || defined(_UNICODE)
   WCHAR sspiProviderNameW[256];
#endif

   DWORD last_error;

   if (!pAS->fInitialized) {
#if defined(UNICODE) || defined(_UNICODE)
      MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, sspiProviderName, -1, sspiProviderNameW, sizeof(sspiProviderNameW)/sizeof(sspiProviderNameW[0]));
      ss = _AcquireCredentialsHandle(NULL, sspiProviderNameW,
#else
      ss = _AcquireCredentialsHandle(NULL, sspiProviderName,
#endif
            SECPKG_CRED_OUTBOUND, NULL, pAuthIdentity, NULL, NULL,
            &pAS->hcred, &tsExpiry);
      if (ss < 0) {
         last_error = ss;
         g_SSPLastError = last_error;
         _stprintf(g_SSPLastErrorStr, _T("AcquireCredentialsHandle: %08X (%d)"), last_error, last_error);
         return FALSE;
      }

      pAS->fHaveCredHandle = TRUE;
   }

   // Prepare output buffer
   sbdOut.ulVersion = 0;
   sbdOut.cBuffers = 1;
   sbdOut.pBuffers = &sbOut;

   sbOut.cbBuffer = *pcbOut;
   sbOut.BufferType = SECBUFFER_TOKEN;
   sbOut.pvBuffer = pOut;

   // Prepare input buffer
   if (pAS->fInitialized)  {
      sbdIn.ulVersion = 0;
      sbdIn.cBuffers = 1;
      sbdIn.pBuffers = &sbIn;

      sbIn.cbBuffer = cbIn;
      sbIn.BufferType = SECBUFFER_TOKEN;
      sbIn.pvBuffer = pIn;
   }

   ss = _InitializeSecurityContext(&pAS->hcred,
         pAS->fInitialized ? &pAS->hctxt : NULL, NULL, 0, 0,
         SECURITY_NATIVE_DREP, pAS->fInitialized ? &sbdIn : NULL,
         0, &pAS->hctxt, &sbdOut, &fContextAttr, &tsExpiry);
   if (ss < 0)  {
      // <winerror.h>
      last_error = ss;
      g_SSPLastError = last_error;
      _stprintf(g_SSPLastErrorStr, _T("InitializeSecurityContext: %08X (%d)"), last_error, last_error);
      return FALSE;
   }

   pAS->fHaveCtxtHandle = TRUE;

   // If necessary, complete token
   if (ss == SEC_I_COMPLETE_NEEDED || ss == SEC_I_COMPLETE_AND_CONTINUE) {

      if (_CompleteAuthToken) {
         ss = _CompleteAuthToken(&pAS->hctxt, &sbdOut);
         if (ss < 0)  {
            last_error = ss;
            g_SSPLastError = last_error;
            _stprintf(g_SSPLastErrorStr, _T("CompleteAuthToken: %08X (%d)"), last_error, last_error);
            return FALSE;
         }
      }
      else {
         last_error = 0x20000000 | 2;
         g_SSPLastError = last_error;
         _tcscpy(g_SSPLastErrorStr, _T("CompleteAuthToken not supported"));
         return FALSE;
      }
   }

   *pcbOut = sbOut.cbBuffer;

   if (!pAS->fInitialized)
      pAS->fInitialized = TRUE;

   *pfDone = !(ss == SEC_I_CONTINUE_NEEDED
         || ss == SEC_I_COMPLETE_AND_CONTINUE );

   return TRUE;
}


///////////////////////////////////////////////////////////////////////////////


BOOL GenServerContext(LPTSTR sspiProviderName, PAUTH_SEQ pAS, PVOID pIn, DWORD cbIn, PVOID pOut,
      PDWORD pcbOut, PBOOL pfDone) {

/*++

 Routine Description:

    Takes an input buffer coming from the client and returns a buffer
    to be sent to the client.  Also returns an indication of whether or
    not the context is complete.

 Return Value:

    Returns TRUE if successful; otherwise FALSE.

--*/

   SECURITY_STATUS ss;
   TimeStamp       tsExpiry;
   SecBufferDesc   sbdOut;
   SecBuffer       sbOut;
   SecBufferDesc   sbdIn;
   SecBuffer       sbIn;
   ULONG           fContextAttr;

#if defined(UNICODE) || defined(_UNICODE)
   WCHAR sspiProviderNameW[256];
#endif

   DWORD last_error;

   if (!pAS->fInitialized)  {
#if defined(UNICODE) || defined(_UNICODE)
      MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, sspiProviderName, -1, sspiProviderNameW, sizeof(sspiProviderNameW)/sizeof(sspiProviderNameW[0]));
      ss = _AcquireCredentialsHandle(NULL, sspiProviderNameW,
#else
      ss = _AcquireCredentialsHandle(NULL, sspiProviderName,
#endif
            SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL, &pAS->hcred,
            &tsExpiry);
      if (ss < 0) {
         last_error = ss;
         g_SSPLastError = last_error;
         _stprintf(g_SSPLastErrorStr, _T("AcquireCredentialsHandle: %08X (%d)"), last_error, last_error);
         return FALSE;
      }

      pAS->fHaveCredHandle = TRUE;
   }

   // Prepare output buffer
   sbdOut.ulVersion = 0;
   sbdOut.cBuffers = 1;
   sbdOut.pBuffers = &sbOut;

   sbOut.cbBuffer = *pcbOut;
   sbOut.BufferType = SECBUFFER_TOKEN;
   sbOut.pvBuffer = pOut;

   // Prepare input buffer
   sbdIn.ulVersion = 0;
   sbdIn.cBuffers = 1;
   sbdIn.pBuffers = &sbIn;

   sbIn.cbBuffer = cbIn;
   sbIn.BufferType = SECBUFFER_TOKEN;
   sbIn.pvBuffer = pIn;

   ss = _AcceptSecurityContext(&pAS->hcred,
         pAS->fInitialized ? &pAS->hctxt : NULL, &sbdIn, 0,
         SECURITY_NATIVE_DREP, &pAS->hctxt, &sbdOut, &fContextAttr,
         &tsExpiry);
   if (ss < 0)  {
      last_error = ss;
      g_SSPLastError = last_error;
      _stprintf(g_SSPLastErrorStr, _T("AcceptSecurityContext: %08X (%d)"), last_error, last_error);
      return FALSE;
   }

   pAS->fHaveCtxtHandle = TRUE;

   // If necessary, complete token
   if (ss == SEC_I_COMPLETE_NEEDED || ss == SEC_I_COMPLETE_AND_CONTINUE) {

      if (_CompleteAuthToken) {
         ss = _CompleteAuthToken(&pAS->hctxt, &sbdOut);
         if (ss < 0)  {
            last_error = ss;
            g_SSPLastError = last_error;
            _stprintf(g_SSPLastErrorStr, _T("CompleteAuthToken: %08X (%d)"), last_error, last_error);
            return FALSE;
         }
      }
      else {
         last_error = 0x20000000 | 3;
         g_SSPLastError = last_error;
         _tcscpy(g_SSPLastErrorStr, _T("CompleteAuthToken not supported"));
         return FALSE;
      }
   }

   *pcbOut = sbOut.cbBuffer;

   if (!pAS->fInitialized)
      pAS->fInitialized = TRUE;

   *pfDone = !(ss == SEC_I_CONTINUE_NEEDED
         || ss == SEC_I_COMPLETE_AND_CONTINUE);

   return TRUE;
}


///////////////////////////////////////////////////////////////////////////////
BOOL WINAPI _SSPLogonUser(LPTSTR szDomain, LPTSTR szUser, LPTSTR szPassword, LPTSTR sspiProviderName)
{
   AUTH_SEQ    asServer   = {0};
   AUTH_SEQ    asClient   = {0};
   BOOL        fDone      = FALSE;
   BOOL        fResult    = FALSE;
   DWORD       cbOut      = 0;
   DWORD       cbIn       = 0;
   DWORD       cbMaxToken = 0;
   PVOID       pClientBuf = NULL;
   PVOID       pServerBuf = NULL;
   PSecPkgInfo pSPI       = NULL;
   HMODULE     hModule    = NULL;

#if defined(UNICODE) || defined(_UNICODE)
   SEC_WINNT_AUTH_IDENTITY_W ai;
   WCHAR sspiProviderNameW[256];
   WCHAR DomainBuf[256];
   WCHAR UserBuf[256];
   WCHAR PassBuf[256];
#else
   SEC_WINNT_AUTH_IDENTITY_A ai;
#endif

   DWORD last_error;
   SECURITY_STATUS ss;

   // drop last error
   g_SSPLastError = 0;
   g_SSPLastErrorStr[0] = '\0';

   __try {

      hModule = LoadSecurityDll();
      if (!hModule) {
         last_error = 0x20000000 | 1;
         g_SSPLastError = last_error;
         _stprintf(g_SSPLastErrorStr, _T("LoadSecurityDll: %08X (%d)"), last_error, last_error);
         __leave;
      }

      // Get max token size
#if defined(UNICODE) || defined(_UNICODE)
      MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, sspiProviderName, -1, sspiProviderNameW, sizeof(sspiProviderNameW)/sizeof(sspiProviderNameW[0]));
      ss = _QuerySecurityPackageInfo(sspiProviderNameW, &pSPI);
#else
      ss = _QuerySecurityPackageInfo(sspiProviderName, &pSPI);
#endif
      if (ss != SEC_E_OK) {
         last_error = ss;
         g_SSPLastError = last_error;
         _stprintf(g_SSPLastErrorStr, _T("QuerySecurityPackageInfo: %08X (%d)"), last_error, last_error);
         __leave;
      }
      cbMaxToken = pSPI->cbMaxToken;
      _FreeContextBuffer(pSPI);

      // Allocate buffers for client and server messages
      pClientBuf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbMaxToken);
      pServerBuf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbMaxToken);

      // Initialize auth identity structure
      ZeroMemory(&ai, sizeof(ai));
#if defined(UNICODE) || defined(_UNICODE)
      ret_code = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szDomain, -1, DomainBuf, cbMaxToken/sizeof(WCHAR));
      C_ASSERT(sizeof(*ai.Domain) == sizeof(*DomainBuf));
      ai.Domain = (unsigned short *)DomainBuf;
      last_error = GetLastError();
      ai.DomainLength = lstrlen(szDomain);
      MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szUser, -1, UserBuf, cbMaxToken/sizeof(WCHAR));
      C_ASSERT(sizeof(*ai.User) == sizeof(*UserBuf));
      ai.User = (unsigned short *)UserBuf;
      ai.UserLength = lstrlen(szUser);
      MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szPassword, -1, PassBuf, cbMaxToken/sizeof(WCHAR));
      C_ASSERT(sizeof(*ai.Password ) == sizeof(*PassBuf));
      ai.Password = (unsigned short *)PassBuf;
      ai.PasswordLength = lstrlen(szPassword);
      ai.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
#else
      ai.Domain = (unsigned char *)szDomain;
      ai.DomainLength = lstrlen(szDomain);
      ai.User = (unsigned char *)szUser;
      ai.UserLength = lstrlen(szUser);
      ai.Password = (unsigned char *)szPassword;
      ai.PasswordLength = lstrlen(szPassword);
      ai.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
#endif

      // Prepare client message (negotiate) .
      cbOut = cbMaxToken;
      if (!GenClientContext(sspiProviderName, &asClient, &ai, NULL, 0, pClientBuf, &cbOut, &fDone))
         __leave;

      // Prepare server message (challenge) .
      cbIn = cbOut;
      cbOut = cbMaxToken;
      if (!GenServerContext(sspiProviderName, &asServer, pClientBuf, cbIn, pServerBuf, &cbOut, &fDone))
         __leave;
         // Most likely failure: AcceptServerContext fails with SEC_E_LOGON_DENIED
         // in the case of bad szUser or szPassword.
         // Unexpected Result: Logon will succeed if you pass in a bad szUser and
         // the guest account is enabled in the specified domain.

      // Prepare client message (authenticate) .
      cbIn = cbOut;
      cbOut = cbMaxToken;
      if (!GenClientContext(sspiProviderName, &asClient, &ai, pServerBuf, cbIn, pClientBuf, &cbOut, &fDone))
         __leave;

      // Prepare server message (authentication) .
      cbIn = cbOut;
      cbOut = cbMaxToken;
      if (!GenServerContext(sspiProviderName, &asServer, pClientBuf, cbIn, pServerBuf, &cbOut, &fDone))
         __leave;

      {
         HANDLE hToken = NULL;

         if (_QuerySecurityContextToken(&asServer.hctxt, &hToken) == 0)
         {
            if (IsGuest(hToken))
            {
               last_error = 0x20000000 | 4;
               g_SSPLastError = last_error;
               _tcscpy(g_SSPLastErrorStr, _T("Logged in as Guest"));
               fResult = FALSE;
            }
            //else
            //{
            //   printf("Logged in as the desired user\n");
            //}
            CloseHandle(hToken);
         }
      }


   } __finally {

      // Clean up resources
      if (asClient.fHaveCtxtHandle)
         _DeleteSecurityContext(&asClient.hctxt);

      if (asClient.fHaveCredHandle)
         _FreeCredentialsHandle(&asClient.hcred);

      if (asServer.fHaveCtxtHandle)
         _DeleteSecurityContext(&asServer.hctxt);

      if (asServer.fHaveCredHandle)
         _FreeCredentialsHandle(&asServer.hcred);

      if (hModule)
         UnloadSecurityDll(hModule);

      HeapFree(GetProcessHeap(), 0, pClientBuf);
      HeapFree(GetProcessHeap(), 0, pServerBuf);
   }

   return fResult;
}

//--------------------------------------------------------------------
// The GetConsoleInput function gets an array of characters from the 
// keyboard, while printing only asterisks to the screen.

//void GetConsoleInput(TCHAR* strInput, int intMaxChars)
//{
//	char ch;
//	char minChar = ' ';
//	minChar++;
//
//	ch = getch();
//	while (ch != '\r')
//	{
//		if (ch == '\b' && strlen(strInput) > 0)
//		{
//			strInput[strlen(strInput)-1]   = '\0';
//			printf("\b \b");
//		}
//		else if (ch >= minChar && (int)strlen(strInput) < intMaxChars)
//		{
//			strInput[strlen(strInput)+1] = '\0';
//			strInput[strlen(strInput)]   = ch;
//			putch('*');
//		}
//		ch = getch();
//	}
//	putch('\n');
//}
//
//void _tmain(int argc, TCHAR **argv)
//{
//	TCHAR password[PWLEN+1];
//
//   if (argc != 3) 
//	{
//		_tprintf(_T("Usage: %s DomainName UserName\n"), argv[0]);
//		return;
//	}
//
//	_tprintf(_T("Enter password for the specified user : "));
//	password[0] = 0;
//	GetConsoleInput(password, PWLEN);
//	_tprintf(_T("\n"));
//   // argv[1] - Domain Name
//   // argv[2] - User Name
//   if (_SSPLogonUser(argv[1], argv[2], password))
//   {
//      _tprintf(_T("User Credentials are valid\n"));
//   }
//   else
//      _tprintf(_T("User Credentials are NOT valid\n"));
//}
