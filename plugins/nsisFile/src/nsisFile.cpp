/*
nsisFile -- NSIS plugin for file content manipulation
Web site: http://wiz0u.free.fr/prog/nsisFile

Copyright (c) 2010 Olivier Marcoux
Copyright (c) 2017 Andrey Dibrov (andry at inbox dot ru)

This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.

    3. This notice may not be removed or altered from any source distribution.
*/

#include "nsisFile.h"
#include <shlwapi.h>


HINSTANCE g_hInstance = HINSTANCE();
HWND g_hwndParent = HWND();


namespace {

UINT_PTR PluginCallback(enum NSPIM msg)
{
	return 0;
}

DWORD ConvertHexToBin(LPTSTR hex, BYTE** bin)
{
	*bin = new BYTE[lstrlen(hex) / 2 + 1];
	LPBYTE scanBin = *bin;
	LPTSTR scanHex = hex;
	TCHAR ch;
	while ((ch = *scanHex++) != _T('\0'))
	{
		BYTE byte;
		if ((ch >= '0') && (ch <= '9'))
			byte = ch-'0';
		else if ((ch >= 'A') && (ch <= 'F'))
			byte = ch-'A'+10;
		else if ((ch >= 'a') && (ch <= 'f'))
			byte = ch-'a'+10;
		else
			continue;
		byte *= 16;
		ch = *scanHex++;
		if ((ch >= '0') && (ch <= '9'))
			byte |= ch-'0';
		else if ((ch >= 'A') && (ch <= 'F'))
			byte |= ch-'A'+10;
		else if ((ch >= 'a') && (ch <= 'f'))
			byte |= ch-'a'+10;
		else if (ch == _T('\0'))
			break;
		else
			continue;
		*scanBin++ = byte;
	}
	return scanBin-*bin;
}

}


BOOL WINAPI DllMain(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
    g_hInstance = (HINSTANCE)hInst;
    return TRUE;
}


extern "C" {

NSISFunction(FileReadBytes)
{
	EXDLL_INIT();
	extra->RegisterPluginCallback(g_hInstance, PluginCallback);
	{
		DWORD status;
		HANDLE hFile = NULL;
		DWORD bytes = 0;
		LPTSTR result = NULL;

		// buffers at the end
		TCHAR buf[256] = {0};

		g_hwndParent = hwndParent;

		popstring(buf);
		hFile = HANDLE(_ttoi(buf));
		popstring(buf);
		bytes = _ttoi(buf);

		__try {
			result = new TCHAR[bytes*2+1];
			LPBYTE resultBytes = LPBYTE(result);

			SetLastError(0); // just in case

			if(!ReadFile(hFile, resultBytes, bytes, &bytes, NULL)) {
				status = GetLastError();
				pushstring(_T(""));
				_tprintf(buf, _T("ERROR %d"), status);
				pushstring(buf);
				return;
			}

			result[bytes*2] = _T('\0');

			while (bytes--) {
				result[bytes*2+1] = _T("0123456789ABCDEF")[resultBytes[bytes] & 15];
				result[bytes*2]   = _T("0123456789ABCDEF")[resultBytes[bytes] / 16];
			}

			pushstring(result);
			pushstring(_T("OK"));
		}
		__finally {
			delete [] result;
		}
	}
}

NSISFunction(HexToBin)
{
	EXDLL_INIT();
	extra->RegisterPluginCallback(g_hInstance, PluginCallback);
	{
		LPTSTR hex = NULL;
		BYTE* bin = NULL;

		g_hwndParent = hwndParent;

		__try {
			hex = new TCHAR[string_size];
			popstring(hex);
			DWORD bytes = ConvertHexToBin(hex, &bin);
			bin[bytes] = '\0';
#ifdef _UNICODE
			MultiByteToWideChar(CP_ACP, 0, LPCSTR(bin), bytes+1, hex, string_size);
			pushstring(hex);
#else
			pushstring(LPCSTR(bin));
#endif
		}
		__finally {
			delete [] hex;
			delete [] bin;
		}
	}
}

NSISFunction(BinToHex)
{
	EXDLL_INIT();
	extra->RegisterPluginCallback(g_hInstance, PluginCallback);
	{
		LPTSTR bin = NULL;
		DWORD count;
		LPTSTR result = NULL;

		g_hwndParent = hwndParent;

		__try {
			bin = new TCHAR[string_size];
			popstring(bin);
			count = lstrlen(bin);

			result = new TCHAR[count*2+1];
			result[count*2] = _T('\0');

			while (count--) {
				result[count*2+1] = _T("0123456789ABCDEF")[bin[count] & 15];
				result[count*2]   = _T("0123456789ABCDEF")[(bin[count] / 16) & 15];
			}

			pushstring(result);
		}
		__finally {
			delete [] result;
			delete [] bin;
		}
	}
}

NSISFunction(FileWriteBytes)
{
	EXDLL_INIT();
	extra->RegisterPluginCallback(g_hInstance, PluginCallback);
	{
		DWORD status;
		HANDLE hFile = NULL;
		LPTSTR hex = NULL;
		BYTE* bin = NULL;
		DWORD bytes;

		// buffers at the end
		TCHAR buf[256] = {0};

		g_hwndParent = hwndParent;

		popstring(buf);
		hFile = HANDLE(_ttoi(buf));
		hex = new TCHAR[string_size];
		popstring(hex);

		__try {
			bytes = ConvertHexToBin(hex, &bin);

			SetLastError(0); // just in case

			if(!WriteFile(hFile, bin, bytes, &bytes, NULL)) {
				status = GetLastError();
				pushstring(_T(""));
				_tprintf(buf, _T("ERROR %d"), status);
				pushstring(buf);
				return;
			}

			_itot(bytes, buf, 10);
			pushstring(buf);
			pushstring(_T("OK"));
		}
		__finally {
			delete [] hex;
			delete [] bin;
		}
	}
}

NSISFunction(FileFindBytes)
{
	EXDLL_INIT();
	extra->RegisterPluginCallback(g_hInstance, PluginCallback);
	{
		DWORD status;
		HANDLE hFile = NULL;
		LPTSTR hex = NULL;
		BYTE* bin = NULL;
		DWORD binlen;
		LPSTR buffer = NULL;
		DWORD buflen;

		// buffers at the end
		TCHAR buf[256] = {0};

		g_hwndParent = hwndParent;

		__try {
			popstring(buf);
			hFile = HANDLE(_ttoi(buf));
			hex = new TCHAR[string_size];
			popstring(hex);
			binlen = ConvertHexToBin(hex, &bin);
			popstring(buf);
			DWORD maxlen = _ttoi(buf);
			buflen = max(binlen*2, 16384);
			buffer = new char[buflen];
			DWORD found = -1;
			DWORD read;
			do
			{
				read = 0;

				SetLastError(0); // just in case

				if(!ReadFile(hFile, buffer, min(maxlen, buflen), &read, NULL)) {
					status = GetLastError();
					pushstring(_T("-1"));
					_tprintf(buf, _T("ERROR %d"), status);
					pushstring(buf);
					return;
				}

				maxlen -= read;
				LPCSTR scan = buffer;
				while ((scan = (LPCSTR) memchr(scan, bin[0], buffer+read-scan)) != NULL) {
					DWORD left = min(binlen, DWORD(buffer+read-scan));
					if (memcmp(++scan, bin+1, left-1) == 0) {
						if (binlen <= left) {
							SetLastError(0); // just in case

							found = SetFilePointer(hFile, scan-1-buffer-read, NULL, FILE_CURRENT);
							if (found == INVALID_SET_FILE_POINTER) {
								status = GetLastError();
								pushstring(_T("-1"));
								_tprintf(buf, _T("ERROR %d"), status);
								pushstring(buf);
								return;
							}

							break;
						}
						else { // we stopped the comparison in the middle of our pattern
							memmove(buffer, scan, left-1); // shift buffer to start (+1) of possible match
							scan = buffer;
							read = 0;

							SetLastError(0); // just in case

							if(!ReadFile(hFile, buffer+left-1, min(maxlen, buflen-left+1), &read, NULL)) { // fill more of buffer from file
								status = GetLastError();
								pushstring(_T("-1"));
								_tprintf(buf, _T("ERROR %d"), status);
								pushstring(buf);
								return;
							}

							maxlen -= read;
							read += left-1;
							if ((read+1 >= binlen) && (memcmp(buffer+left-1, bin+left, binlen-left) == 0)) {
								SetLastError(0); // just in case

								found = SetFilePointer(hFile, 0-read-1, NULL, FILE_CURRENT);
								if (found == INVALID_SET_FILE_POINTER) {
									status = GetLastError();
									pushstring(_T("-1"));
									_tprintf(buf, _T("ERROR %d"), status);
									pushstring(buf);
									return;
								}

								break;
							}
						}
					}
				}
			} while ((found == -1) && (read != 0));

			_itot(found, buf, 10);
			pushstring(buf);
			pushstring(_T("OK"));
		}
		__finally {
			delete [] buffer;
			delete [] bin;
			delete [] hex;
		}
	}
}

NSISFunction(FileTruncate)
{
	EXDLL_INIT();
	extra->RegisterPluginCallback(g_hInstance, PluginCallback);
	{
		DWORD status;
		HANDLE hFile = NULL;

		// buffers at the end
		TCHAR buf[256] = {0};

		g_hwndParent = hwndParent;

		popstring(buf);
		hFile = HANDLE(_ttoi(buf));

		SetLastError(0); // just in case

		if(!SetEndOfFile(hFile)) {
			status = GetLastError();
			_tprintf(buf, _T("ERROR %d"), status);
			pushstring(buf);
			return;
		}

		pushstring(_T("OK"));
	}
}

}
