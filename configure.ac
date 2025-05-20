dnl
dnl xfce4-weather-plugin - Show current weather conditions in your panel
dnl
dnl 2003      Benedikt Meurer <benny@xfce.org>
dnl 2006-2007 Nick Schermer <nick@xfce.org>
dnl

dnl ***************************
dnl *** Version information ***
dnl ***************************
m4_define([copyright_year], [2025])
XDT_VERSION_INIT([0.12.0])

dnl *******************************************
dnl *** Debugging support for git snapshots ***
dnl *******************************************
m4_define([weather_debug_default], [ifelse(weather_version_tag(), [git], [yes], [minimum])])

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_COPYRIGHT([Copyright (c) 2004-copyright_year()
        The Xfce development team. All rights reserved.])
AC_INIT([xfce4-weather-plugin], [xdt_version], [https://gitlab.xfce.org/panel-plugins/xfce4-weather-plugin], [xfce4-weather-plugin])
AC_REVISION([xdt_version_build])
AC_PREREQ([2.69])
AC_CONFIG_MACRO_DIRS([m4])
AC_DEFINE([VERSION_FULL], [PACKAGE_VERSION], [Alias for VERSION and PACKAGE_VERSION for meson compatibility])
COPYRIGHT_YEAR=copyright_year()
AC_DEFINE_UNQUOTED([COPYRIGHT_YEAR], ["$COPYRIGHT_YEAR"], [Copyright year])
AC_SUBST([COPYRIGHT_YEAR])

dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE([1.9 dist-bzip2 tar-ustar no-dist-gzip])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
LT_PATH_LD([])
AC_PROG_INSTALL()
LT_PREREQ([2.2.6])
LT_INIT([disable-static])
AM_PROG_CC_C_O()

dnl **********************************
dnl *** Check for standard headers ***
dnl **********************************
AC_CHECK_HEADERS([math.h stdarg.h stddef.h stdlib.h string.h sys/stat.h time.h])
LT_LIB_M
AC_SUBST(LIBM)

dnl ******************************
dnl *** Check for i18n support ***
dnl ******************************
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [3.22.0])
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [2.64.0])
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [2.64.0])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [4.16.0])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [4.16.0])
XDT_CHECK_PACKAGE([LIBXML], [libxml-2.0], [2.4.0])
dnl XDT_CHECK_PACKAGE([SOUP], [libsoup-3.0], [3.0.0])
XDT_CHECK_PACKAGE([SOUP], [libsoup-3.0], [3.0.0], [],
                          [XDT_CHECK_PACKAGE([SOUP], [libsoup-2.4], [2.42.0])])
XDT_CHECK_PACKAGE([JSON], [json-c], [0.13.1])
XDT_CHECK_OPTIONAL_PACKAGE([UPOWER_GLIB], [upower-glib], [0.99.0], [upower],
                           [upower for adapting update interval to power state])
XDT_CHECK_PACKAGE_BINARY([GLIB_COMPILE_RESOURCES], [gio-2.0], [glib_compile_resources], [glib-compile-resources])


dnl ************************************
dnl *** Special compile time options ***
dnl ************************************
AC_ARG_VAR([GEONAMES_USERNAME], [Username for the GeoNames webservice.
          Please register your own if you're going to distribute the
          package, as requests are limited per hour and day.
          See README for further information.])
AS_IF([test "x$GEONAMES_USERNAME" = x], [GEONAMES_USERNAME="xfce4weatherplugin"])


dnl ***********************************
dnl *** Check for debugging support ***
dnl ***********************************
XDT_FEATURE_DEBUG([xdt_debug_default])

AC_CONFIG_FILES([
Makefile
panel-plugin/Makefile
po/Makefile.in
icons/Makefile
icons/liquid/Makefile
icons/liquid/22/Makefile
icons/liquid/48/Makefile
icons/liquid/128/Makefile
icons/liquid-dark/Makefile
icons/liquid-dark/22/Makefile
icons/liquid-dark/48/Makefile
icons/liquid-dark/128/Makefile
icons/simplistic/Makefile
icons/simplistic/22/Makefile
icons/simplistic/48/Makefile
icons/simplistic/128/Makefile
icons/16x16/Makefile
icons/32x32/Makefile
icons/48x48/Makefile
icons/128x128/Makefile
icons/scalable/Makefile
])
AC_OUTPUT

echo
echo "Build Configuration:"
echo
echo "* Debug Support:             $enable_debug"
echo "* UPower Glib Support:       ${UPOWER_GLIB_FOUND:-no}"
echo "* Soup Version:              ${SOUP_VERSION:-no}"
