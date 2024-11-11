FROM alpine AS build

ARG TARGETARCH

LABEL org.opencontainers.image.title="envhttpd"
LABEL org.opencontainers.image.authors="Kilna Anthony <kilna@kilna.com>"
LABEL org.opencontainers.image.url="https://hub.docker.com/r/kilna/envhttpd"
LABEL org.opencontainers.image.source="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.documentation="https://github.com/kilna/envhttpd"
LABEL org.opencontainers.image.description="A dockerized HTTPD for serving environment variables"
LABEL org.opencontainers.image.licenses="MIT"

RUN apk add --no-cache gcc musl-dev build-base tini

WORKDIR /build

COPY build/ /build/

RUN make scratch-install

FROM scratch

COPY --from=build /build/ /

USER www-data

ENTRYPOINT [ "/bin/tini", "--", "envhttpd" ]

EXPOSE 8111

CMD []
