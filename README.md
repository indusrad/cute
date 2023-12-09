# Prompt

Prompt is a terminal for GNOME with first-class support for containers.


## Authors Note

Prompt came out of my work to make VTE much faster on modern Linux desktops
which run GTK 4 and a Wayland-based compositor such as GNOME Shell. It is
competitively fast in this area, although unlikely the fastest. It supports a
number of features due to VTE in exchange. Some of those include robust
clipboard support, drag-n-drop integration, scrollbars, kinetic scrolling, tabs
with overviews, a modern interface, configurable color palettes, menus, you get
the idea.


## Installation

You can install the Nightly version of Prompt using the gnome-nightly
Flatpak repository.

```sh
flatpak install --user --from https://nightly.gnome.org/repo/appstream/org.gnome.Prompt.Devel.flatpakref
```


## Building

Prompt is designed for Flatpak.

If you wish to install Prompt on your host system, you'll need some patches
applied to VTE (in build-aux/) until an upstream solution is provided for
container tracking. We do expect this in the not-too-distant future.

The easiest way to build and test your changes to Prompt is by opening the
project in GNOME Builder and clicking the Run button.

Otherwise, it is built as any other meson-based project.


## Features

 * We recognize that there are a number of situations where users have
   particular needs for integrating with external systems. Preferences allow
   for tweaking a number of these compatibility options.
 * Built-in support for profiles allows for many of those preferences to
   be tweaked on a more fine-grained basis.
 * You can specify a default container per-profile or to inherit the current
   terminal's container. At the moment of writing, this is only supported on
   Fedora hosts, but will be extended to other host operating systems soon
   once changes land in VTE, toolbox, and elsewhere.
 * Flatpak'able without losing features! This works through the use of a
   specialized `prompt-agent` that runs on the host to provide Prompt the
   ability to create PTY devices which integrate better with containers.
   Care is taken for this agent to run on systems as far back as CentOS 7.
 * A number of palettes are provided with native support for light/dark mode
   and can update with your desktop style preference.
 * Due to how `prompt-agent` works, we can do various foreground process
   tracking for things like sudo or SSH without significant overhead. This
   requires some improvements in various container systems which is in the
   process of merging at the time of writing.
 * Tab overview using modern libadwaita features.
 * Transparency support does exist but must be tweaked manually using GSettings
   until better transitions can be implemented in libadwaita.
 * Use of libadwaita "Toasts" to notify the user when the clipboard has been
   updated as well as other operations.
 * There are various tweaks to how VTE draws which are performed by rewriting
   the retained render tree. This allows for appropriate padding around the
   terminal while still keeping scrollback from looking cut off.
 * User customizable keyboard shortcuts.
 * "Single application mode" allows you to run an application in a prompt
   instance but just for that command. Useful when integrating with
   .desktop files containing Terminal=true.
 * Pinned tab profile/container/directory are saved across sessions so that
   you can get back to your projects quickly. Currently this is restricted
   to a single window but may change in the future.
 * `prompt-agent` will automatically create systemd scopes for your tab
   when available to help reduce the chances that the whole app would be
   killed by the OOM killer instead of the tab taking up resources.


## Container Support

The highlevel is that we should be able to support:

 * Native "user session" even when in Flatpak
 * Podman/Toolbox/Distrobox (See caveat below)
 * JHBuild (See caveat below)

I'd certainly love to see support for systemd-nspawn if that is something
you're interested in and are aware of the mechanics to make that work.


### Toolbox/Podman/Distrobox

Currently `toolbox` knows how to emit the appropriate escape sequence under
certain conditions (namely being run on a Fedora host). But when the support
for this uses new, more generic API in VTE we will switch to that.

Until then, opening new tabs with the same container will only work if you are
on a Fedora host such as Fedora Workstation or Silverblue.


### JHBuild

If you use `jhbuild` for GNOME development, you can make that work by adding
this to your `.bashrc` to have your jhbuild session persisted between tabs
automatically. Otherwise you'll need to create a jhbuild tab using the menu
item which is less convenient.

```sh
if [ x$UNDER_JHBUILD != x ]; then
    printf "\033]777;container;push;%s;jhbuild\033\\" "JHBuild"
    function pop_container {
        printf "\033]777;container;pop;;\033\\"
    }
    trap pop_container EXIT
fi
```


## How it Works

Prompt has a small `prompt-agent` that manages PTY, PID tracking, and
container monitoring. This is spawned on the host-system similar to how you
would execute a new shell. However, it communicates with the UI via D-Bus
serialization over a point-to-point `socketpair()`.

It is able to do this because `prompg-agent`, on `x86` and `x86_64` will
restrict it's `glibc` usage to what is available on fairly old Linux
installs such as CentOS 7. It does use GLib as well for D-Bus communication
but is limited to what is available with Flatpak on the targetted platforms.

Doing so allows Prompt to support Flatpak natively while also traversing
container PTY and PID namespaces in an efficient manner.


## Screenshots

![Shows a menu with available containers](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/containers-menu.png "Containers are automatically discovered and displayed")

![A dialog showing options for behavior change such as scrollback, bell, and more](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/change-behavior.png "Many behaviors of the terminal may be tweaked to user preference")

![A dialog allowing the user to rename a tab](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/rename-tab.png "You may rename a tab by providing a prefix to the title")

![A menu showing the options for light/dark/follow system style](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/integrated-dark-mode.png "Palettes provide an integrated dark mode")

![All terminals displayed as a grid](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/tab-overview.png "You can see an overview of your tabs at any time")

![A window containing a list of shortcuts which may be remapped to new values](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/shortcut-editing.png "Many shortcuts may be remapped to user preference")

![A dialog providing appearance preferences such as color palette](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/palette-selector.png "Built-in color palettes provide both dark and light mode variants")

![A dialog providing per-profile overrides](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/edit-profile.png "Profiles allow for overriding a number of features such as default container")

![The terminal showing an overlay when the column or row size changes](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/columns-and-rows.png "A column and row size indicator is displayed when resizing the window")

![A scarf is displayed on the top of a window indicating root access](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/sudo-tracking.png "The terminal will remind you when you're root on the local system")

![A scarf is displayed on the top of a window indicating remote SSH access](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/ssh-tracking.png "The terminal will remind you when you're on a remote system")

![A terminal with a partially transparent background](https://gitlab.gnome.org/chergert/prompt/-/raw/main/data/screenshots/transparency.png "Transparency support is available for the daring")
