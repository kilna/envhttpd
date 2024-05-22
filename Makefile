VERSION    = ''
IMAGE      := 'kilna/envhttpd'
PLATFORMS  := linux/amd64,linux/386,linux/arm64,linux/arm/v6,linux/arm/v7
GIT_BRANCH := $(shell git branch | grep -F '*' | cut -f2- -d' ')
GIT_STATUS := $(shell git status)
DOCKER_VER := $(shell echo "$(VERSION)" | sed -e 's/^v//')
DESC       := $(shell yq -r '."$(VERSION)".description' CHANGELOG.yml || true)

build:
	docker buildx build . -t $(IMAGE):build --progress plain \
	  --platform $(PLATFORMS) --push
	docker pull $(IMAGE):build

check_version:
	if [ $(VERSION) == '' ]; then \
	  echo "Must run make as 'VERSION=vX.Y.Z make release'" >&2; \
	  false; \
	fi
	if [ "$(DESC)" == '' ]; then \
	  echo "You need a version entry for $(VERSION) in CHANGELOG.yml" >&2; \
	  false; \
	fi

check_git_status:
	if [ "$(GIT_STATUS)" != *'up to date'*'working tree clean'* ]; then \
	  echo "`make release` needs up to date, clean git working tree" >&2; \
	  false; \
	fi
	if [ "$(VERSION)" != *-* && "$(GIT_BRANCH)" != 'main' ]; then \
	  echo "`make release` must be on main branch for non-prerelease" >&2; \
	  false; \
	fi

docker_release: check_version build
	docker buildx imagetools create --tag $(IMAGE):edge $(IMAGE):build
	if [ "$(VERSION)" != *-* ]; then \
	  docker buildx imagetools create --tag $(IMAGE):latest $(IMAGE):build; \
	fi
	docker buildx imagetools create --tag $(IMAGE):$(DOCKER_VER) $(IMAGE):build

github_unrelease:
	gh release delete -y $(VERSION) || true
	git push --delete origin $(VERSION) || true

github_release: check_git_status check_version github_unrelease
	if [ $(VERSION) == *-* ]; then \
	  gh release create -n "$desc" --title $(VERSION) $(VERSION) \
	    --target $(GIT_BRANCH) --prerelease; \
	else \
	  gh release create -n "$(DESC)" --title $(VERSION) $(VERSION); \
	fi

release: check_git_status docker_release github_release

