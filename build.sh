#!/bin/bash

: ${CC=gcc}
: ${JOBS=4}

$CC $@ -fPIC -shared ruby/ruby.c `pkg-config --cflags ruby` `pkg-config --libs ruby` -llua5.4 -o ruby.so
