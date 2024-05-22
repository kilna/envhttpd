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

check_version:
	if [ $(VERSION) == '' ]; then \
	  echo "Must run make as 'VERSION=vX.Y.Z make release'" >&2; \
	  exit 1; \
	fi
	if [ $(DESC) == '' ]; then \
	  echo "You need a version entry for $(VERSION) in CHANGELOG.yml" >&2; \
	  exit 1; \
	fi

check_git_status:
	if [ $(GIT_STATUS) != *'up to date'*'working tree clean' ]; then \
	  echo "`make release` needs up to date, clean git working tree" >&2; \
	  exit 1; \
	fi
	if [ $(VERSION) != *-* && $(GIT_BRANCH) != 'main' ]; then \
	  echo "`make release` must be on main branch for non-prerelease" >&2; \
	  exit 1; \
	fi

docker_release: check_version build
	docker pull $(IMAGE):build
	if [ $(VERSION) == *-* ]; then \
	  docker tag $(IMAGE):build $(IMAGE):edge; \
	  docker push $(IMAGE):edge; \
	else \
	  docker tag $(IMAGE):build $(IMAGE):edge; \
	  docker push $(IMAGE):edge; \
	  docker tag $(IMAGE):build $(IMAGE):latest; \
	  docker push $(IMAGE):latest; \
	fi
	docker tag $(IMAGE):build $(IMAGE):$(DOCKER_VER)
	docker push $(IMAGE):$(DOCKER_VER)

github_release: check_git_status check_version
	gh release delete -y $(VERSION) || true
	git push --delete origin $(VERSION) || true
	if [ $(VERSION) == *-* ]; then \
	  gh release create -n "$desc" --title $(VERSION) $(VERSION) \
	    --target $(GIT_BRANCH) --prerelease; \
	else \
	  gh release create -n $(DESC) --title $(VERSION) $(VERSION); \
	fi

release: github_release docker_release

