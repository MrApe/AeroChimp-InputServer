# AeroChimp-InputServer

This is a small tool, which captures keyboard input of different devices and sends it to a RESTful interface. It is used as part of a score-input system for aerobic gymnastics competitions. 

## Dependencies ##

* a UNIX with input event files (`/dev/input/*`)
* some sort of c compiler
* [libcurl](http://curl.haxx.se)
* libuuid
		sudo apt-get install uuid-dev

## Install ##

There is no install. It's just the executable. Build it with 
		
		gcc -I/usr/include/curl -o inputserver main.c -L/usr/lib/arm-linux-gnueabihf -lcurl -luuid

### OpenWRT cross

If you want to build the executable for OpenWRT (or any other platform) you might want to cross compile it. The following is essentially borrowed from [here](https://wiki.openwrt.org/doc/devel/crosscompile).

1. Add your cross compiler to the PATH
		
		PATH=$PATH:(your toolchain/bin directory here)
		export PATH

2. Set the STAGING_DIR variable to yout toolchain directory. This is the one starting with 'toolchain' in the `staging_dir`.

		STAGING_DIR=/Volumes/workspace/openwrt/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2
		export STAGING_DIR

3. Build it using your cross compiler. **Be sure to issue this command from the target directory**
		
		cd /Volumes/workspace/openwrt/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2
		mipsel-openwrt-linux-gcc ~/Dev/AeroChimp-InputServer/main.c -o Inputserver -Iusr/include/curl -Iusr/include -lcurl -luuid

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

		sudo ./inputserver SERVER PORT UID

* **SERVER** is the IP of the AeroChimp Server where scores are posted to
* **PORT** is the respective port of the AeroChimp Server
* **UID** is an (max.) 8-char unique identifier of the system to handle multiple inputserver devices

An example launch call may look like the following:

		sudo ./inputserver 192.168.0.314 31415 PI1

## Usage ##

You may register your device at the server under your score with the following command:

        ...[PANEL][JUDGE][NUMBER]

Where..

* **[PANEL]** is the number of your panel (eg. 1)
* **[JUDGE]** is the one letter abbreviation of your judge type. The following codes are used (***NOTICE: the letters are partly derived from the german judge names***)
    * `a` for Artistic
    * `b` or `e` for Execution
    * `s` or `d` for Difficulty
    * `o` for Superior (from german ***Oberkampfrichter***)
* **[NUMBER]*** is your judge number (eg. 3)

An example registration call would look like:

        ...1e2

This call would register the devices to the execution judge 2 of judges panel 1.
