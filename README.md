# Tally

A compact task list app for Omarchy with multiple lists, clipboard paste, nested tasks, and animated row transitions.

## Install on another Omarchy unit

After pushing this project to your Git remote, clone it on the other computer:

```sh
git clone <your-repository-url> tally
cd tally
```

Or copy the `todo` project folder there and enter that folder. Then install the build dependencies and Tally:

```sh
sudo pacman -S --needed base-devel qt6-base qt6-declarative qt6-wayland xdg-desktop-portal
./bin/install
```

The installer builds Tally as an Arch package, installs it, and adds it to the app launcher (`Super` + `Space`). Launch it from the app launcher or run `tally` in a terminal.

## Move your lists too

Tally stores its data locally at `~/.local/share/Tally/lists.json`. To move your lists, quit Tally on both computers, copy this file from the old computer, and place it at the same path on the new one:

```sh
mkdir -p ~/.local/share/Tally
cp /path/to/lists.json ~/.local/share/Tally/lists.json
```

For a network copy, run this from the old computer:

```sh
ssh <user>@<new-computer> 'mkdir -p ~/.local/share/Tally'
scp ~/.local/share/Tally/lists.json <user>@<new-computer>:~/.local/share/Tally/lists.json
```

## Shortcuts and controls

| Control | Action |
| --- | --- |
| `Ctrl+N` | Focus task entry |
| `Ctrl+Shift+N` | Create a list |
| Double click a task | Edit task text |
| `+` | Add a subtask when allowed |
| `↑` / `↓` | Move a task through rows and levels |
| `?` | Show shortcuts |
| `◐` | Change Tally's theme |
| `Q` | Quit |

Delete a task to show a short-lived **Undo** banner.

The Tally theme picker offers **Follow system**, **Tally Dark**, and
**Tally Light**. Follow system tracks Omarchy's active palette; the Tally
themes only change this app.
