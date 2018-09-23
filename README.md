# Build Instructions

Install the following if on Ubuntu or use equivalent on other OSes:

    sudo apt install libperl-dev libxft-dev

Clone libev (Or libevent) and libptytty into working directory of this repo.

Configure with:

    ./configure

Compile with

    make
    sudo make install

See `README.configure` for more details and configuration options.

# Configuration

Goes in `~/.Xdefaults`. Just make sure to run `xrdb ~/X.defaults` after you update it.

Here's an example:

    ! geometry and cursor
    URxvt.geometry: 100x25
    URxvt.cursorColor: #ff6600

    ! font: tamzen 8x16
    URxvt.font: -misc-tamzen-medium-r-normal--16-116-100-100-c-80-iso8859-1
    URxvt.boldFont: -misc-tamzen-bold-r-normal--16-116-100-100-c-80-iso8859-1

    ! or terminus 8x16
    URxvt.font: -xos4-terminus-medium-r-normal--16-160-72-72-c-80-iso10646-1
    URxvt.boldFont: -xos4-terminus-bold-r-normal--16-160-72-72-c-80-iso10646-1

    ! to disable iso14755 mode
    URxvt.iso14755: false
    URxvt.iso14755_52: false

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

    ! enable URL highlight plugin 
    URxvt.perl-ext-common: default,matcher
    URxvt.urlLauncher: xdg-open
    URxvt.colorUL: #ff6600

    ! scrolling 
    URxvt.saveLines: 10000
    URxvt.scrollBar: false
    URxvt.scrollstyle: plain
    URxvt.internalBorder: 0
    URxvt.externalBorder: 0

    ! more scrolling
    URxvt.jumpScroll: true                                                   
    URxvt.skipScroll: true
    URxvt.mouseWheelScrollPage: true

    URxvt.secondaryScroll: true
    URxvt.secondaryScreen: false

    ! do not scroll with output
    URxvt.scrollTtyOutput: false

    ! scroll in relation to buffer (with mouse scroll or Shift+Page Up)
    URxvt.scrollWithBuffer: true

    ! scroll back to the bottom on keypress
    URxvt.scrollTtyKeypress: true

