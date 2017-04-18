#include <windows.h>
#include "pluginapi.h"
#include <stdio.h>

#define NSISFunction(funcname) void __declspec(dllexport) funcname(HWND hwndParent, int string_size, TCHAR *variables, stack_t **stacktop, extra_parameters *extra)
