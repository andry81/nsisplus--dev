!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "${TEST_LIB_ROOT}\common.nsi"
!include "${TEST_LIB_ROOT}\config.nsi"

!insertmacro MUI_PAGE_INSTFILES

Section -Hidden
  MessageBox MB_OK "Waiting Debugger..."

  StrCpy $5 "Test1234567890"
  StrCpy $9 "XYZ"

  Delete "Temp.tmp"
  FileOpen $0 "Temp.tmp" "a"
  FileWrite $0 $5
  FileSeek $0 0 END

  nsisFile::BinToHex $9
  Pop $8
  DetailPrint 'BinToHex: "$9" -> "$8"'

  nsisFile::FileWriteInsertBytes $0 $8
  Pop $2
  Pop $1
  DetailPrint 'FileWriteInsertBytes: $2: written: $1 = 3'
 
  FileSeek $0 0 SET
  
  nsisFile::FileReadBytes $0 ${NSIS_MAX_STRLEN}
  Pop $2
  Pop $1

  nsisFile::HexToBin $1
  Pop $1
  DetailPrint 'FileReadBytes: $2: read: "$5" + "$9" = "$1"'

  FileClose $0
SectionEnd

ShowInstDetails show

!insertmacro MUI_LANGUAGE "English"
