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

Goes in `~/.Xdefaults`. Here's an example:

    ! geometry and font
    URxvt.geometry: 100x25
    URxvt.cursorColor: #ff6600
    URxvt.font: -misc-tamzen-medium-r-normal--16-116-100-100-c-80-iso8859-1
    URxvt.boldFont: -misc-tamzen-bold-r-normal--16-116-100-100-c-80-iso8859-1

    ! to disable iso14755 mode
    URxvt.iso14755: false
    URxvt.iso14755_52: false

    ! transparent background    
    URxvt.inheritPixmap: true
    URxvt.transparent: true
    URxvt.tintColor: #00ffff
    URxvt.shading: 30

    ! scrolling 
    URxvt.saveLines: 10000
    URxvt.scrollBar: false
    URxvt.scrollstyle: plain
    URxvt.internalBorder: 0
    URxvt.externalBorder: 0

    ! enable URL highlight plugin 
    URxvt.perl-ext-common: default,matcher
    URxvt.urlLauncher: xdg-open
    URxvt.colorUL: #ff6600
