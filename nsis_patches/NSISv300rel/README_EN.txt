* README_EN.txt
* 2017.06.21
* NSISv300rel patches

1. DESCRIPTION
2. REPOSITORIES
3. KNOWN ISSUES
3.1. An error message on install scons in the Python v2.7.
4. AUTHOR

-------------------------------------------------------------------------------
1. DESCRIPTION
-------------------------------------------------------------------------------
This is the patched sources for the NSIS 3.00 (release).

The latest version is here: sf.net/p/nsisplus

WARNING:
  Use the SVN access to find out new functionality and bug fixes:
    https://svn.code.sf.net/p/nsisplus/NsisSetupDev/trunk

Main list of changes:
* disabled MakeLangId build because of an error:
  sourceforge.net/p/nsis/bugs/1159
* call MUI_LANGDLL_DISPLAY_ABORT macro before the raw Abort call to call
  a custom user abort
* increased maximal length of global variables from 60 to 128
* minor fixes

-------------------------------------------------------------------------------
2. REPOSITORIES
-------------------------------------------------------------------------------
Primary:
  * https://svn.code.sf.net/p/nsisplus/NsisSetupDev/trunk
Secondary:
  * https://github.com/andry81/nsisplus--NsisSetupDev.git

-------------------------------------------------------------------------------
3. KNOWN ISSUES
-------------------------------------------------------------------------------

-------------------------------------------------------------------------------
3.1. An error message on install scons in the Python v2.7.
-------------------------------------------------------------------------------
Solution:
  1. Try to run `pip install --egg scons` instead.

-------------------------------------------------------------------------------
4. AUTHOR
-------------------------------------------------------------------------------
Andrey Dibrov (andry at inbox dot ru)
