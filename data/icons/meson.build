icondir = join_paths(get_option('datadir'), 'icons/hicolor')

app_icon = '@0@.svg'.format(app_id)

app_icons = [
  'hicolor/scalable/apps/org.gnome.Prompt.svg',
  'hicolor/scalable/apps/org.gnome.Prompt-symbolic.svg',
]

if get_option('development')
  app_icons += [
    'hicolor/scalable/apps/@0@.svg'.format(app_id),
    'hicolor/scalable/apps/@0@-symbolic.svg'.format(app_id),
  ]
endif

install_data(app_icons, install_dir: join_paths(icondir, 'scalable/apps'))
