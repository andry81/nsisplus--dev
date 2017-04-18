!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "${TEST_LIB_ROOT}\common.nsi"
!include "${TEST_LIB_ROOT}\config.nsi"


!insertmacro MUI_PAGE_INSTFILES

!define MUI_CUSTOMFUNCTION_GUIINIT myGuiInit
#!define MUI_CUSTOMFUNCTION_GUIUNINIT myGuiUninit

Page Custom Show Leave
 
Function myGuiInit
  ; You are supposed to use ChangeUI (or MUI_UI) and a modified ui file to add new buttons but this example adds the button at run-time...
  GetDlgItem $0 $hwndparent 2 ; Find cancel button
  System::Call *(i,i,i,i)i.r1
  System::Call 'USER32::GetWindowRect(ir0,ir1)'
  System::Call *$1(i.r2,i.r3,i.r4,i.r5)
  IntOp $5 $5 - $3 ;height
  IntOp $4 $4 - $2 ;width
  System::Call 'USER32::ScreenToClient(i$hwndparent,ir1)'
  System::Call *$1(i.r2,i.r3)
  System::Free $1
  #IntOp $2 $2 + $4 ;x
  #IntOp $2 $2 + 8  ;x+padding
  StrCpy $2 12
  System::Call 'USER32::CreateWindowEx(i0,t "Button",t "Stop",i${WS_CHILD}|${WS_VISIBLE}|${WS_TABSTOP}|${WS_DISABLED},ir2,ir3,ir4,ir5,i $hwndparent,i 0x666,i0,i0)i.r0'
  StrCpy $CANCEL_ASYNC_BUTTON_ID $0

  SendMessage $hwndparent ${WM_GETFONT} 0 0 $1
  SendMessage $CANCEL_ASYNC_BUTTON_ID ${WM_SETFONT} $1 1

  StrCpy $CANCEL 0

  GetFunctionAddress $1 OnStopClick
  ButtonEvent::AddEventHandler 0x666 $1
FunctionEnd


#Function myGuiUninit
#  MessageBox MB_OK "myGuiUninit"
#  AnimGif::stop
#FunctionEnd

Function OnStopClick
  StrCpy $CANCEL 1 ; quit repeat_loop
FunctionEnd

Function Show
  nsDialogs::Create 1018
  Pop $DialogID

  nsDialogs::Show
FunctionEnd

Function Leave
FunctionEnd

Function PingAsync
  DetailPrint "Pinging ${Remote}..."
  ${BeginFrameWait} $FRAMES $FIRST_FRAME_TICKS

  UserMgr::PingAsync "${Remote}" "test111-$PROCESS_ID" "10000" ; 10s internal timeout
  Pop $LAST_STATUS_STR
  Pop $ASYNC_ID ; async queue handle id, negative if asynchronous request begin
  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  IntFmt $R0 "0x%08X" $LAST_ERROR

  DetailPrint "Begin async ping: id=$ASYNC_ID ($R0) $\"$R1$\""
FunctionEnd

Section -Hidden
  System::Call "kernel32::GetCurrentProcessId() i.R9"
  StrCpy $PROCESS_ID $R9

  MessageBox MB_OK "Waiting Debugger..."

  EnableWindow $CANCEL_ASYNC_BUTTON_ID 1

  Call PingAsync
  StrCpy $FRAMES_OVERALL 0

  repeat_loop:
    IntOp $FRAMES $FRAMES + 1
    IntOp $FRAMES_OVERALL $FRAMES_OVERALL + 1

    IntCmp $CANCEL 0 +1 cancel cancel

    UserMgr::GetPingAsyncStatus $ASYNC_ID
    Pop $R0 ; async request status code
    Pop $LAST_STATUS_STR ; status string

    UserMgr::GetAsyncRequestStatusString $R0
    Pop $R5
    ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
    IntFmt $R2 "0x%08X" $LAST_ERROR
    DetailPrint "Async Ping: $FRAMES_OVERALL: $FRAMES: status=($R0) $\"$R5$\": ($R2) $\"$R1$\""

    Pop $R1 ; RTT
    Pop $R2 ; status
    Pop $R3 ; address
    Pop $R4 ; responces
    Pop $R5 ; reply
    ${If} $R0 = 0 ; ASYNC_REQUEST_STATUS_ACCOMPLISH
      UserMgr::GetPingStatusMessage $R2
      Pop $R6
      DetailPrint "Async Ping: $FRAMES_OVERALL: $FRAMES: Reply: $R1ms a=$R3 n=$R4 s=($R2) $\"$R6$\" r=$\"$R5$\""
    ${EndIf}

    IntCmp $CANCEL 0 +1 cancel cancel
   
    ${If} $FRAMES_OVERALL < 1000
      ${GetNextFrameWaitTime} $R7 $FRAMES $FRAMES $FIRST_FRAME_TICKS ${FRAME_TIME_SPAN}
      ${If} $R7 > 20 ; 20 is resolution of GetTickCount
        DetailPrint "Sleep: $R7"
        Sleep $R7
      ${EndIf}

      ${If} $R0 = 0 ; ASYNC_REQUEST_STATUS_ACCOMPLISH
        DetailPrint ""
      ${EndIf}

      IntCmp $CANCEL 0 +1 cancel cancel

      ${If} $R0 = 0 ; ASYNC_REQUEST_STATUS_ACCOMPLISH
        ; try again
        Call PingAsync
      ${EndIf}

      Goto repeat_loop
    ${EndIf}

  cancel:
  ${If} $CANCEL <> 0
    UserMgr::CancelPingAsync $ASYNC_ID
    Pop $LAST_STATUS_STR
    ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
    IntFmt $R0 "0x%08X" $LAST_ERROR

    DetailPrint "Cancel: id=$ASYNC_ID ($R0) $\"$R1$\""
  ${EndIf}

  EnableWindow $CANCEL_ASYNC_BUTTON_ID 0
SectionEnd

ShowInstDetails show

!insertmacro MUI_LANGUAGE "English"
