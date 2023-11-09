prompt_sources = [
  'main.c',
  'prompt-application.c',
  'prompt-close-dialog.c',
  'prompt-container-provider.c',
  'prompt-container.c',
  'prompt-find-bar.c',
  'prompt-flatpak-provider.c',
  'prompt-host-container.c',
  'prompt-palette.c',
  'prompt-podman-provider.c',
  'prompt-preferences-list-item.c',
  'prompt-preferences-window.c',
  'prompt-process.c',
  'prompt-profile-editor.c',
  'prompt-profile-menu.c',
  'prompt-profile-row.c',
  'prompt-profile.c',
  'prompt-run-context.c',
  'prompt-settings.c',
  'prompt-shortcut-accel-dialog.c',
  'prompt-shortcut-row.c',
  'prompt-shortcuts.c',
  'prompt-tab.c',
  'prompt-tab-monitor.c',
  'prompt-terminal.c',
  'prompt-title-dialog.c',
  'prompt-unix-fd-map.c',
  'prompt-user.c',
  'prompt-util.c',
  'prompt-window-dressing.c',
  'prompt-window.c',
  'eggshell.c',
]

prompt_enum_headers = [
  'prompt-process.h',
  'prompt-profile.h',
  'prompt-settings.h',
  'prompt-tab.h',
]

prompt_sources += gnome.mkenums_simple('prompt-enums',
            sources: prompt_enum_headers,
  identifier_prefix: 'Prompt',
      symbol_prefix: 'prompt',
)

prompt_sources += gnome.compile_resources('prompt-resources',
  'prompt.gresource.xml',
  c_name: 'prompt'
)

configure_file(
          input: 'org.gnome.Prompt.gschema.xml.in',
         output: '@0@.gschema.xml'.format(app_id),
  configuration: config_h,
    install_dir: get_option('datadir') / 'glib-2.0' / 'schemas',
)

prompt_deps = [
  dependency('gio-unix-2.0'),
  dependency('gtk4', version: '>= 4.12'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('json-glib-1.0', version: '>= 1.6'),
  dependency('vte-2.91-gtk4'),
  cc.find_library('m', required: false),
]

prompt_c_args = [
  release_link_args,
]

prompt_include_directories = [
  include_directories('.'),
  include_directories('..'),
]

executable('prompt', prompt_sources,
         dependencies: prompt_deps,
              install: true,
               c_args: prompt_c_args,
  include_directories: prompt_include_directories,
)
