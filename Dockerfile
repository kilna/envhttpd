ARG ENVHTTPD_BUSYBOX_VERSION=1.36.1

FROM alpine AS build

ARG ENVHTTPD_TINI_VERSION=0.19.0

LABEL org.opencontainers.image.title="envhttpd"
LABEL org.opencontainers.image.authors="Kilna Anthony <kilna@kilna.com>"
LABEL org.opencontainers.image.url="https://hub.docker.com/r/kilna/envhttpd"
LABEL org.opencontainers.image.source="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.documentation="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.description="A dockerized HTTPD for serving environment variables"
LABEL org.opencontainers.image.licenses="MIT"

RUN set -e -x; \
    tini_base=https://github.com/krallin/tini/releases/download/; \
    busybox_base=https://busybox.net/downloads/; \
    apk update; \
    apk add dpkg; \
    mkdir -p /opt/bin; \
    arch="$(dpkg --print-architecture | rev | cut -d- -f1 | rev)"; \
    wget "$tini_base/v$ENVHTTPD_TINI_VERSION/tini-static-$arch" \
      -O /opt/bin/tini

FROM busybox:${ENVHTTPD_BUSYBOX_VERSION}-uclibc

COPY --chown=root:root --chmod=644 ./etc/* /etc/
COPY --chown=root:root --chmod=555 ./envhttpd /bin/
COPY --from=build --chown=root:root --chmod=555 /opt/bin/* /bin/

RUN set -e -x; \
    mkdir -p /var/www; \
    chown www-data:www-data /var/www

USER www-data
WORKDIR /var/www

ENTRYPOINT [ "/bin/tini", "--" ]

CMD [ "/bin/envhttpd" ]

