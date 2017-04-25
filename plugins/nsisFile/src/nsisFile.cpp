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
#include <assert.h>


#define if_break(x) if(!(x)); else switch(0) case 0: default:

// defines for internal tests
#define COPY_MOVE_STACK_BLOCK_SIZE 64 * 1024
#define COPY_MOVE_HEAP_BLOCK_SIZE0 1 * 1024 * 1024
#define COPY_MOVE_HEAP_BLOCK_SIZE1 10 * 1024 * 1024


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

bool IsSetFilePointerError(DWORD offset, DWORD & status)
{
	status = 0; // just in case

	if (offset == INVALID_SET_FILE_POINTER) {
		status = GetLastError();
		// MSDN:
		//  Note  Because INVALID_SET_FILE_POINTER is a valid value for the low-order DWORD of the new file pointer,
		//  you must check both the return value of the function and the error code returned by GetLastError to determine
		//  whether or not an error has occurred. If an error has occurred, the return value of SetFilePointer is
		//  INVALID_SET_FILE_POINTER and GetLastError returns a value other than NO_ERROR.
		if (status != NO_ERROR) {
			return true;
		}
	}

	return false;
}

bool IsWriteFileError(DWORD & status)
{
	status = GetLastError();
	// MSDN:
	//  Note  The GetLastError code ERROR_IO_PENDING is not a failure; it designates the write operation is pending completion asynchronously.
	if (status != ERROR_IO_PENDING) {
		return true;
	}

	return false;
}

bool IsReadFileError(DWORD & status)
{
	status = GetLastError();
	// MSDN:
	//  Note  The GetLastError code ERROR_IO_PENDING is not a failure; it designates the read operation is pending completion asynchronously.
	if (status != ERROR_IO_PENDING) {
		return true;
	}

	return false;
}

DWORD MoveFileContent(HANDLE hFile, DWORD fromOffset, DWORD toOffset, DWORD moveDistance, BYTE * memBuf, size_t bufSize)
{
	DWORD status = 0;
	DWORD offset;
	DWORD bytes;

	assert(moveDistance);
	assert(bufSize);

	DWORD moveOffset = toOffset - fromOffset;
	assert(moveOffset);

	const DWORD blocksQuotient = moveDistance / bufSize;
	const DWORD blockRemainder = moveDistance % bufSize;

	DWORD copyOffset = moveDistance > bufSize ? fromOffset + moveDistance - blockRemainder : fromOffset;

	SetLastError(0); // just in case

	offset = SetFilePointer(hFile, copyOffset, NULL, FILE_BEGIN);
	if (IsSetFilePointerError(offset, status)) return status;

	SetLastError(0); // just in case

	if(!ReadFile(hFile, memBuf, blockRemainder, &bytes, NULL)) {
		if(IsReadFileError(status)) return status;
	}

	SetLastError(0); // just in case

	offset = SetFilePointer(hFile, copyOffset + moveOffset, NULL, FILE_BEGIN);
	if (IsSetFilePointerError(offset, status)) return status;

	SetLastError(0); // just in case

	if(!WriteFile(hFile, memBuf, bytes, &bytes, NULL)) {
		if (IsWriteFileError(status)) return status;
	}

	for (DWORD i = 0; i < blocksQuotient; i++) {
		copyOffset = fromOffset + (blocksQuotient - i - 1) * bufSize;

		SetLastError(0); // just in case

		offset = SetFilePointer(hFile, copyOffset, NULL, FILE_BEGIN);
		if (IsSetFilePointerError(offset, status)) return status;

		SetLastError(0); // just in case

		if(!ReadFile(hFile, memBuf, bufSize, &bytes, NULL)) {
			if(IsReadFileError(status)) return status;
		}

		SetLastError(0); // just in case

		offset = SetFilePointer(hFile, copyOffset + moveOffset, NULL, FILE_BEGIN);
		if (IsSetFilePointerError(offset, status)) return status;

		SetLastError(0); // just in case

		if(!WriteFile(hFile, memBuf, bytes, &bytes, NULL)) {
			if (IsWriteFileError(status)) return status;
		}
	}

	return status;
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
		LPTSTR result = NULL;
		DWORD bytes = 0;

		// buffers at the end
		TCHAR buf[256] = {0};

		g_hwndParent = hwndParent;

		popstring(buf);
		hFile = HANDLE(_ttoi(buf));
		popstring(buf);
		bytes = _ttoi(buf);

		__try {
			if (!bytes) {
				// nothing to read
				pushstring(_T("0"));
				pushstring(_T("OK"));
				return;
			}

			result = new TCHAR[bytes*2+1];
			LPBYTE resultBytes = LPBYTE(result);

			SetLastError(0); // just in case

			if(!ReadFile(hFile, resultBytes, bytes, &bytes, NULL)) {
				if(IsReadFileError(status)) {
					pushstring(_T("0"));
					_tprintf(buf, _T("ERROR %d"), status);
					pushstring(buf);
					return;
				}
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
		DWORD bytes = 0;

		// buffers at the end
		TCHAR buf[256] = {0};

		g_hwndParent = hwndParent;

		popstring(buf);
		hFile = HANDLE(_ttoi(buf));
		hex = new TCHAR[string_size];
		popstring(hex);

		__try {
			bytes = ConvertHexToBin(hex, &bin);
			if (!bytes) {
				// nothing to write
				pushstring(_T("0"));
				pushstring(_T("OK"));
				return;
			}

			SetLastError(0); // just in case

			if(!WriteFile(hFile, bin, bytes, &bytes, NULL)) {
				if (IsWriteFileError(status)) {
					_itot(bytes, buf, 10); // in case if not 0
					pushstring(buf);
					_tprintf(buf, _T("ERROR %d"), status);
					pushstring(buf);
					return;
				}
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

NSISFunction(FileWriteInsertBytes)
{
	EXDLL_INIT();
	extra->RegisterPluginCallback(g_hInstance, PluginCallback);
	{
		DWORD status;
		HANDLE hFile = NULL;
		LPTSTR hex = NULL;
		BYTE* bin = NULL;
		BYTE* moveBuf = NULL;
		DWORD bytes = 0;
		DWORD insertOffset;
		DWORD offset;
		DWORD endOffset;
		DWORD moveBlockSize;
		DWORD moveDistance;

		// buffers at the end
		TCHAR buf[256] = {0};

		g_hwndParent = hwndParent;

		popstring(buf);
		hFile = HANDLE(_ttoi(buf));
		hex = new TCHAR[string_size];
		popstring(hex);

		__try {
			bytes = ConvertHexToBin(hex, &bin);
			if (!bytes) {
				// nothing to write
				pushstring(_T("0"));
				pushstring(_T("OK"));
				return;
			}

			bool setError = false;
			if_break(1) {
				// get current offset position
				insertOffset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
				if (setError = IsSetFilePointerError(insertOffset, status)) break;

				// set file pointer to the end of the data before the file resize
				endOffset = SetFilePointer(hFile, 0, NULL, FILE_END);
				if (setError = IsSetFilePointerError(endOffset, status)) break;

				if (insertOffset < endOffset) {
					// resize file at first
					offset = SetFilePointer(hFile, bytes, NULL, FILE_END);
					if (setError = IsSetFilePointerError(offset, status)) break;

					// commit resize
					if (!SetEndOfFile(hFile)) {
						status = GetLastError();
						setError = true;
						break;
					}
				}
			}

			if (setError) {
				pushstring(_T("0"));
				_tprintf(buf, _T("ERROR %d"), status);
				pushstring(buf);
				return;
			}

			// move file content after insert point to the end:
			// - by 64KB block in the stack if endOffset-insertOffset <= 64KB
			// - by 64KB block in the heap if endOffset-insertOffset <= 1MB
			// - by 10MB block in the heap if endOffset-insertOffset > 1MB
			if (insertOffset < endOffset) {
				moveDistance = endOffset - insertOffset;
				if (moveDistance <= COPY_MOVE_STACK_BLOCK_SIZE) {
					BYTE moveBufStack[COPY_MOVE_STACK_BLOCK_SIZE];
					MoveFileContent(hFile, insertOffset, insertOffset + bytes, moveDistance, moveBufStack, sizeof(moveBufStack)/sizeof(moveBufStack[0]));
				}
				else {
					if (moveDistance > COPY_MOVE_HEAP_BLOCK_SIZE0) {
						moveBlockSize = COPY_MOVE_HEAP_BLOCK_SIZE1;
					}
					else {
						moveBlockSize = COPY_MOVE_HEAP_BLOCK_SIZE0;
					}
					moveBuf = new BYTE[moveBlockSize];
					MoveFileContent(hFile, insertOffset, insertOffset + bytes, moveDistance, moveBuf, moveBlockSize);
				}

				// restore file pointer
				offset = SetFilePointer(hFile, insertOffset, NULL, FILE_BEGIN);
				if (setError = IsSetFilePointerError(offset, status)) {
					pushstring(_T("0"));
					_tprintf(buf, _T("ERROR %d"), status);
					pushstring(buf);
					return;
				}
			}

			SetLastError(0); // just in case

			if(!WriteFile(hFile, bin, bytes, &bytes, NULL)) {
				if (IsWriteFileError(status)) {
					_itot(bytes, buf, 10); // in case if not 0
					pushstring(buf);
					_tprintf(buf, _T("ERROR %d"), status);
					pushstring(buf);
					return;
				}
			}

			_itot(bytes, buf, 10);
			pushstring(buf);
			pushstring(_T("OK"));
		}
		__finally {
			delete [] hex;
			delete [] bin;
			delete [] moveBuf;
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
					if(IsReadFileError(status)) {
						pushstring(_T("-1"));
						_tprintf(buf, _T("ERROR %d"), status);
						pushstring(buf);
						return;
					}
				}

				maxlen -= read;
				LPCSTR scan = buffer;
				while ((scan = (LPCSTR) memchr(scan, bin[0], buffer+read-scan)) != NULL) {
					DWORD left = min(binlen, DWORD(buffer+read-scan));
					if (memcmp(++scan, bin+1, left-1) == 0) {
						if (binlen <= left) {
							SetLastError(0); // just in case

							found = SetFilePointer(hFile, scan-1-buffer-read, NULL, FILE_CURRENT);
							if (IsSetFilePointerError(found, status)) {
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
								if(IsReadFileError(status)) {
									pushstring(_T("-1"));
									_tprintf(buf, _T("ERROR %d"), status);
									pushstring(buf);
									return;
								}
							}

							maxlen -= read;
							read += left-1;
							if ((read+1 >= binlen) && (memcmp(buffer+left-1, bin+left, binlen-left) == 0)) {
								SetLastError(0); // just in case

								found = SetFilePointer(hFile, 0-read-1, NULL, FILE_CURRENT);
								if (found == INVALID_SET_FILE_POINTER) {
									if (IsSetFilePointerError(found, status)) {
										pushstring(_T("-1"));
										_tprintf(buf, _T("ERROR %d"), status);
										pushstring(buf);
										return;
									}
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
