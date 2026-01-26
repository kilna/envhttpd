.PHONY: all clean scratch-install

all: bin/envhttpd

scratch-install: bin/envhttpd
	mkdir -p -v var/www
	chown -R 33:33 var/www
	mkdir -p -v etc
	echo "root:x:0:" > etc/group
	echo "www-data:x:33:www-data" >> etc/group
	echo "root:x:0:0:root:/root:/sbin/nologin" > etc/passwd
	echo "www-data:x:33:33:www-data:/var/www:/sbin/nologin" >> etc/passwd

src/template.h: src/template.html
	echo -n 'const char* template =(' >$@
	sed 's/\\/\\\\/g; s/"/\\"/g; s/^\(.*\)$$/  \"\1\\n\"/' $< >>$@
	echo ');' >>$@

src/icon.h: icon.png
	echo 'const unsigned char icon_png[] = {' >$@
	od -A n -t x1 -v $< | awk '{for(i=1;i<=NF;i++) if($$i!="") printf " 0x%s,\n", $$i}' >>$@
	echo '};' >>$@
	echo 'const unsigned int icon_png_len = '$$(wc -c < $<)';' >>$@

bin/envhttpd: src/envhttpd.c src/template.h src/icon.h
	mkdir -p -v bin
	gcc -O2 -static $< -o $@
	strip $@

clean:
	rm -tfv bin/ src/*.h var/ etc/
