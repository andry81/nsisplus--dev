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

  ; save current locale
  UserMgr::GetLocale LC_CTYPE
  Pop $LOCALE

  ; reset current locale
  UserMgr::SetLocale LC_CTYPE ".${LocaleCharset}"
  
  DetailPrint "Old locale: $\"$LOCALE$\""
  DetailPrint "New locale: $\".${LocaleCharset}$\""

  DetailPrint "Checking user existence: $\"${LocalUser}$\"..."
  UserMgr::GetUser "${LocalUser}"
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

  DetailPrint "Creating the administrative user: $\"${LocalUser}$\"..."
  UserMgr::CreateAccount "${LocalUser}" "${LocalPass}" "PostWin administrative user"
  Pop $LAST_STATUS_STR
  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "Last error message: ($R0) $\"$R1$\""
  
  UserMgr::GetUserNameFromSID "${SG_ADMINISTRATORS}"
  Pop $ADMINS_GROUP
  
  DetailPrint "Adding the user to Administrators group: $\"${LocalUser}$\" -> $\"${ADMINS_GROUP}$\"..."
  UserMgr::AddToGroup "${LocalUser}" "${ADMINS_GROUP}"
  Pop $LAST_STATUS_STR
  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
  IntFmt $R0 "0x%08X" $LAST_ERROR
  DetailPrint "Last error message: ($R0) $\"$R1$\""

#  DetailPrint "Building account environment for the user: $\"${LocalUser}$\"..."
#  UserMgr::BuiltAccountEnv "${LocalUser}" "${LocalPass}"
#  Pop $LAST_STATUS_STR
#  ${GetUserMgrErrorMessage} $LAST_STATUS_STR $LAST_ERROR $R1
#  IntFmt $R0 "0x%08X" $LAST_ERROR
#  DetailPrint "Last error message: ($R0) $\"$R1$\""
SectionEnd

ShowInstDetails show

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"

LangString ADMINS_GROUP ${LANG_ENGLISH} "ANDRYDT\Administrators"
LangString ADMINS_GROUP ${LANG_RUSSIAN} "ANDRYDT\Администраторы"