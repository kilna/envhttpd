# Build statically linked binary for the container image (musl via Alpine).
FROM alpine:3.23 AS build

ARG TARGETARCH

RUN apk add --no-cache build-base make curl musl-dev

WORKDIR /envhttpd/

COPY . /envhttpd/

RUN make -f src/Makefile scratch-install

FROM scratch AS static-artifact
COPY --from=build /envhttpd/bin/envhttpd /envhttpd-static

FROM scratch

LABEL org.opencontainers.image.title="envhttpd"
LABEL org.opencontainers.image.authors="Kilna Anthony <kilna@kilna.com>"
LABEL org.opencontainers.image.url="https://hub.docker.com/r/kilna/envhttpd"
LABEL org.opencontainers.image.source="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.documentation="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.description="HTTP Server for Environment Variables"
LABEL org.opencontainers.image.licenses="LicenseRef-MIT-Link-License"

COPY --from=build /envhttpd/bin/envhttpd /bin/envhttpd
COPY --from=build /envhttpd/etc /etc
COPY --from=build /envhttpd/var /var

USER www-data

ENTRYPOINT [ "/bin/envhttpd" ]

EXPOSE 8111

CMD []
