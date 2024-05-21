FROM alpine AS build

ARG ENVHTTPD_BUSYBOX_VERSION=1.36.1

LABEL org.opencontainers.image.title="envhttpd"
LABEL org.opencontainers.image.authors="Kilna Anthony <kilna@kilna.com>"
LABEL org.opencontainers.image.url="https://hub.docker.com/r/kilna/envhttpd"
LABEL org.opencontainers.image.source="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.documentation="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.description="A dockerized HTTPD for serving environment variables"
LABEL org.opencontainers.image.licenses="MIT"

COPY ./busybox.config /build/busybox/.config

WORKDIR /build

ARG TARGETARCH

RUN set -e -x; \
    tini_base=https://github.com/krallin/tini/releases/download/; \
    busybox_base=https://busybox.net/downloads/; \
    apk update; \
    mkdir -p /opt/bin; \
    case "$TARGETARCH" in \
      amd64|arm64*) \
        echo "Building busybox on known-working platform $TARGETARCH"; \
        apk add alpine-sdk dpkg; \
        wget "$busybox_base/busybox-$ENVHTTPD_BUSYBOX_VERSION.tar.bz2" \
          -O /build/busybox.tar.bz2; \
        cd busybox; \
        tar -x --strip-components=1 -f ../busybox.tar.bz2; \
        make; \
        ./make_single_applets.sh; \
        make V=1 install;; \
      *) \
        echo "Installing busybox on platform; Todo: build on $TARGETARCH"; \
        apk add busybox-extras; \
        cd /opt/bin; \
        cp -a /bin/busybox* ./; \
        for exe in cat cut env mkdir rev sh tr; do ln -s busybox $exe; done; \
        ln -s busybox-extras httpd;; \
    esac; \
    mkdir -p /opt/lib; \
    cp -a /lib/*musl* /opt/lib; \
    apk add tini; \
    cp /sbin/tini /opt/bin/

# Switch to the scratch image
FROM scratch

COPY --chown=root:root --chmod=644 ./etc/* /etc/
COPY --chown=root:root --chmod=555 ./envhttpd /bin/
COPY --from=build --chown=root:root --chmod=555 /opt/ /

#RUN ls -laR /bin /etc /lib /sbin

USER www-data
WORKDIR /var/www

ENTRYPOINT [ "/bin/tini", "--" ]

CMD [ "/bin/envhttpd" ]

