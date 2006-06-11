dnl configure.ac
dnl
dnl xfce4-weather-plugin - <Plugin for xfce4-panel>
dnl
dnl 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
dnl

m4_define([weather_version],[0.5.0])

AC_INIT([xfce4-weather-plugin],[weather_version()],
	[xfce-goodies-dev@lists.berlios.de])

WEATHER_VERSION=weather_version()
AM_INIT_AUTOMAKE([xfce4-weather-plugin], [$WEATHER_VERSION])
AM_CONFIG_HEADER([config.h])

AM_MAINTAINER_MODE

dnl Check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl Check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_INTLTOOL

dnl Check for standard header files
AC_HEADER_STDC
AC_CHECK_HEADERS([string.h fcntl.h errno.h sys/socket.h netdb.h netinet/in.h sys/types.h unistd.h stdio.h])

dnl check for xml
XDT_CHECK_PACKAGE([LIBXML], [libxml-2.0], [2.4.0])

dnl configure the panel plugin
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-1.0], [4.3.90])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.3.90])

dnl check for gtk
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [2.6.0])
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [2.6.0])
XDT_CHECK_PACKAGE([GMODULE], [gmodule-2.0], [2.6.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-2.0], [2.6.0])

dnl check for i18n support
XDT_I18N([de fr lt nl sk zh_TW])

dnl Check for debugging support
XDT_FEATURE_DEBUG()

dnl Compilation fails with --as-needed in the LDFLAGS
AC_MSG_CHECKING([Check if --as-needed is in the LDFLAGS])
if echo ${LDFLAGS} | grep as-needed >/dev/null 2>&1; then
  LDFLAGS=""
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([whether to use new header dialog])
if $PKG_CONFIG --atleast-version 4.3.90.2 libxfcegui4-1.0 >/dev/null 2>&1; then
  AC_DEFINE([USE_NEW_DIALOG], [1], [Define to use new header dialog])
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

AC_OUTPUT([
Makefile
panel-plugin/Makefile
po/Makefile.in
icons/Makefile
icons/liquid/Makefile
icons/48x48/Makefile
])

