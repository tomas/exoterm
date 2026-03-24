# Exoterm

A fork of [rxvt-unicode](http://software.schmorp.de/pkg/rxvt-unicode.html) (urxvt), kept in sync with upstream and extended with additional features.

## Features beyond upstream urxvt

- **Native tabs** — tab bar with mouse support, fully managed in C (no Perl)
- **Split panes** — horizontal/vertical pane splitting
- **Minimap** — scrollback overview panel with viewport indicator
- **Sixel image support** — inline image rendering via DCS sixel sequences
- **True 24-bit color** — full RGB888 support beyond the standard 256-color cube
- **Native URL detection and click-to-open** — no Perl extension required
- **Drag-and-drop** — XDND protocol support for dropping files/text into the terminal
- **Native searchable scrollback** — incremental search bar implemented in C
- **Settings pane** — graphical settings panel accessible at runtime
- **Right-click context menu** — context-sensitive actions on selected text
- **Shift+Enter** — sends line feed (`\n`) separately from Enter (`\r`), useful in some TUI applications
- **Prompt markers** — tracks shell prompt positions for navigation with keyboard shortcuts
- **Auto copy selection** — copies text to clipboard on selection (xterm-style, configurable)
- **Improved input handling** — key encoding closer to xterm for better TUI/Vim/tmux compatibility

### Stuff ported from upstream

- Wide glyph rendering (`ENABLE_WIDE_GLYPHS`) — always enabled in this build
- SGR mouse mode (1006), extended mouse reporting (1015), DECRQM (mode query)
- Bracketed paste mode
- Background image support with compositing

## Build

Install dependencies (Ubuntu/Debian):

    sudo apt install libxpm-dev libxft-dev libxrender-dev libxmu-dev libstartup-notification0-dev

Initialize submodules:

    git submodule init
    git submodule update

Configure and build:

    ./configure
    make
    sudo make install

Run `./configure -h` to see all options. See `README.configure` for details.

## Configuration

Configuration goes in `~/.Xdefaults`. Run `xrdb ~/.Xdefaults` after changes.

Example:

    URxvt.loginShell: true

    ! geometry and cursor
    URxvt.foreground: #fff
    URxvt.geometry: 100x25
    URxvt.cursorColor: #ff6600

    ! font (Terminus 8x16)
    URxvt.font: -xos4-terminus-medium-r-normal--16-160-72-72-c-80-iso10646-1
    URxvt.boldFont: -xos4-terminus-bold-r-normal--16-160-72-72-c-80-iso10646-1

    ! real transparency (requires compositor)
    URxvt.depth: 32
    URxvt.background: rgba:0000/0000/0800/c800

    ! borders
    URxvt.internalBorder: 10
    URxvt.externalBorder: 0
    URxvt.borderLess: false

    ! scrollback
    URxvt.saveLines: 10000
    URxvt.scrollBar: false
    URxvt.jumpScroll: true
    URxvt.skipScroll: true
    URxvt.scrollTtyOutput: false
    URxvt.scrollWithBuffer: true
    URxvt.scrollTtyKeypress: true

## Colors

Example using a Dracula-based palette (in `~/.Xdefaults`):

    URxvt.color0:  #000000
    URxvt.color8:  #4D4D4D
    URxvt.color1:  #FF5555
    URxvt.color9:  #FF798C
    URxvt.color2:  #50FAB2
    URxvt.color10: #5AF78E
    URxvt.color3:  #F1FA8C
    URxvt.color11: #F4F99D
    URxvt.color4:  #BD93F9
    URxvt.color12: #CAA9FA
    URxvt.color5:  #FF79C6
    URxvt.color13: #FF9AF4
    URxvt.color6:  #8BE9FD
    URxvt.color14: #9AEDFE
    URxvt.color7:  #BFBFBF
    URxvt.color15: #E6E6E6

## Misc

### Installing Terminus Powerline fonts

    wget https://github.com/powerline/fonts/raw/master/Terminus/PCF/ter-powerline-x16b.pcf.gz
    wget https://github.com/powerline/fonts/raw/master/Terminus/PCF/ter-powerline-x16n.pcf.gz

    fonts_dir=/usr/share/fonts/X11/misc
    sudo cp ter-powerline-x16b.pcf.gz ter-powerline-x16n.pcf.gz $fonts_dir
    sudo mkfontdir $fonts_dir
    xset +fp $fonts_dir

Make sure `$fonts_dir` is listed in the `FontPath` section of `/etc/X11/xorg.conf`.

### Borderless window in JWM

    <Group>
      <Class>Exoterm</Class>
      <Option>noborder</Option>
      <Option>notitle</Option>
    </Group>
