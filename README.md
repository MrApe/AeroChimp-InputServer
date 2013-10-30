# AeroChimp-InputServer

This is a small tool, which captures keyboard input of different devices and sends it to a RESTful interface. It is used as part of a score-input system for aerobic gymnastics competitions. 

## Dependencies ##

* a UNIX with input event files (`/dev/input/*`)
* some sort of c compiler

## Install ##

There is no install. It's just the executable. Build it with `gcc -o inputserver main.c -pthread`.

## Launch ##

Get a list of your input event files with:

		ls -la /dev/input/

You may check also `/dev/input/by-path/` and `/dev/input/by-id/` to find out which devices you need.

Copy the device urls to the file events.txt:

		/dev/input/event1
		/dev/input/event4

Launch the server with

		sudo ./inputserver 
