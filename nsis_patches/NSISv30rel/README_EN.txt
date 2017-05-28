* README_EN.txt
* 2017.04.18
* NSISv30rel patches

1. DESCRIPTION
2. AUTHOR EMAIL

-------------------------------------------------------------------------------
1. DESCRIPTION
-------------------------------------------------------------------------------
This is the patched sources for the NSIS 3.0.

Main list of changes:
* disabled MakeLangId build because of an error:
  sourceforge.net/p/nsis/bugs/1159
* call MUI_LANGDLL_DISPLAY_ABORT macro before the raw Abort call to call
  a custom user abort
* increased maximal length of global variables from 60 to 128
* minor fixes

-------------------------------------------------------------------------------
2. AUTHOR EMAIL
-------------------------------------------------------------------------------
Andrey Dibrov (andry at inbox dot ru)
