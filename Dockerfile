FROM alpine AS build

ARG ENVHTTPD_BUSYBOX_VERSION=1.36.1
ARG TARGETARCH

LABEL org.opencontainers.image.title="envhttpd"
LABEL org.opencontainers.image.authors="Kilna Anthony <kilna@kilna.com>"
LABEL org.opencontainers.image.url="https://hub.docker.com/r/kilna/envhttpd"
LABEL org.opencontainers.image.source="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.documentation="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.description="A dockerized HTTPD for serving environment variables"
LABEL org.opencontainers.image.licenses="MIT"

COPY ./busybox.config /build/busybox/.config

COPY --chmod=755 /install.sh /build/install.sh

WORKDIR /build

RUN /build/install.sh

# Switch to the scratch image
FROM scratch

COPY --chown=root:root --chmod=644 ./etc/* /etc/
COPY --chown=root:root --chmod=555 ./envhttpd /bin/
COPY --from=build --chown=root:root --chmod=555 /build/install/ /

USER www-data
WORKDIR /var/www

ENTRYPOINT [ "/bin/tini", "--" ]

CMD [ "/bin/envhttpd" ]

