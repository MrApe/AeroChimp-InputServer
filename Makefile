.PHONY: build install uninstall clean
build:
	gcc -I/usr/include/curl -o inputserver main.c -L/usr/lib/arm-linux-gnueabihf -lcurl -luuid

all: build install

install:
	sudo cp inputserver /usr/local/bin/
	sudo cp -f 101-judge-pad-added.rules /etc/udev/rules.d/

uninstall:
	sudo rm -f /usr/local/bin/inputserver
	sudo rm -f /etc/udev/rules.d/101-judge-pad-added.rules

clean:
	rm inputserver
