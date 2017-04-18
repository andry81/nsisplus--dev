; Windows-1251 source encoding

!define Remote "192.168.1.151"
!define RemoteUser "Tester"
!define RemotePass "tester"
!define Provider "Negotiate"  ; Kerberos + NTLM (secur32.dll)
#!define Provider "NEGOExts"  ; Windows 7 and higher (custom SSPs)
#!define Provider "Kerberos"  ; Windows 2000 and higher (secur32.dll)
#!define Provider "NTLM"      ; Windows NT 3.51 and higher (Msv1_0.dll)
#!define Provider "Digest"    ; Windows XP and higher (wdigest.dll)
#!define Provider "CredSSP"   ; Windows XP SP3 and higher (credssp.dll)
#!define Provider "Schannel"  ; Windows 2000 and higher (schannel.dll)

!define LocalUser "User"
!define LocalPass "123"
#!define ADMINS_GROUP "$(ADMINS_GROUP)"
!define ADMINS_GROUP $ADMINS_GROUP
#!define ADMINS_GROUP "Администраторы"
#!define ADMINS_GROUP "BUILTIN\Administrators"
#!define ADMINS_GROUP "Administrators"
