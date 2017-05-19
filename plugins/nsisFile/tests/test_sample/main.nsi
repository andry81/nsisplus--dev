!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "MUI2.nsh"
!include "${TEST_LIB_ROOT}\common.nsi"
!include "${TEST_LIB_ROOT}\config.nsi"

Name "${TEST_TITLE}"

!insertmacro MUI_PAGE_INSTFILES

Section -Hidden
  MessageBox MB_OK "Waiting Debugger..."

  Delete "Temp.tmp"
  FileOpen $0 "Temp.tmp" "a"
  FileWrite $0 "Test-Atotorney"
  FileSeek $0 -4 END

  nsisFile::FileReadBytes $0 5
  Pop $2
  Pop $1

  DetailPrint 'FileReadBytes: $2: read: "$1" = "726E6579"'
  FileSeek $0 1

  nsisFile::FileWriteBytes $0 "6f 746F d-u-m-b"
  Pop $2
  Pop $1

  DetailPrint 'FileWriteBytes: $2: written: $1 = 3'
  FileSeek $0 0

  nsisFile::FileFindBytes $0 "42" -1
  Pop $2
  Pop $1

  DetailPrint 'FileFindBytes: $2: searching "B": $1 = -1'
  FileSeek $0 0

  nsisFile::HexToBin "746f726e"
  Pop $1
  DetailPrint 'HexToBin: "$1" = "torn"'

  nsisFile::BinToHex $1
  Pop $1
  DetailPrint 'BinToHex: "$1" = "746F726E"'

  nsisFile::FileFindBytes $0 "$1" -1
  Pop $2
  Pop $1  ; should be 1
  DetailPrint 'FileFindBytes: $2: searching "torn": $1 = 8'

  nsisFile::FileTruncate $0
  Pop $1
  DetailPrint 'FileTruncate: $1'
  FileClose $0
SectionEnd

ShowInstDetails show

!insertmacro MUI_LANGUAGE "English"
