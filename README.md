# Exoterm

An up-to-date fork of rxvt-unicode. In progress but usable.

## Build Instructions

Install the following if on Ubuntu or use equivalent on other OSes:

    sudo apt install libperl-dev libxpm-dev libxft-dev libxrender-dev

Initialize submodules:

    git submodule init
    git submodule update

Configure with:

    ./configure
    
To see all configure options, run:

    ./configure -h

Compile with

    make
    (sudo) make install

See `README.configure` for more details and configuration options.

## Configuration

Goes in `~/.Xdefaults`. Just make sure to run `xrdb ~/.Xdefaults` after you update it.

Here's an example:

    ! main
    ! URxvt.display: :0
    ! URxvt.termName: rxvt-unicode
    ! URxvt.chdir: /home/tomas 
    ! URxvt.locale: true
    URxvt.loginShell: true

    ! geometry and cursor
    URxvt.foreground: #fff
    URxvt.geometry: 100x25
    URxvt.cursorColor: #ff6600
    ! URxvt.skipBuiltinGlyphs: true

    ! font: tamzen 8x16
    URxvt.font: -misc-tamzen-medium-r-normal--16-116-100-100-c-80-iso8859-1
    URxvt.boldFont: -misc-tamzen-bold-r-normal--16-116-100-100-c-80-iso8859-1

    ! or terminus 8x16
    URxvt.font: -xos4-terminus-medium-r-normal--16-160-72-72-c-80-iso10646-1
    URxvt.boldFont: -xos4-terminus-bold-r-normal--16-160-72-72-c-80-iso10646-1

    ! or terminus 8x16 powerline
    URxvt.font: -xos4-terminesspowerline-medium-r-normal--16-160-72-72-c-80-iso10646-1
    URxvt.boldFont: -xos4-terminesspowerline-bold-r-normal--16-160-72-72-c-80-iso10646-1

    ! fake transparent background    
    URxvt.inheritPixmap: true
    URxvt.transparent: true
    URxvt.tintColor: #00ffff
    URxvt.shading: 30

    ! for real transparency
    URxvt.inheritPixmap: false
    URxvt.transparent: false
    URxvt.depth: 32
    URxvt.background: rgba:0000/0000/0800/c800

    ! borders
    URxvt.internalBorder: 0
    URxvt.externalBorder: 0
    URxvt.borderLess: false

    ! scrolling 
    URxvt.saveLines: 10000
    URxvt.scrollBar: false
    URxvt.scrollstyle: plain
    URxvt.jumpScroll: true                                                   
    URxvt.skipScroll: true
    URxvt.mouseWheelScrollPage: false
    URxvt.secondaryScroll: true
    URxvt.secondaryScreen: false

    ! do not scroll with output
    URxvt.scrollTtyOutput: false

    ! scroll in relation to buffer (with mouse scroll or Shift+Page Up)
    URxvt.scrollWithBuffer: true

    ! scroll back to the bottom on keypress
    URxvt.scrollTtyKeypress: true

## Colors

Also are defined in `.Xdefaults`. Here's a custom version of the Dracula theme:

    ! Dracula Xresources palette
    !URxvt.foreground: #F8F8F2
    !URxvt.background: #282A36
    URxvt.color0:     #000000
    URxvt.color8:     #4D4D4D
    URxvt.color1:     #FF5555
    ! URxvt.color9:     #FF6E67
    URxvt.color9:     #FF798C
    ! URxvt.color2:     #50FA7B
    URxvt.color2:     #50FAB2
    URxvt.color10:    #5AF78E
    URxvt.color3:     #F1FA8C
    URxvt.color11:    #F4F99D
    URxvt.color4:     #BD93F9
    URxvt.color12:    #CAA9FA
    URxvt.color5:     #FF79C6
    ! URxvt.color13:    #FF92D0
    URxvt.color13:    #FF9AF4
    URxvt.color6:     #8BE9FD
    URxvt.color14:    #9AEDFE
    URxvt.color7:     #BFBFBF
    URxvt.color15:    #E6E6E6

## Misc tweaks

### Installing fonts

To fetch Terminus Powerline fonts:

    wget https://github.com/powerline/fonts/raw/master/Terminus/PCF/ter-powerline-x16b.pcf.gz
    wget https://github.com/powerline/fonts/raw/master/Terminus/PCF/ter-powerline-x16n.pcf.gz

To install them:

    fonts_dir=/usr/share/fonts/X11/misc
    sudo cp ter-powerline-x16b.pcf.gz ter-powerline-x16n.pcf.gz $fonts_dir
    sudo mkfontdir $fonts_dir
    xset +fp $fonts_dir

And make sure that $fonts_dir is in /etc/X11/xorg.conf in the FontPath section.

### Borderless in JWM

To remove borders if using JWM, add this to your jwmrc:

    <Group>
      <Class>Exoterm</Class>
      <Option>noborder</Option>
      <Option>notitle</Option>
    </Group>

## Interesting forks to follow

 - https://github.com/theKlanc/rxvt-unicode-fa-patched
 - https://github.com/rayburgemeestre/rxvt-unicode
 - https://github.com/RauliL/raxvt
