#!/bin/bash

OUTPUT="events.txt"

if [ $1 ]; then
OUTPUT="events.txt"
fi

ls -la /dev/input/by-path/ | grep event-kbd | sed 's/.*->\ //g' | sed 's/\.\./\/dev\/input/g' > $OUTPUT
