dnl configure.ac
dnl
dnl xfce4-fsguard-plugin - <Plugin for xfce4-panel>
dnl
dnl 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
dnl

AC_INIT([configure])

AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([xfce4-weather-plugin], [0.4.9])

AM_MAINTAINER_MODE

dnl Check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl Check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl Check for standard header files
AC_HEADER_STDC

dnl check for xml
PKG_CHECK_MODULES(XML_LIBS, libxml-2.0 >= 0,,exit)
AC_SUBST(XML_LIBS_LIBS)
AC_SUBST(XML_LIBS_CFLAGS)

dnl check for i18n support
BM_I18N([xfce4-weather], [de fr ja lt nl sk zh_TW])

dnl configure the panel plugin
XFCE_PANEL_PLUGIN([XFCE4_PANEL], [4.1.90])

dnl Check for debugging support
BM_DEBUG_SUPPORT

AC_OUTPUT([
Makefile
panel-plugin/Makefile
po/Makefile.in
icons/Makefile
icons/liquid/Makefile
])
