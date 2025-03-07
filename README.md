# Cute
Cute is a GNOME Terminal Emulator for Windows based on Ptyxis

# Ptyxis

Ptyxis is a terminal for GNOME with first-class support for containers.

Flatpak is the intended and preferred distribution mechanism.


## Feature Requests and Design Changes

Ptyxis uses the issue tracker for tracking engineering defects only.

Feature requests tend to be long, drawn out, and never fully solvable.
Therefore we request that you file an issue with the
[Whiteboards project](https://gitlab.gnome.org/Teams/Design/whiteboards/)
and progress towards designing the feature you'd like to see. I will not design
the feature for you.

The outcome of the design process should be a specification which includes:

 * How the feature should work
 * How the feature should not work
 * How the feature interacts with the existing features
 * If any existing features should be changed or removed
 * Any necessary migration strategies for existing users
 * UI mock-ups (if necessary)
 * How the feature should be tested
 * What are the risks and security gotchas involved?
 * Who is going to implement the feature

After that process has completed, you may file an issue referencing it.


## FAQ

### Notifications Don't Work

Make sure your supported shell sources `/etc/profile.d/vte.sh` which contains
the necessary shell-side components for notification plumbing for VTE. It
ensures the proper OSC sequence is emitted at the right points.

### My Favorite Container Technology is not Supported

See the sources in `agent/` and implement a `PtyxisContainerProvider` and
`PtyxisContainer` for your technology. It needs to know how to query them
and spawn processes (with the appropriate PTY) within the container.

It should also update the list of available containers when they change
without polling (a `GFileMonitor` to requery is a suitable solution).

## I Have a Problem with My GPU

While the underlying terminal emulator (VTE) supports GPU rendering, we have
very little to do with that from this application. Bugs filed here will almost
certainly end up in either the GNOME/vte or GNOME/gtk projects.

## I Have Weird Font Rendering

Currently (as of GTK 4.16) GTK has an oddity with rendering at 200% scale when
the rounding for hinting is different between 1x and 2x scaling. This can cause
overly constrained clipping. This must be addressed in GTK.


## Authors Note

Ptyxis is a combination of work I've done across multiple projects. The
container abstractions came from a prototype I did many years ago for GNOME
Builder around a "Terminal Workspace". The motivation to finally push this
over the line was some recent (Fall '23) work on making VTE feel
fast/low-latency on GTK 4 with GNOME Shell as a Wayland compositor.

It's probably not the fastest thing out there, but it should be close enough
that other features are of more value.  Some of those include robust
clipboard support, drag-n-drop integration, scrollbars, kinetic scrolling,
tabs with overviews, a modern interface, configurable color palettes, menus,
accessibility, encrypted scrollback buffers, you get the idea.


## Installation

You can install the Nightly version of Ptyxis using the gnome-nightly
Flatpak repository.

```sh
flatpak install --user --from https://nightly.gnome.org/repo/appstream/org.gnome.Ptyxis.Devel.flatpakref
```


## Building

Ptyxis is designed for Flatpak.

The easiest way to build and test your changes to Ptyxis is by opening the
project in GNOME Builder and clicking the Run button.

Otherwise, it is built as any other meson-based project.


## Features

 * We recognize that there are a number of situations where users have
   particular needs for integrating with external systems. Preferences allow
   for tweaking a number of these compatibility options.
 * Built-in support for profiles allows for many of those preferences to
   be tweaked on a more fine-grained basis.
 * You can specify a default container per-profile or to inherit the current
   terminal's container. Recently, toolbox gained support to always emit the
   necessary escape sequence. But you'll need a very recent version of
   toolbox for this. Podman does not inherently emit the escape sequence and
   therefore discovery of current container may be limited.
 * Flatpak'able without losing features! This works through the use of a
   specialized `ptyxis-agent` that runs on the host to provide Ptyxis the
   ability to create PTY devices which integrate better with containers.
   Care is taken for this agent to run on systems as far back as CentOS 7.
 * A number of palettes are provided with native support for light/dark mode
   and can update with your desktop style preference.
 * Due to how `ptyxis-agent` works, we can do various foreground process
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
 * "Single application mode" allows you to run an application in a ptyxis
   instance but just for that command. Useful when integrating with
   .desktop files containing Terminal=true.
 * Pinned tab profile/container/directory are saved across sessions so that
   you can get back to your projects quickly. Currently this is restricted
   to a single window but may change in the future.
 * `ptyxis-agent` will automatically create systemd scopes for your tab
   when available to help reduce the chances that the whole app would be
   killed by the OOM killer instead of the tab taking up resources.
 * An terminal inspector to help you debug issues when writing applications
   for the terminal.


## Container Support

The highlevel is that we should be able to support:

 * Native "user session" even when in Flatpak
 * Podman/Toolbox/Distrobox (See caveat below)
 * JHBuild (See caveat below)

I'd certainly love to see support for systemd-nspawn if that is something
you're interested in and are aware of the mechanics to make that work.


### Toolbox/Podman/Distrobox

Until very recently `toolbox` would only emit the escape sequences notifying
the terminal emulator of the current container when on certain Fedora hosts.
This has been changed but is not available in all distributions yet.

Podman inherently does not emit the specific escape sequence.

Distrobox does not appear to emit the escape sequence.

Of course, you can get a similar effect by doing something similar to what
the `jhbuild` example below does.


### JHBuild

If you use `jhbuild` for GNOME development, you can make that work by adding
this to your `.bashrc` to have your jhbuild session persisted between tabs
automatically. Otherwise you'll need to create a jhbuild tab using the menu
item which is less convenient.

```sh
if [ -t 1 -a x$UNDER_JHBUILD != x ]; then
    printf "\033]777;container;push;%s;jhbuild\033\\" "JHBuild"
    function pop_container {
        printf "\033]777;container;pop;;\033\\"
    }
    trap pop_container EXIT
fi
```


## How it Works

Ptyxis has a small `ptyxis-agent` that manages PTY, PID tracking, and
container monitoring. This is spawned on the host-system similar to how you
would execute a new shell. However, it communicates with the UI via D-Bus
serialization over a point-to-point `socketpair()`.

It is able to do this because `ptyxis-agent`, on `x86` and `x86_64` will
restrict its `glibc` usage to what is available on fairly old Linux
installs such as CentOS 7. It does use GLib as well for D-Bus communication
but is limited to what is available with Flatpak on the targetted platforms.

Doing so allows Ptyxis to support Flatpak natively while also traversing
container PTY and PID namespaces in an efficient manner.

If your host system does not support GLibc (such as Alpine or PostmarketOS) or
lacks a functioning dynamic linker from the session (NixOS) then Ptyxis will
detect this and fallback to running `ptyxis-agent` within the Flatpak sandbox.
This will naturally restrict the capability of some features.


## Screenshots

![Shows a menu with available containers](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/containers-menu.png "Containers are automatically discovered and displayed")

![A dialog showing options for behavior change such as scrollback, bell, and more](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/change-behavior.png "Many behaviors of the terminal may be tweaked to user preference")

![A dialog allowing the user to rename a tab](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/rename-tab.png "You may rename a tab by providing a prefix to the title")

![A menu showing the options for light/dark/follow system style](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/integrated-dark-mode.png "Palettes provide an integrated dark mode")

![All terminals displayed as a grid](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/tab-overview.png "You can see an overview of your tabs at any time")

![A window containing a list of shortcuts which may be remapped to new values](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/shortcut-editing.png "Many shortcuts may be remapped to user preference")

![A dialog providing appearance preferences such as color palette](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/palette-selector.png "Built-in color palettes provide both dark and light mode variants")

![A dialog providing per-profile overrides](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/edit-profile.png "Profiles allow for overriding a number of features such as default container")

![The terminal showing an overlay when the column or row size changes](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/columns-and-rows.png "A column and row size indicator is displayed when resizing the window")

![A scarf is displayed on the top of a window indicating root access](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/sudo-tracking.png "The terminal will remind you when you're root on the local system")

![A scarf is displayed on the top of a window indicating remote SSH access](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/ssh-tracking.png "The terminal will remind you when you're on a remote system")

![A terminal with a partially transparent background](https://gitlab.gnome.org/chergert/ptyxis/-/raw/main/data/screenshots/transparency.png "Transparency support is available for the daring")
