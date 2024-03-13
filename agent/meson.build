static_link = get_option('static-agent')

ptyxis_agent_deps = [
  dependency('gio-2.0', static: static_link),
  dependency('gio-unix-2.0', static: static_link),
  dependency('json-glib-1.0', static: static_link),
]

ptyxis_agent_sources = [
  'ptyxis-agent.c',
  'ptyxis-agent-impl.c',
  'ptyxis-agent-util.c',
  'ptyxis-container-provider.c',
  'ptyxis-distrobox-container.c',
  'ptyxis-podman-container.c',
  'ptyxis-podman-provider.c',
  'ptyxis-process-impl.c',
  'ptyxis-run-context.c',
  'ptyxis-session-container.c',
  'ptyxis-toolbox-container.c',
  'ptyxis-unix-fd-map.c',
]

ptyxis_agent_ipc = gnome.gdbus_codegen('ptyxis-agent-ipc',
           sources: 'org.gnome.Ptyxis.Agent.xml',
  interface_prefix: 'org.gnome.Ptyxis.',
         namespace: 'PtyxisIpc',
)

ptyxis_agent_link_args = []

ptyxis_agent_c_args = [
  release_link_args,
]

if static_link
  ptyxis_agent_c_args += [
    '-static-libgcc',
    '-static-libstdc++',
    '-pthread',
  ]
  ptyxis_agent_link_args += [
    '-lgcc_eh',
  ]
endif

executable('ptyxis-agent', ptyxis_agent_sources + ptyxis_agent_ipc,
         dependencies: ptyxis_agent_deps,
              install: true,
          install_dir: get_option('libexecdir'),
               c_args: ptyxis_agent_c_args,
            link_args: ptyxis_agent_link_args,
  include_directories: include_directories('..'),
)
