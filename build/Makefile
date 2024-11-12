.PHONY: all clean destroy static

all: bin/tini bin/envhttpd bin/envhttpd-static

scratch-install: bin/tini bin/envhttpd etc/passwd etc/group
	rm -rfv Makefile *.h *.c
	mkdir -p -v var/www
	mv -v icon.png style.css var/www/
	chown -R 33:33 var/www

bin/tini:
	mkdir -p -v bin lib
	cp -v $$(which tini) $@
	cp -v $$(ldd $@ | awk '{print $$3}' | grep -v '^$$') lib

template.h: template.html
	echo -n 'const char* template =(' >template.h
	sed 's/\\/\\\\/g; s/"/\\"/g; s/^\(.*\)$$/  \"\1\\n\"/' $< >>template.h
	echo ');' >>template.h

bin/envhttpd: envhttpd.c template.h
	mkdir -p -v bin lib
	gcc -O2 $< -o $@
	strip $@
	cp -v $$(ldd $@ | awk '{print $$3}' | grep -v '^$$') lib

bin/envhttpd-static: envhttpd.c style.h
	mkdir -p -v bin
	gcc -O2 -static $< -o $@
	strip $@

clean:
	rm -tfv bin/ lib/ style.h

etc/passwd etc/group:
	mkdir -p -v etc
	echo "root:x:0:" > etc/group
	echo "www-data:x:33:www-data" >> etc/group
	echo "root:x:0:0:root:/root:/sbin/nologin" > etc/passwd
	echo "www-data:x:33:33:www-data:/var/www:/sbin/nologin" >> etc/passwd
