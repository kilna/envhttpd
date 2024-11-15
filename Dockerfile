FROM alpine AS build

ARG TARGETARCH

LABEL org.opencontainers.image.title="envhttpd"
LABEL org.opencontainers.image.authors="Kilna Anthony <kilna@kilna.com>"
LABEL org.opencontainers.image.url="https://hub.docker.com/r/kilna/envhttpd"
LABEL org.opencontainers.image.source="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.documentation="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.description="HTTP Server for Environment Variables"
LABEL org.opencontainers.image.licenses="LicenseRef-MIT-Link-License"

RUN apk add --no-cache gcc musl-dev build-base tini

WORKDIR /envhttpd/

COPY . /envhttpd/

RUN make -f src/Makefile scratch-install

FROM scratch

COPY --from=build /envhttpd/bin /bin
COPY --from=build /envhttpd/lib /lib
COPY --from=build /envhttpd/etc /etc
COPY --from=build /envhttpd/var /var

USER www-data

ENTRYPOINT [ "/bin/tini", "--", "envhttpd" ]

EXPOSE 8111

CMD []
