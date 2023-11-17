# We build the agent with older versions of GLib required as well
# as various linker symbols so that we can run the agent, unmodified
# on older enterprise linux distributions. Currently, prompt-agent
# can run on CentOS 7.

PROMPT_AGENT_MIN_GLIB_MAJOR = 2
PROMPT_AGENT_MIN_GLIB_MINOR = 56
PROMPT_AGENT_MIN_GLIB_VERSION = '@0@.@1@'.format(PROMPT_AGENT_MIN_GLIB_MAJOR, PROMPT_AGENT_MIN_GLIB_MINOR)

prompt_agent_deps = [
  dependency('gio-2.0', version: '>= @0@'.format(PROMPT_AGENT_MIN_GLIB_VERSION)),
  dependency('gio-unix-2.0', version: '>= @0@'.format(PROMPT_AGENT_MIN_GLIB_VERSION)),
  dependency('json-glib-1.0'),
]

prompt_agent_sources = [
  'prompt-agent.c',
  'prompt-agent-impl.c',
  'prompt-agent-util.c',
  'prompt-container-provider.c',
  'prompt-podman-container.c',
  'prompt-process-impl.c',
  'prompt-run-context.c',
  'prompt-session-container.c',
  'prompt-unix-fd-map.c',
]

prompt_agent_ipc = gnome.gdbus_codegen('prompt-agent-ipc',
           sources: 'org.gnome.Prompt.Agent.xml',
  interface_prefix: 'org.gnome.Prompt.',
         namespace: 'PromptIpc',
)

prompt_agent_link_args = []

prompt_agent_c_args = [
  release_link_args,
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(PROMPT_AGENT_MIN_GLIB_MAJOR, PROMPT_AGENT_MIN_GLIB_MINOR),
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_@0@_@1@'.format(PROMPT_AGENT_MIN_GLIB_MAJOR, PROMPT_AGENT_MIN_GLIB_MINOR),
]

libc_compat = true
if target_machine.cpu_family() == 'x86_64'
  prompt_agent_c_args += ['-include', 'x86_64/force_link_glibc_2.17.h']
elif target_machine.cpu_family() == 'x86'
  prompt_agent_c_args += ['-include', 'x86/force_link_glibc_2.17.h']
else
  libc_compat = false
endif

if libc_compat
  config_h.set10('LIBC_COMPAT', libc_compat)
  prompt_agent_link_args += ['-Wl,--wrap=__libc_start_main']
endif

executable('prompt-agent', prompt_agent_sources + prompt_agent_ipc,
         dependencies: prompt_agent_deps,
              install: true,
          install_dir: get_option('libexecdir'),
               c_args: prompt_agent_c_args,
            link_args: prompt_agent_link_args,
  include_directories: include_directories('..'),
)
