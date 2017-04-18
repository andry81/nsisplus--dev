///////////////////////////////////////////////////////////////////////////////
//
//  SSPI Authentication Sample
//
//  This program demonstrates how to use SSPI to authenticate user credentials.
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED
//  TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//  Copyright (C) 2007.  Microsoft Corporation.  All rights reserved.
///////////////////////////////////////////////////////////////////////////////

#ifndef SSPLOGON_H
#define SSPLOGON_H

#define SECURITY_WIN32
#include <windows.h>
#include <nsis_tchar.h>
#include <stdio.h>
#include <conio.h>
#include <sspi.h>
#include <lm.h>
#include <lmcons.h>
#include <string.h>

// Older versions of WinError.h do not have SEC_I_COMPLETE_NEEDED #define.
// So, in such an SDK environment setup, we will include issperr.h which has the
// definition for SEC_I_COMPLETE_NEEDED. Include issperr.h only if
// SEC_I_COMPLETE_NEEDED is not defined.
#ifndef SEC_I_COMPLETE_NEEDED
#include <issperr.h>
#endif

typedef struct _AUTH_SEQ {
   BOOL fInitialized;
   BOOL fHaveCredHandle;
   BOOL fHaveCtxtHandle;
   CredHandle hcred;
   struct _SecHandle hctxt;
} AUTH_SEQ, *PAUTH_SEQ;

extern DWORD g_SSPLastError;
extern TCHAR g_SSPLastErrorStr[256];

BOOL WINAPI _SSPLogonUser(LPTSTR szDomain, LPTSTR szUser, LPTSTR szPassword, LPTSTR sspiProviderName);

#endif
