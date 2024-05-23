VERSION    = ''
IMAGE      := 'kilna/envhttpd'
PLATFORMS  := linux/amd64,linux/386,linux/arm64,linux/arm/v6,linux/arm/v7
GIT_BRANCH := $(shell git branch | grep -F '*' | cut -f2- -d' ')
GIT_STATUS := $(shell git status)
DOCKER_VER := $(shell echo "$(VERSION)" | sed -e 's/^v//')
DESC       := $(shell yq -r '."$(VERSION)".description' CHANGELOG.yml || true)
VER_REGEX  := 'v[0-9]+\.[0-9]+\.[0-9]+(-[a-z0-9]+)?'

.PHONY: build check_version check_git_status docker_release github_release \
        github_unrelease release

build:
	docker buildx build . -t $(IMAGE):build --progress plain \
	  --platform $(PLATFORMS) --push
	docker pull $(IMAGE):build

check_version:
	ifeq ($(VERSION),$(echo "$(VERSION)" | grep -E "$(VER_REGEX)"))
	  $(error Must run make as \`make release VERSION=vX.Y.Z\`)
	endif
	ifeq ($(DESC), '')
	  $(error You need a version entry for $(VERSION) in CHANGELOG.yml)
	endif

check_git_status:
	$(shell echo "$(GIT_STATUS)" | grep -qF "up to date")) || \
	  $(error Git working copy is not up to date)
	$(shell echo "$(GIT_STATUS)" | grep -qF "working tree clean")) || \
	  $(error Git working tree is not clearn)
	$(shell [ "$(VERSION)" != *-* && "$(GIT_BRANCH)" != 'main' ]) || \
	  $(error \`make release\` must be on main branch for non-prerelease)

docker_release: check_version build
	docker buildx imagetools create --tag $(IMAGE):edge $(IMAGE):build
	[ "$(VERSION)" != *-* ] && \
	  docker buildx imagetools create --tag $(IMAGE):latest $(IMAGE):build
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

