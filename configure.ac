dnl configure.ac
dnl
dnl xfce4-fsguard-plugin - <Plugin for xfce4-panel>
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

dnl check for xml
XDT_CHECK_PACKAGE([LIBXML], [libxml-2.0], [2.4.0])

dnl configure the panel plugin
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-1.0], [4.3.22])

dnl check for i18n support
XDT_I18N([de fr lt nl sk zh_TW])

dnl Check for debugging support
XDT_FEATURE_DEBUG()

AC_OUTPUT([
Makefile
panel-plugin/Makefile
po/Makefile.in
icons/Makefile
icons/liquid/Makefile
])

