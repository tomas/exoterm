<p align="center">
  <img src="https://raw.githubusercontent.com/tomas/exoterm/master/doc/screenshot.png" alt="Exoterm Window" />
</p>

# Exoterm

A fork of [rxvt-unicode](http://software.schmorp.de/pkg/rxvt-unicode.html) (urxvt), kept in sync with upstream and improved with a few tricks. X11 only.

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

All this, while still using less than 20 MB of RAM. :)

### Stuff ported from upstream

- Wide glyph rendering (`ENABLE_WIDE_GLYPHS`) — always enabled in this build
- SGR mouse mode (1006), extended mouse reporting (1015), DECRQM (mode query)
- Bracketed paste mode
- Background image support with compositing

## Why not just use ____?

Because a terminal should not require over 50 MB of RAM, period.
I want mine to open up in less than a second. And urxvt delivers exactly that.

Plus, it doesn't rely on the GPU for rendering, so it can be used virtually anywhere.

## How can I use this in Wayland?

You can't, sorry.

But I hear that [foot](https://codeberg.org/dnkl/foot) is pretty lightweight and minimal.

## Install

No binary packages available at the moment. But you can build it in a second or two.

## How to build

First, install dependencies. These are

- libxrender-dev 
- libxmu-dev 
- libxpm-dev 
- libxft-dev (for xft fonts, not needed if you use bitmap fonts)
- libstartup-notification0-dev

For Ubuntu/Debian, the command would be:

    sudo apt install libxpm-dev libxft-dev libxrender-dev libxmu-dev libstartup-notification0-dev

Then, initialize submodules:

    git submodule init
    git submodule update

Configure and build:

    ./configure
    make
    sudo make install

Run `./configure -h` to see all options. See `README.configure` for details. 
For instance, to enable true 24 bit color support and disable xft, you'd run:

    ./configure --enable-24-bit-color --disable-xft

## Settings

Exoterm provides a GUI for changing (some of) the available options. In the same
spirit as urxvt, Exoterm reads the values from  `~/.Xdefaults`. You can use either
`Exoterm` or `Urxvt` as a prefix for each option. 

If you edit the `.Xdefaults` file, run `xrdb ~/.Xdefaults` to ensure new values are 
read. Here's an example configuration:

    ! main
    Exoterm.loginShell: true
    Exoterm.foreground: #fff
    Exoterm.geometry: 100x25
    Exoterm.cursorColor: #ff6600

    ! font (Terminus 8x16)
    Exoterm.font: -xos4-terminus-medium-r-normal--16-160-72-72-c-80-iso10646-1
    Exoterm.boldFont: -xos4-terminus-bold-r-normal--16-160-72-72-c-80-iso10646-1

    ! fake transparent background
    ! Exoterm.inheritPixmap: true
    ! Exoterm.transparent: true
    ! Exoterm.tintColor: #00ffff
    ! Exoterm.shading: 30

    ! for real transparency (requires compositor)
    Exoterm.inheritPixmap: false
    Exoterm.transparent: false
    Exoterm.depth: 32
    Exoterm.background: rgba:0000/0000/0800/c800

    ! borders
    Exoterm.internalBorder: 10
    Exoterm.externalBorder: 0
    Exoterm.borderLess: false

    ! scrollback
    Exoterm.saveLines: 10000
    Exoterm.scrollBar: false
    Exoterm.jumpScroll: true
    Exoterm.skipScroll: true
    Exoterm.scrollTtyOutput: false
    Exoterm.scrollWithBuffer: true
    Exoterm.scrollTtyKeypress: true

## Colors

Example using a Dracula-based palette (in `~/.Xdefaults`):

    Exoterm.color0:  #000000
    Exoterm.color8:  #4D4D4D
    Exoterm.color1:  #FF5555
    Exoterm.color9:  #FF798C
    Exoterm.color2:  #50FAB2
    Exoterm.color10: #5AF78E
    Exoterm.color3:  #F1FA8C
    Exoterm.color11: #F4F99D
    Exoterm.color4:  #BD93F9
    Exoterm.color12: #CAA9FA
    Exoterm.color5:  #FF79C6
    Exoterm.color13: #FF9AF4
    Exoterm.color6:  #8BE9FD
    Exoterm.color14: #9AEDFE
    Exoterm.color7:  #BFBFBF
    Exoterm.color15: #E6E6E6

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

## Credits

To the rxvt and unicode-rxvt developers, of course. I'm just a humble servant.

## License

GPLv3, as the original source.
