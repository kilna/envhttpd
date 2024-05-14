FROM alpine AS build

ARG ENVHTTPD_BUSYBOX_VERSION=1.36.1
ARG ENVHTTPD_TINI_VERSION=0.19.0

LABEL org.opencontainers.image.authors="kilna@kilna.com"
LABEL org.opencontainers.image.url="https://hub.docker.io/r/kilna/envhttpd"
LABEL org.opencontainers.image.source="https://github.com/kilna/envhttpd"

COPY ./busybox.config /build/busybox/.config

WORKDIR /build

RUN set -e -x; \
    tini_base=https://github.com/krallin/tini/releases/download/; \
    busybox_base=https://busybox.net/downloads/; \
    apk update; \
    apk add alpine-sdk dpkg; \
    mkdir -p /opt/bin; \
    arch="$(dpkg --print-architecture | rev | cut -d- -f1 | rev)"; \
    wget "$tini_base/v$ENVHTTPD_TINI_VERSION/tini-static-$arch" \
      -O /opt/bin/tini; \
    wget "$busybox_base/busybox-$ENVHTTPD_BUSYBOX_VERSION.tar.bz2" \
      -O /build/busybox.tar.bz2; \
    cd busybox; \
    tar -x -v --strip-components=1 -f ../busybox.tar.bz2; \
    make; \
    ./make_single_applets.sh; \
    make V=1 install

# Switch to the scratch image
FROM scratch

COPY --chown=root:root --chmod=644 ./etc/* /etc/
COPY --chown=root:root --chmod=555 ./envhttpd /bin/
COPY --from=build --chown=root:root --chmod=555 /opt/bin/* /bin/
COPY --from=build --chown=root:root --chmod=555 /opt/sbin/* /sbin/

USER www-data
WORKDIR /var/www

ENTRYPOINT [ "/bin/tini", "--" ]

CMD [ "/bin/envhttpd" ]

