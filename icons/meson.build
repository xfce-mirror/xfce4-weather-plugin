sizes = [16, 32, 48, 128]

foreach size : sizes
  install_data(
    '@0@x@0@'.format(size) / 'org.xfce.panel.weather.png',
    install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / '@0@x@0@'.format(size) / 'apps',
  )
endforeach

install_data(
  'scalable' / 'org.xfce.panel.weather.svg',
  install_dir: get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)

theme_icon_sizes = [22, 48, 128]

names = [
  'cloud',
  'fog',
  'lightcloud',
  'lightcloud-night',
  'lightrain',
  'lightrainsun',
  'lightrainsun-night',
  'lightrainthunder',
  'lightrainthundersun',
  'lightrainthundersun-night',
  'nodata',
  'partlycloud',
  'partlycloud-night',
  'rain',
  'rainthunder',
  'sleet',
  'sleetsun',
  'sleetsun-night',
  'sleetsunthunder',
  'sleetsunthunder-night',
  'sleetthunder',
  'snow',
  'snowsun',
  'snowsun-night',
  'snowsunthunder',
  'snowsunthunder-night',
  'snowthunder',
  'sun',
  'sun-night',
]

liquid_themes = [
  'liquid',
  'liquid-dark',
]

foreach theme : liquid_themes
  foreach size : theme_icon_sizes
    foreach name : names
      install_data(
        theme / '@0@'.format(size) / '@0@.png'.format(name),
        install_dir: pkgdatadir / 'icons' / theme / '@0@'.format(size),
      )
    endforeach
  endforeach

  install_data(
    theme / 'theme.info',
    install_dir: pkgdatadir / 'icons' / theme,
  )
endforeach

simplistic_names = names + [
  'fog-night',
  'lightrain-night',
  'lightrainthunder-night',
  'rain-night',
  'rainthunder-night',
  'sleet-night',
  'sleetthunder-night',
  'snow-night',
  'snowthunder-night',
]

foreach size : theme_icon_sizes
  foreach name : simplistic_names
    install_data(
      'simplistic' / '@0@'.format(size) / '@0@.png'.format(name),
      install_dir: pkgdatadir / 'icons' / 'simplistic' / '@0@'.format(size),
    )
  endforeach
endforeach

install_data(
  'simplistic' / 'theme.info',
  install_dir: pkgdatadir / 'icons' / 'simplistic',
)
