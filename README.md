# Prompt

Prompt is a terminal for GNOME with first-class support for containers.


## Installation

You can install the Nightly version of Prompt using the gnome-nightly
Flatpak repository.

```sh
flatpak install --user --from https://nightly.gnome.org/repo/appstream/org.gnome.Prompt.Devel.flatpakref
```


## Authors Note

Prompt came out of my work to make VTE much faster on modern Linux desktops
which run GTK 4 and a Wayland-based compositor such as GNOME Shell. It is
competitively fast in this area, although unlikely the fastest. It supports a
number of features due to VTE in exchange. Some of those include robust
clipboard support, drag-n-drop integration, scrollbars, tabs, a modern
interface, configurable color palettes, menus, tab overviews, you get the idea.


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


## Container Support

Currently `toolbox` knows how to emit the appropriate escape sequence under
certain conditions (namely being run on a Fedora host). But when the support
for this uses new, more generic API in VTE we will switch to that.

If you use `jhbuild` for GNOME development, you can make that work by adding
this to your `.bashrc` to have your jhbuild session persisted between tabs.

```sh
if [ x$UNDER_JHBUILD != x ]; then
    printf "\033]777;container;push;%s;jhbuild\033\\" "JHBuild"
    function pop_container {
        printf "\033]777;container;pop;;\033\\"
    }
    trap pop_container EXIT
fi
```


## Distribution Notes

Prompt is built in a way that can be shipped as a Flatpak and still provide a
number of modern features we want from a container-aware terminal. Currently
this relies on a number of VTE patches which are in the `build-aux/` directory.

In the future, this may be reduced, but at the moment both Prompt and tools
like "toolbox" need to be aware of what escape-sequences to use to propagate
container information to the application.


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
