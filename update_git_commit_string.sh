#!/bin/bash
VERSIONFILE=src/commit.h

COMMIT=$(git log -n 1 | head -n 1 | sed -E 's/.* (.{10}).*/\1/' )
if test -n "$(git status -s)"
then
    COMMIT="$COMMIT (+changes)"
fi
if ! grep -q "$COMMIT" $VERSIONFILE
then
    echo "upating $VERSIONFILE"
    rm -f $VERSIONFILE
    echo "#pragma once"               >> $VERSIONFILE
    echo "#define COMMIT \"$COMMIT\"" >> $VERSIONFILE
fi

