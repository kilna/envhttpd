VERSION=''
PLATFORMS=linux/amd64,linux/386,linux/arm64,linux/arm/v6,linux/arm/v7

build:
	docker buildx build . -t kilna/envhttpd:build --progress plain \
	  --platform $(PLATFORMS) --push

release:
	if [ "$(VERSION)" == '' ]; then
	  echo "Must run make as 'VERSION=vX.Y.Z make release'" >&2; exit 1
	fi
	desc=""
