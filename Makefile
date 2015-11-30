.PHONY: build install uninstall clean
inputserver: main.c
	gcc -I/usr/include/curl -o inputserver main.c -L/usr/lib/arm-linux-gnueabihf -lcurl -luuid

build: inputserver

all: build install

install: build
	sudo cp inputserver /usr/local/bin/
	sudo cp -f 101-judge-pad-added.rules /etc/udev/rules.d/
	sudo cp startJudgePad.sh /usr/local/bin/
	sudo cp heartbeat /usr/local/bin/ 

uninstall:
	sudo rm -f /usr/local/bin/inputserver
	sudo rm -f /etc/udev/rules.d/101-judge-pad-added.rules
	sudo rm -f /usr/local/bin/startJudgePad.sh
	sudo rm -f /usr/local/bin/heartbeat

clean:
	rm inputserver
