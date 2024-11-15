FROM alpine:latest

RUN apk add --no-cache curl

COPY . /test/

RUN chmod +x /test/test.sh

ENV SERVER_HOST=localhost
ENV SERVER_PORT=8111

CMD [ "/test/test.sh" ]
