; plugin load priority is in reverse order!
!addplugindir "${PROJECT_ROOT}" ; already built plugin
!addplugindir "${PROJECT_ROOT}\tests\_testlib"
!addplugindir "${PROJECT_ROOT}\${BUILD_DIR_NAME}" ; building one at first
!addincludedir .
!addincludedir "${PROJECT_ROOT}\tests\_testlib"

Var /GLOBAL PROCESS_ID

Var /GLOBAL DialogID
Var /GLOBAL LAST_ERROR
Var /GLOBAL LAST_STATUS_STR

Var /GLOBAL MACRO_RET_R0
Var /GLOBAL MACRO_RET_R1
Var /GLOBAL MACRO_RET_R2
Var /GLOBAL MACRO_RET_R3
Var /GLOBAL MACRO_RET_R4
Var /GLOBAL MACRO_RET_R5
Var /GLOBAL MACRO_RET_R6
Var /GLOBAL MACRO_RET_R7
Var /GLOBAL MACRO_RET_R8
Var /GLOBAL MACRO_RET_R9

Var /GLOBAL ADMINS_GROUP

Var /GLOBAL ASYNC_ID
Var /GLOBAL CANCEL_ASYNC_BUTTON_ID

!define FRAME_TIME_SPAN 1000 ; ideal time between calls/prints (msec)
Var /GLOBAL FRAMES_OVERALL
Var /GLOBAL FRAMES
Var /GLOBAL FIRST_FRAME_TICKS

Var /GLOBAL CANCEL

Var /GLOBAL LOCALE

!define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
!define FORMAT_MESSAGE_FROM_STRING     0x00000400
!define FORMAT_MESSAGE_FROM_HMODULE    0x00000800
!define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
!define FORMAT_MESSAGE_ARGUMENT_ARRAY  0x00002000
!define FORMAT_MESSAGE_MAX_WIDTH_MASK  0x000000FF

!define SG_ADMINISTRATORS                 "S-1-5-32-544"
!define SG_USERS                          "S-1-5-32-545"
!define SG_POWERUSERS                     "S-1-5-32-547"
!define SG_GUESTS                         "S-1-5-32-546"

!define SG_EVERYONE                       "S-1-1-0"
!define SG_CREATOROWNER                   "S-1-3-0"
!define SG_NTAUTHORITY_NETWORK            "S-1-5-2"
!define SG_NTAUTHORITY_INTERACTIVE        "S-1-5-4"
!define SG_NTAUTHORITY_SYSTEM             "S-1-5-18"
!define SG_NTAUTHORITY_AUTHENTICATEDUSERS "S-1-5-11"
!define SG_NTAUTHORITY_LOCALSERVICE       "S-1-5-19"
!define SG_NTAUTHORITY_NETWORKSERVICE     "S-1-5-20"

; order is matter
!define ASYNC_REQUEST_STATUS_UNINIT       -2    ; handle is valid but asynchronous request thread is not yet initialized
!define ASYNC_REQUEST_STATUS_PENDING      -1    ; handle is valid and asynchronous request thread is initialized but is still pending
!define ASYNC_REQUEST_STATUS_ACCOMPLISH   0     ; asynchronous request is finished
!define ASYNC_REQUEST_STATUS_ABORTED      1     ; asynchronous request is aborted in alive thread
!define ASYNC_REQUEST_STATUS_CANCELLED    254   ; asynchronous request is cancelled, thread associated with the request is terminated
!define ASYNC_REQUEST_STATUS_NOT_FOUND    255   ; handle is not associated to anyone asynchronous request


; locale
!define LC_ALL          0
!define LC_COLLATE      1
!define LC_CTYPE        2
!define LC_MONETARY     3
!define LC_NUMERIC      4
!define LC_TIME         5

!define LC_MIN          LC_ALL
!define LC_MAX          LC_TIME


!define GetWin32ErrorMesssage "!insertmacro GetWin32ErrorMesssage"
!macro GetWin32ErrorMesssage id var
Push $R0
Push $R1
Push $R3
Push $R9

#System::Call "kernel32::GetSystemDefaultLangID(v) i .R1"
StrCpy $R1 $LANGUAGE

StrCpy $R9 ${FORMAT_MESSAGE_FROM_SYSTEM}
#IntOp $R9 $R9 + ${FORMAT_MESSAGE_ALLOCATE_BUFFER}
IntOp $R9 $R9 + ${FORMAT_MESSAGE_IGNORE_INSERTS}
IntOp $R9 $R9 + ${FORMAT_MESSAGE_MAX_WIDTH_MASK}

System::Call "kernel32::FormatMessage(i,p,i,i,t,i,p) i .R3 (R9, 0, ${id}, R1, .R0, ${NSIS_MAX_STRLEN}, 0)"

Pop $MACRO_RET_R9
Pop $MACRO_RET_R3
Pop $MACRO_RET_R1
Pop $MACRO_RET_R0

; R0 - return value
!if "${var}" S!= "$R0"
StrCpy ${var} $R0
!endif

; restore registers
!if "${var}" S!= "$R0"
StrCpy $R0 $MACRO_RET_R0
!endif
!if "${var}" S!= "$R1"
StrCpy $R1 $MACRO_RET_R1
!endif
!if "${var}" S!= "$R3"
StrCpy $R3 $MACRO_RET_R3
!endif
!if "${var}" S!= "$R9"
StrCpy $R9 $MACRO_RET_R9
!endif
!macroend

!define GetUserMgrErrorMessage "!insertmacro GetUserMgrErrorMessage"
!macro GetUserMgrErrorMessage mgr_msg err_var msg_var
Push $R0
Push $R1
Push $R2
Push $R9

!if "${mgr_msg}" S!= "$R0"
StrCpy $R0 "${mgr_msg}"
!endif

StrCpy $R9 ""

StrCpy $R1 $R0 6
${If} $R1 == "ERROR "
  StrCpy $R2 $R0 "" 6
  ${GetWin32ErrorMesssage} $R2 $R9
${Else}
  StrCpy $R2 0
${EndIf}

Pop $MACRO_RET_R9
Pop $MACRO_RET_R2
Pop $MACRO_RET_R1
Pop $MACRO_RET_R0

; return values
!if "${err_var}" S!= "$R2"
StrCpy ${err_var} $R2
!endif
!if "${msg_var}" S!= "$R9"
StrCpy ${msg_var} $R9
!endif

; restore registers
!if "${err_var}" S!= "$R0"
!if "${msg_var}" S!= "$R0"
StrCpy $R0 $MACRO_RET_R0
!endif
!endif
!if "${err_var}" S!= "$R1"
!if "${msg_var}" S!= "$R1"
StrCpy $R1 $MACRO_RET_R1
!endif
!endif
!if "${err_var}" S!= "$R2"
!if "${msg_var}" S!= "$R2"
StrCpy $R2 $MACRO_RET_R2
!endif
!endif
!if "${err_var}" S!= "$R9"
!if "${msg_var}" S!= "$R9"
StrCpy $R9 $MACRO_RET_R9
!endif
!endif
!macroend

!define GetTickCount "!insertmacro GetTickCount"
!macro GetTickCount var
Push $R0

System::Call "kernel32::GetTickCount() i.R0"

Pop $MACRO_RET_R0

; return values
!if "${var}" S!= "$R0"
StrCpy ${var} $R0
!endif

; restore registers
!if "${var}" S!= "$R0"
StrCpy $R0 $MACRO_RET_R0
!endif
!macroend

!define BeginFrameWait "!insertmacro BeginFrameWait"
!macro BeginFrameWait frames_var first_frame_ticks_var
!if "${frames_var}" == "${first_frame_ticks_var}"
!error "BeginFrameWait: frames_var and first_frame_ticks_var must be different: frames_var=$\"${frames_var}$\" first_frame_ticks_var=$\"${first_frame_ticks_var}$\""
!endif
StrCpy ${frames_var} 0
${GetTickCount} ${first_frame_ticks_var}
!macroend

; CAUTION:
;   frame_time_span should be the same in frame-to-frame calls.
!define GetNextFrameWaitTime "!insertmacro GetNextFrameWaitTime"
!macro GetNextFrameWaitTime next_frame_wait_time_var shifted_next_frame_var next_frame first_frame_ticks frame_time_span
${If} ${next_frame} >= 0
  Push $R0
  Push $R5
  Push $R6
  Push $R8
  Push $R9

  StrCpy $R5 ${next_frame}
  StrCpy $R6 ${frame_time_span}

  ; calculate sleep timeout
  IntOp $R9 $R5 * $R6
  IntOp $R9 $R9 + ${first_frame_ticks}

  ${GetTickCount} $R0 ; last frame ticks
  IntOp $R9 $R9 - $R0
  
  ; WARNING:
  ;   To compensate too long wait between ping frames we have to return shifted_next_frame_var to notificate a caller on how much time spent
  ;   after last frame and what frame should be next at current time.
  ;   For example, if next_frame=10 but time spent is greater frame_time_span from 9th frame, then the subtraction will be negative and we must report 
  ;   shifted_next_frame_var=11.
  
  ${If} $R9 >= 0
    StrCpy $R8 $R5
  ${Else}
    ; negative subtraction, make compensate
    IntOp $R8 0 - $R9
    IntOp $R8 $R8 + $R6
    IntOp $R8 $R8 - 1 ; truncation to lowest
    IntOp $R8 $R8 / $R6
    IntOp $R8 $R8 + $R5 ; will be next frame to call

    ; recalculate new next frame sleep
    IntOp $R9 $R8 * ${frame_time_span}
    IntOp $R9 $R9 - $R0
    ${If} $R9 < 0 ; precision flow
      StrCpy $R9 0
    ${EndIf}
  ${EndIf}

  Pop $MACRO_RET_R9
  Pop $MACRO_RET_R8
  Pop $MACRO_RET_R6
  Pop $MACRO_RET_R5
  Pop $MACRO_RET_R0

  ; return values
  !if "${next_frame_wait_time_var}" S!= "$R9"
  StrCpy ${next_frame_wait_time_var} $R9
  !endif
  !if "${shifted_next_frame_var}" S!= "$R8"
  StrCpy ${shifted_next_frame_var} $R8
  !endif

  ; restore registers
  !if "${next_frame_wait_time_var}" S!= "$R0"
  !if "${shifted_next_frame_var}" S!= "$R0"
  StrCpy $R0 $MACRO_RET_R0
  !endif
  !endif
  !if "${next_frame_wait_time_var}" S!= "$R5"
  !if "${shifted_next_frame_var}" S!= "$R5"
  StrCpy $R5 $MACRO_RET_R5
  !endif
  !endif
  !if "${next_frame_wait_time_var}" S!= "$R6"
  !if "${shifted_next_frame_var}" S!= "$R6"
  StrCpy $R6 $MACRO_RET_R6
  !endif
  !endif
  !if "${next_frame_wait_time_var}" S!= "$R8"
  !if "${shifted_next_frame_var}" S!= "$R8"
  StrCpy $R8 $MACRO_RET_R8
  !endif
  !endif
  !if "${next_frame_wait_time_var}" S!= "$R9"
  !if "${shifted_next_frame_var}" S!= "$R9"
  StrCpy $R9 $MACRO_RET_R9
  !endif
  !endif
${Else}
  StrCpy ${next_frame_wait_time_var} 0
  !if "${shifted_next_frame_var}" != "${next_frame}"
  StrCpy ${shifted_next_frame_var} ${next_frame}
  !endif
${EndIf}
!macroend
