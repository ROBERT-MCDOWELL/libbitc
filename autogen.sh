#!/bin/sh
srcdir="$(dirname $0)"
checkTool="$( which glibtoolize 2>/dev/null)"
cd "$srcdir"

if [ "$checkTool" != "" ]; then
        glibtoolize && autoreconf --install --force
else
        libtoolize && autoreconf --install --force
fi
