OS=$(shell uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(shell uname -m | sed -e 's/aarch64/arm64/;s/x86_64/amd64/;s/i686/386/')
CFLAGS=-I/usr/include/cjson  -L/usr/lib -lmicrohttpd -Wl,-Bstatic -lmicrohttpd -Wl,-Bdynamic

.PHONY: all scratch-install clean

all: bin/tini bin/envhttpd bin/envhttpd-static

scratch-install: bin/tini bin/envhttpd etc/passwd etc/group
	rm -rfv Makefile *.h *.c
	mkdir -p -v var/www
	mv -v icon.png LICENSE var/www/
	chown -R 33:33 var/www

bin/tini:
	mkdir -p -v bin lib
	cp -v $$(which tini) $@
	cp -v $$(ldd $@ | awk '{print $$3}' | grep -v '^$$') lib

src/template.h: src/template.html
	echo -n 'const char* template =(' >$@
	sed 's/\\/\\\\/g; s/"/\\"/g; s/^\(.*\)$$/  \"\1\\n\"/' $< >>$@
	echo ');' >>$@

bin/envhttpd: src/envhttpd.c src/template.h
	mkdir -p -v bin lib
	gcc -O2 -static $< -o $@ $(CFLAGS)
	strip $@
	cp -v $$(ldd $@ | awk '{print $$3}' | grep -v '^$$') lib

bin/envhttpd-static: envhttpd.c src/template.h
	mkdir -p -v bin
	gcc -O2 -static $< -o envhttpd-static-$(OS)-$(ARCH) $(CFLAGS)
	strip $@

clean:
	rm -tfv bin/ lib/ src/template.h var/ etc/

etc/passwd etc/group:
	mkdir -p -v etc
	echo "root:x:0:" > etc/group
	echo "www-data:x:33:www-data" >> etc/group
	echo "root:x:0:0:root:/root:/sbin/nologin" > etc/passwd
	echo "www-data:x:33:33:www-data:/var/www:/sbin/nologin" >> etc/passwd
