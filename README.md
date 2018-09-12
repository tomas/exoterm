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

    URxvt.geometry: 100x25
    URxvt.cursorColor: #ff6600

    ! for transparent background    
    URxvt.inheritPixmap: true
    URxvt.transparent: true
    URxvt.tintColor: #00ffff
    URxvt.shading: 30

