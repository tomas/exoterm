#! /bin/sh

if ! [ -e libev/ev++.h ]; then
   cat <<EOF
**
** libev/ directory is missing
**
** you need a checkout of libev (http://software.schmorp.de/pkg/libev.html)
** in the top-level build directory.
**
EOF
   exit 1
fi

if ! [ -e libptytty/ptytty.m4 ]; then
   cat <<EOF
**
** libptytty/ptytty.m4 is missing
**
** please run git submodule init && git submodule update
** in the top-level build directory.
**
EOF
   exit 1
fi

if autoheader && autoconf; then
	rm -rf autom4te.cache
	echo "Now run ./configure"
fi
