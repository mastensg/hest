AC_INIT(hest, 0.2.3)
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_CHECK_HEADERS([stdlib.h string.h unistd.h])
AC_CHECK_FUNCS([memset])

AC_CHECK_HEADER([X11/Xlib.h], [], [AC_MSG_ERROR([libx11 required (apt-get install libx11-dev)])])
AC_CHECK_HEADER([X11/extensions/Xinerama.h], [], [AC_MSG_ERROR([libxinerama required (apt-get install libxinerama-dev)])])

AC_CHECK_LIB([X11], [XOpenDisplay], [], [AC_MSG_ERROR([libx11 required (apt-get install libx11-dev)])])
AC_CHECK_LIB([Xinerama], [XineramaQueryScreens], [], [AC_MSG_ERROR(libxinerama required (apt-get install libxinerama-dev))])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_OUTPUT(Makefile hest.1)
