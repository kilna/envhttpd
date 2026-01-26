.PHONY: all scratch-install clean

all: bin/envhttpd bin/envhttpd-static

scratch-install: bin/envhttpd etc/passwd etc/group
	rm -rfv Makefile *.h *.c
	mkdir -p -v var/www
	mv -v icon.png LICENSE var/www/
	chown -R 33:33 var/www

src/template.h: src/template.html
	echo -n 'const char* template =(' >$@
	sed 's/\\/\\\\/g; s/"/\\"/g; s/^\(.*\)$$/  \"\1\\n\"/' $< >>$@
	echo ');' >>$@

bin/envhttpd: src/envhttpd.c src/template.h
	mkdir -p -v bin lib
	gcc -O2 $< -o $@
	strip $@
	cp -v $$(ldd $@ | awk '{print $$3}' | grep -v '^$$') lib

bin/envhttpd-static: src/envhttpd.c src/template.h
	mkdir -p -v bin
	gcc -O2 -static $< -o $@
	strip $@

clean:
	rm -tfv bin/ lib/ src/template.h var/ etc/

etc/passwd etc/group:
	mkdir -p -v etc
	echo "root:x:0:" > etc/group
	echo "www-data:x:33:www-data" >> etc/group
	echo "root:x:0:0:root:/root:/sbin/nologin" > etc/passwd
	echo "www-data:x:33:33:www-data:/var/www:/sbin/nologin" >> etc/passwd
