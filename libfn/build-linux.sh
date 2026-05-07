#!/bin/sh
set -e

echo "Building..."

if [ -n "$1" ]; then
    if [ -x "$1/gcc" ]; then
        export PATH="$1:$PATH"
    elif [ -x "$1" ]; then
        export PATH="$(dirname "$1"):$PATH"
    else
        echo "ERROR: gcc not found at '$1' or '$1/gcc'"
        exit 1
    fi
fi

if ! command -v gcc >/dev/null 2>&1; then
    echo "ERROR: gcc not found. Pass gcc bin path as argument."
    echo "Example:"
    echo "./build-linux.sh /usr/bin"
    exit 1
fi

rm -f libfn.so main

echo "[1/2] Building libfn.so..."
gcc -fPIC -shared libfn.c -o libfn.so -lm

echo "[2/2] Building main..."
gcc main.c -o main -ldl -lm

echo "Building completed"
