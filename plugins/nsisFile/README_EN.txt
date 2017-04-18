* README_EN.txt
* 2017.04.18
* NsisFile

1. DESCRIPTION
2. LICENSE
3. DEPLOY
4. TESTS
5. AUTHOR EMAIL

-------------------------------------------------------------------------------
1. DESCRIPTION
-------------------------------------------------------------------------------
This is the fork of the NSIS nsisFile plugin from here:
http://nsis.sourceforge.net/NsisFile_plug-in
http://wiz0u.free.fr/prog/nsisFile/

Main list of changes:
* the project converted into Microsoft Visual Studio 2010
* lastest NSIS 3.x SDK
* patches and bug fixes
* old functionality rewriten to fit Visual Studio C++
* old functionality changed to show the GetLastError result on error
  (see sources for details)
* tests

-------------------------------------------------------------------------------
2. LICENSE
-------------------------------------------------------------------------------
Original author: Olivier Marcoux (http://nsis.sourceforge.net/User:Wizou)

As is, no warranties and guarantees or anything similar. Use at your own risk.
So long and be the code with you.

-------------------------------------------------------------------------------
3. DEPLOY
-------------------------------------------------------------------------------
1. Install Microsoft Visual Studio 2010 + SP1Rel.

-------------------------------------------------------------------------------
4. TESTS
-------------------------------------------------------------------------------
1. Copy into the libs/nsis directory the actual nsis SDK headers and libraries
   with which you want to build.
2. Run tests/configure.bat.
3. Edit tests/configure.user.bat for correct environment variables.
   The MAKENSIS_EXE variable must point to existing nsis executable.
4. Edit tests/<TestName>/main.nsi for correct test definitions.
5. Run tests/<TestName>/build.bat to build a test.
6. Run tests/<TestName>/test.exe to run a test.

-------------------------------------------------------------------------------
5. AUTHOR EMAIL
-------------------------------------------------------------------------------
Andrey Dibrov (andry at inbox dot ru)
