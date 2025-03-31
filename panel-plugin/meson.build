plugin_sources = [
  'weather-config.c',
  'weather-config.h',
  'weather-data.c',
  'weather-data.h',
  'weather-debug.c',
  'weather-debug.h',
  'weather-icon.c',
  'weather-icon.h',
  'weather-parsers.c',
  'weather-parsers.h',
  'weather-scrollbox.c',
  'weather-scrollbox.h',
  'weather-search.c',
  'weather-search.h',
  'weather-summary.c',
  'weather-summary.h',
  'weather-translate.c',
  'weather-translate.h',
  'weather.c',
  'weather.h',
  xfce_revision_h,
]

plugin_sources += gnome.compile_resources(
  'weather-config-resources',
  'weather-config.gresource.xml',
)

plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

plugin_lib = shared_module(
  'weather',
  plugin_sources,
  gnu_symbol_visibility: 'hidden',
  c_args: [
    '-DG_LOG_DOMAIN="@0@"'.format('xfce4-weather-plugin'),
    '-DTHEMESDIR="@0@"'.format(pkgdatadir / 'icons'),
    '-DGEONAMES_USERNAME="@0@"'.format(get_option('geonames-username')),
  ],
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gtk,
    json,
    libm,
    libsoup,
    libxfce4panel,
    libxfce4ui,
    libxfce4util,
    libxml,
    upower_glib,
    xfconf,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

i18n.merge_file(
  input: 'weather.desktop.in',
  output: 'weather.desktop',
  po_dir: '..' / 'po',
  type: 'desktop',
  install: true,
  install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)
