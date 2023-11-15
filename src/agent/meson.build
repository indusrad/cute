# Require API to be GLib 2.56 for various enterprise Linux
PROMPT_AGENT_MIN_GLIB_MAJOR = 2
PROMPT_AGENT_MIN_GLIB_MINOR = 56
PROMPT_AGENT_MIN_GLIB_VERSION = '@0@.@1@'.format(PROMPT_AGENT_MIN_GLIB_MAJOR, PROMPT_AGENT_MIN_GLIB_MINOR)

prompt_agent_deps = [
  dependency('gio-2.0', version: '>= @0@'.format(PROMPT_AGENT_MIN_GLIB_VERSION)),
  dependency('gio-unix-2.0', version: '>= @0@'.format(PROMPT_AGENT_MIN_GLIB_VERSION)),
]

prompt_agent_sources = [
  'prompt-agent.c',
]

prompt_agent_ipc = gnome.gdbus_codegen('prompt-agent-ipc',
           sources: 'org.gnome.Prompt.Agent.xml',
  interface_prefix: 'org.gnome.Prompt.',
         namespace: 'PromptIpc',
)

prompt_agent_c_args = [
  release_link_args,
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(PROMPT_AGENT_MIN_GLIB_MAJOR, PROMPT_AGENT_MIN_GLIB_MINOR),
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_@0@_@1@'.format(PROMPT_AGENT_MIN_GLIB_MAJOR, PROMPT_AGENT_MIN_GLIB_MINOR),
]

executable('prompt-agent', prompt_agent_sources + prompt_agent_ipc,
         dependencies: prompt_agent_deps,
              install: true,
          install_dir: get_option('libexecdir'),
               c_args: prompt_agent_c_args,
  include_directories: include_directories('../..'),
)
