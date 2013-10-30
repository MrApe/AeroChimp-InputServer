# AeroChimp-InputServer

This is a small tool, which captures keyboard input of different devices and sends it to a RESTful interface. It is used as part of a score-input system for aerobic gymnastics competitions. 

## Dependencies ##

* a UNIX with input event files (`/dev/input/*`)
* some sort of c compiler
* [libcurl](http://curl.haxx.se)

## Install ##

There is no install. It's just the executable. Build it with 
		
		gcc -I/usr/include/curl -o inputserver main.c -pthread -L/usr/lib/arm-linux-gnueabihf -lcurl

## Launch ##

### Device Discovery 

#### Automatic device discovery

The capture script automatically discovers and saves connected keyboard devices to `events.txt`. Simply call it with

		./capture.sh

#### Get Devices Manually

Get a list of your input event files with:

		ls -la /dev/input/

You may check also `/dev/input/by-path/` and `/dev/input/by-id/` to find out which devices you need.

Copy the device urls to the file events.txt:

		/dev/input/event1
		/dev/input/event4

### Launching the server

Launch the server with

		sudo ./inputserver http://[SERVER]:[PORT]/scoreInput
