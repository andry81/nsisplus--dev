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
  ${BeginFrameWait} $FRAMES $FIRST_FRAME_TICKS

  repeat_loop:
    IntOp $FRAMES $FRAMES + 1

    UserMgr::Ping "${Remote}" "test111-$PROCESS_ID" "100" ; 100ms internal timeout
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
    IntFmt $R6 "0x%08X" $LAST_ERROR
    DetailPrint "Ping: $FRAMES: $R9: $R0ms a=$R2 n=$R3 s=($R1) $\"$R5$\" r=$\"$R4$\" e=($R6;$LAST_ERROR) $\"$R8$\""

    ${If} $FRAMES < 20
      ${GetNextFrameWaitTime} $R7 $FRAMES $FRAMES $FIRST_FRAME_TICKS ${FRAME_TIME_SPAN}
      ${If} $R7 > 20 ; 20 is resolution of GetTickCount
        DetailPrint "Sleep: $R7"
        Sleep $R7
      ${EndIf}

      Goto repeat_loop
    ${EndIf}
SectionEnd

ShowInstDetails show

!insertmacro MUI_LANGUAGE "English"
