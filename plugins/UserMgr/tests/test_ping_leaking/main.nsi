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
  System::Call "kernel32::GetCurrentProcessId() i.R9"
  StrCpy $PROCESS_ID $R9

  MessageBox MB_OK "Waiting Debugger..."

  DetailPrint "Pinging ${Remote}..."
  StrCpy $R9 0
  ping_loop:
    UserMgr::Ping "${Remote}" "test222-$PROCESS_ID" "100" ; 1000ms timeout
    Pop $LAST_STATUS_STR
    ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R8
    UserMgr::GetLastPingReply
    Pop $R0 ; RTT
    Pop $R1 ; status
    Pop $R2 ; address
    Pop $R3 ; responces
    Pop $R4 ; reply
    UserMgr::GetPingStatusMessage $R1
    Pop $R5
    IntOp $7 $R9 % 1000
    ${If} $7 = 1 ; to decrease memory increasing by DetailPrint (memory leak involves memory increase too)
      IntFmt $R6 "0x%08X" $LAST_ERROR
      DetailPrint "Ping: $R9: $R0ms a=$R2 n=$R3 s=($R1) $\"$R5$\" r=$\"$R4$\" e=($R6;$LAST_ERROR) $\"$R8$\""
    ${EndIf}
    IntOp $R9 $R9 + 1
    ${If} $R9 < 1000000000
      Goto ping_loop
    ${EndIf}
SectionEnd

ShowInstDetails show

!insertmacro MUI_LANGUAGE "English"
