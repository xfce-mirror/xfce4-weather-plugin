dnl configure.ac
dnl
dnl xfce4-sample-plugin - Sample plugin for xfce4-panel
dnl
dnl 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
dnl

AC_INIT([configure])

AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([xfce4-weather-plugin], [0.3.9])

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

dnl configure the panel plugin
XFCE_PANEL_PLUGIN([XFCE4_PANEL], [0.10.1])

PKG_CHECK_MODULES(XML_LIBS, libxml-2.0 >= 0,,exit)
AC_SUBST(XML_LIBS_LIBS)
AC_SUBST(XML_LIBS_CFLAGS)


dnl Check for debugging support
BM_DEBUG_SUPPORT

AC_OUTPUT([
Makefile
panel-plugin/Makefile
icons/Makefile
icons/liquid/Makefile
])
