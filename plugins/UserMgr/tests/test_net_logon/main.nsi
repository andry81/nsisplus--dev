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
  
  DetailPrint "Getting remote accound SID: \\${Remote} $\"${RemoteUser}$\"..."
  UserMgr::GetSIDFromUserName "\\${Remote}" "${RemoteUser}"
  Pop $LAST_STATUS_STR
  DetailPrint "Last status message: $LAST_STATUS_STR"

  # won't work
  #DetailPrint "User password remote policy check: \\${Remote}..."
  #UserMgr::ValidateLogonPass "\\${Remote}" "${RemotePass}"
  #Pop $LAST_STATUS_STR
  #${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  #IntFmt $R0 "0x%08X" $LAST_ERROR
  #DetailPrint "Last error message: ($R0) $\"$R1$\""

  DetailPrint "Logging on: \\${Remote} $\"${RemoteUser}$\"..."
  UserMgr::TryLogonNetShare "\\${Remote}" "${RemoteUser}" "${RemotePass}"
  Pop $LAST_STATUS_STR
  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "Last error message: ($R0) $\"$R1$\""
  
  UserMgr::GetLogonNetShareError
  Pop $LAST_ERROR
  Pop $LAST_STATUS_STR
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "Last WNet error message: ($R0) $\"$LAST_STATUS_STR$\""
SectionEnd

ShowInstDetails show

!insertmacro MUI_LANGUAGE "English"
