!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "${TEST_LIB_ROOT}\common.nsi"
!include "${TEST_LIB_ROOT}\config.nsi"

!insertmacro MUI_PAGE_INSTFILES

Page Custom Show Leave

Function Show
  nsDialogs::Create 1018
  Pop $DialogID

  nsDialogs::Show
FunctionEnd

Function Leave
FunctionEnd

Section -Hidden
  MessageBox MB_OK "Waiting Debugger..."

  DetailPrint "Getting current user name..."
  UserMgr::GetCurrentUserName
  Pop $LAST_STATUS_STR
  DetailPrint "Last status message: $LAST_STATUS_STR"

  DetailPrint "Getting current logon domain..."
  UserMgr::GetCurrentDomain
  Pop $LAST_STATUS_STR
  DetailPrint "Last status message: $LAST_STATUS_STR"

  DetailPrint "Getting local accound SID: $\"${LocalUser}$\"..."
  UserMgr::GetSIDFromUserName "" "${LocalUser}"
  Pop $LAST_STATUS_STR
  DetailPrint "Last status message: $LAST_STATUS_STR"

  DetailPrint "Checking user existence: $\"${LocalUser}$\"..."
  UserMgr::GetUser "${LocalUser}"
  Pop $LAST_STATUS_STR
  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "Last error message: ($R0) $\"$R1$\""

  DetailPrint "User password local policy check..."
  UserMgr::ValidateLogonPass "" "${LocalPass}"
  Pop $LAST_STATUS_STR
  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "Last error message: ($R0) $\"$R1$\""

  DetailPrint "Empty pass logon: $\"${LocalUser}$\"..."
  UserMgr::TryLogonUser "${LocalUser}" ""
  Pop $LAST_STATUS_STR
  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "Last error message: ($R0) $\"$R1$\""

  DetailPrint "Checking user credentials: $\"${LocalUser}$\"..."
  UserMgr::TryLogonUser "${LocalUser}" "${LocalPass}"
  Pop $LAST_STATUS_STR
  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "Last error message: ($R0) $\"$R1$\""
SectionEnd

ShowInstDetails show

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"

LangString ADMINS_GROUP ${LANG_ENGLISH} "ANDRYDT\Administrators"
LangString ADMINS_GROUP ${LANG_RUSSIAN} "ANDRYDT\Администраторы"