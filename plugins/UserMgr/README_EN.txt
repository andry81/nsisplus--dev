* README_EN.txt
* 2017.05.28
* UserMgr

1. DESCRIPTION
2. LICENSE
3. REPOSITORIES
4. DEPLOY
5. TESTS
6. AUTHOR EMAIL

-------------------------------------------------------------------------------
1. DESCRIPTION
-------------------------------------------------------------------------------
This is the fork of the NSIS UserMgr plugin from here:
http://nsis.sourceforge.net/UserMgr_plug-in

The latest version is here: sf.net/p/nsisplus

WARNING:
  Use the SVN access to find out new functionality and bug fixes:
    https://svn.code.sf.net/p/nsisplus/NsisSetupDev/trunk

Main list of changes:
* the project converted into Microsoft Visual Studio 2010
* lastest NSIS 3.x SDK
* patches and bug fixes
* old functionality rewriten to fit Visual Studio C++
* new functionality including async versions of already existed functions
  (see sources for details)
* tests

-------------------------------------------------------------------------------
2. LICENSE
-------------------------------------------------------------------------------
Original author: http://nsis.sourceforge.net/User:Hgerstung

As is, no warranties and guarantees or anything similar. Use at your own risk.
So long and be the code with you.

-------------------------------------------------------------------------------
3. REPOSITORIES
-------------------------------------------------------------------------------
Primary:
  * https://svn.code.sf.net/p/nsisplus/NsisSetupDev/trunk
Secondary:
  * https://github.com/andry81/nsisplus--NsisSetupDev.git

-------------------------------------------------------------------------------
4. DEPLOY
-------------------------------------------------------------------------------
1. Install Microsoft Visual Studio 2010 + SP1Rel.
2. Copy the boost directory from the /NsisSetupDev/3dparty to
   /NsisSetupDev/plugins/UserMgr/libs if not done yet.

-------------------------------------------------------------------------------
5. TESTS
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
6. AUTHOR EMAIL
-------------------------------------------------------------------------------
Andrey Dibrov (andry at inbox dot ru)
