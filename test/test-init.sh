#!/bin/sh
# Test that envhttpd exits cleanly when run as PID 1 (init) and receives SIGTERM.
# docker stop sends SIGTERM to PID 1; we expect exit code 0 from our handler.
set -e -u

IMAGE="${IMAGE:-kilna/envhttpd}"
TAG=build
CONTAINER=envhttpd-init-test

docker rm -f "$CONTAINER" 2>/dev/null || true
trap 'docker rm -f "$CONTAINER" 2>/dev/null || true' EXIT

echo "Starting container (envhttpd as PID 1)..."
docker run -d --name "$CONTAINER" "${IMAGE}:${TAG}" -x '*' -D

sleep 1

echo "Sending SIGTERM via docker stop (timeout 3s)..."
docker stop -t 3 "$CONTAINER"

exitcode=$(docker inspect "$CONTAINER" --format '{{.State.ExitCode}}')
if [ "$exitcode" = "0" ]; then
  echo "OK: envhttpd exited with 0 on SIGTERM (init behavior)"
else
  echo "Error: expected exit code 0, got $exitcode (137=SIGKILL, 143=unhandled SIGTERM)"
  exit 1
fi
