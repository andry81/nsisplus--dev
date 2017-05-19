; plugin load priority is in reverse order!
!addplugindir "${PROJECT_ROOT}" ; already built plugin
!addplugindir "${PROJECT_ROOT}\tests\_testlib"
!addplugindir "${PROJECT_ROOT}\${BUILD_DIR_NAME}" ; building one at first
!addincludedir .
!addincludedir "${PROJECT_ROOT}\tests\_testlib"
