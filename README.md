# Prompt

Prompt is a terminal for GNOME with first-class support for containers.

## Features

 * We recognize that there are a number of situations where users have
   particular needs for integrating with external systems. Preferences allow
   for tweaking a number of these compatibility options.
 * Built-in support for profiles allows for many of those preferences to
   be tweaked on a more fine-grained basis.
 * You can specify a default container per-profile or to inherit the current
   terminals container. At the moment of writing, this is only supported on
   Fedora hosts, but will be extended to other host operating systems soon
   once changes land in VTE, toolbox, and elsewhere.
 * Flatpak'able without looking features! This works through the use of a
   specialized `prompt-agent` that runs on the host to provide Prompt the
   ability to create PTY devices which integrate better with containers.
   Care is taken for this agent to run on systems as far back as CentOS 7.
 * A number of palettes are provided with native support for light/dark mode
   and can update with your desktop style preference.
 * Due to how `prompt-agent` works, we can do various foreground process
   tracking for things like sudo or SSH without significant overhead.
 * Tab overview using modern libadwaita features.
 * Transparency support does exist but must be tweaked manually using GSettings
   until better transitions can be implemented in libadwaita.
 * Use of libadwaita "Toasts" to notify the user when the clipboard has been
   updated as well as other operations.
 * There are various tweaks to how VTE draws which are performed by rewriting
   the retained render tree. This allows for appropriate padding around the
   terminal while still keeping scrollback from looking cut off.

## Distribution Notes

Prompt is built in a way that can be shipped as a Flatpak and still provide a
number of modern features we want from a container-aware terminal. Currently
this relies on a number of VTE patches which are in the `build-aux/` directory.

In the future, this may be reduced, but at the moment both Prompt and tools
like "toolbox" need to be aware of what escape-sequences to use to propagate
container information to the application.

