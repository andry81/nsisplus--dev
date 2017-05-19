!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "${TEST_LIB_ROOT}\common.nsi"
!include "${TEST_LIB_ROOT}\config.nsi"

Name "${TEST_TITLE}"

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

  DetailPrint "Logging on: \\${Remote} $\"${RemoteUser}$\"..."
  UserMgr::TrySSPLogonUser "\\${Remote}" "${RemoteUser}" "${RemotePass}" "${Provider}"
  Pop $LAST_STATUS_STR
  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "Last error message: ($R0) $\"$R1$\""
  
  UserMgr::GetSSPLogonUserError
  Pop $LAST_STATUS_STR
  Pop $LAST_ERROR
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "SSPI Last error message: ($R0) $\"$LAST_STATUS_STR$\""
SectionEnd

ShowInstDetails show

!insertmacro MUI_LANGUAGE "English"
