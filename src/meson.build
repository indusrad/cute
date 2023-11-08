capsule_sources = [
  'main.c',
  'capsule-application.c',
  'capsule-close-dialog.c',
  'capsule-container-provider.c',
  'capsule-container.c',
  'capsule-find-bar.c',
  'capsule-flatpak-provider.c',
  'capsule-host-container.c',
  'capsule-palette.c',
  'capsule-podman-provider.c',
  'capsule-preferences-list-item.c',
  'capsule-preferences-window.c',
  'capsule-profile-editor.c',
  'capsule-profile-menu.c',
  'capsule-profile-row.c',
  'capsule-profile.c',
  'capsule-run-context.c',
  'capsule-settings.c',
  'capsule-shortcut-accel-dialog.c',
  'capsule-shortcut-row.c',
  'capsule-shortcuts.c',
  'capsule-tab.c',
  'capsule-terminal.c',
  'capsule-title-dialog.c',
  'capsule-unix-fd-map.c',
  'capsule-user.c',
  'capsule-util.c',
  'capsule-window-dressing.c',
  'capsule-window.c',
  'eggshell.c',
]

capsule_enum_headers = [
  'capsule-profile.h',
  'capsule-settings.h',
  'capsule-tab.h',
]

capsule_sources += gnome.mkenums_simple('capsule-enums',
            sources: capsule_enum_headers,
  identifier_prefix: 'Capsule',
      symbol_prefix: 'capsule',
)

capsule_sources += gnome.compile_resources('capsule-resources',
  'capsule.gresource.xml',
  c_name: 'capsule'
)

configure_file(
          input: 'org.gnome.Capsule.gschema.xml.in',
         output: '@0@.gschema.xml'.format(app_id),
  configuration: config_h,
    install_dir: get_option('datadir') / 'glib-2.0' / 'schemas',
)

capsule_deps = [
  dependency('gio-unix-2.0'),
  dependency('gtk4', version: '>= 4.12'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('json-glib-1.0', version: '>= 1.6'),
  dependency('vte-2.91-gtk4'),
  cc.find_library('m', required: false),
]

capsule_c_args = [
  release_link_args,
]

capsule_include_directories = [
  include_directories('.'),
  include_directories('..'),
]

executable('capsule', capsule_sources,
         dependencies: capsule_deps,
              install: true,
               c_args: capsule_c_args,
  include_directories: capsule_include_directories,
)
