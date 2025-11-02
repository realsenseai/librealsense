#!/bin/sh

set -e

cd "$(dirname "$0")"

if [ ! -d m4 ]; then
    mkdir m4
fi
exec autoreconf -ivf
