# Usage: make release VERSION=1.2.3

# Project information is stored in PROJECT.yml
# Releases can only be performed from entries in CHANGELOG.yml:
#  For main branch:
#    must not have a dash in the version (bare X.Y.Z)
#    will result in docker latest tag if the version is highest non-prerelease
#    will result in a docker edge tag if it is the highest version overall
#  For other branches:
#    must have a dash-suffix in the version (X.Y.Z-description)
#    are marked as prerelease in github
#    must have a branch in the CHANGELOG.yml entry that matches the git branch
#    will result in a docker edge tag if it is the highest version overall

IMAGE=$(shell yq e .image PROJECT.yml)
SHORT_DESC=$(shell yq e .description PROJECT.yml)
PLATFORMS=$(shell yq e '.platforms | join(" ")' PROJECT.yml)
STATIC_PLATFORMS=$(shell yq e '.static_platforms | join(" ")' PROJECT.yml)
STATIC_OUT ?= out

VERSIONS=$(shell yq e 'keys | .[]' CHANGELOG.yml \
                  | sed -e 's/^v//' | sort -t. -k1,1n -k2,2n -k3,3n)
PRERELEASE=$(shell echo $(VERSIONS) | tr ' ' '\n' | tail -n 1)
LATEST=$(shell echo $(VERSIONS) | tr ' ' '\n' | grep -vF '-' | tail -n 1)
EDGE=$(if $(filter $(LATEST)-%,$(PRERELEASE)),$(LATEST),$(PRERELEASE))

CLEAN=$(shell echo $(VERSION) | sed -e 's/^v//')
VER=$(if $(CLEAN),$(CLEAN),$(if $(eq $(GIT_BRANCH),main),$(LATEST),$(EDGE)))
MAJOR=$(shell echo $(VER) | cut -d. -f1)
MINOR=$(shell echo $(VER) | cut -d. -f1-2)

GIT_BRANCH=$(shell git rev-parse --abbrev-ref HEAD)

OS=$(shell uname -s | tr '[:upper:]' '[:lower:]')
ARCH=$(shell uname -m | sed -e 's/aarch64/arm64/;s/x86_64/amd64/;s/i686/386/')
export DOCKER_PLATFORM=linux/$(ARCH)
PUSHRM_REPO = https://github.com/christian-korneck/docker-pushrm
PUSHRM_URL = $(PUSHRM_REPO)/releases/download/v1.9.0/docker-pushrm_$(OS)_$(ARCH)

build:
	docker build -t $(IMAGE):build --progress plain .

build-clean:
	docker build -t $(IMAGE):build --progress plain --no-cache .

build-all:
	docker buildx build -t $(IMAGE):build --progress plain --push \
	  --platform $(subst $(shell echo " "),$(shell echo ","),$(PLATFORMS)) .
	docker pull $(IMAGE):build

build-all-clean:
	docker buildx build -t $(IMAGE):build --progress plain --push --no-cache \
	  --platform $(subst $(shell echo " "),$(shell echo ","),$(PLATFORMS)) .
	docker pull $(IMAGE):build

clean:
	docker-compose -f test/docker-compose.test.yml down || true
	docker-compose -f test/docker-compose.test.yml rm -f || true
	docker-compose -f test/docker-compose.platform.yml down || true
	docker-compose -f test/docker-compose.platform.yml rm -f || true
	docker rmi -f $(IMAGE):build $(IMAGE)-test:build || true

test: build
	set -e -x; \
	cd test; \
	docker-compose -f docker-compose.test.yml up --abort-on-container-exit

test-all-nobuild:
	set -e -x; \
	cd test; \
	for platform in $(PLATFORMS); do \
	  export DOCKER_PLATFORM=$$platform; \
	  docker-compose -f docker-compose.platform.yml up \
	    --abort-on-container-exit; \
	done

test-all: build-all test-all-nobuild

test-all-clean: build-all-clean test-all-nobuild

run:
	docker run --env-file test/test.env -p 8111:8111 $(IMAGE):build \
	  -x '*' -i '*_ME' -x EXCLUDE_ME -D

build-run: build run

info:
	@echo IMAGE: $(IMAGE)
	@echo SHORT_DESC: $(SHORT_DESC)
	@echo PLATFORMS: $(PLATFORMS)
	@echo VERSION: $(VER)
	@echo MINOR: $(MINOR)
	@echo MAJOR: $(MAJOR)
	@echo VERSIONS: $(VERSIONS)
	@echo PRERELEASE: $(PRERELEASE)
	@echo LATEST: $(LATEST)
	@echo EDGE: $(EDGE)
	@echo GIT_BRANCH: $(GIT_BRANCH)
	@echo OS: $(OS)
	@echo ARCH: $(ARCH)
	@echo PUSHRM_URL: $(PUSHRM_URL)

check_version:
	@if ! echo '$(VERSIONS)' | tr ' ' '\n' | grep -qF '$(VER)'; then \
	  echo "Error: Must provide a version number as found in CHANGELOG.yml" >&2; \
	  exit 1; \
	fi
	@ver_branch="$$(yq e '.["v$(VER)"].branch // "main"' CHANGELOG.yml)"; \
	if [ "$$ver_branch" != "$(GIT_BRANCH)" ]; then \
	  echo "Error: CHANGELOG.yml branch $$ver_branch for v$(VER)" \
	       "mismatches current branch $(GIT_BRANCH)" >&2; \
	  exit 1; \
	fi
	@if [ "$$(yq e '.["v$(VER)"].description' CHANGELOG.yml)" = "" ]; then \
	  echo "Error: CHANGELOG.yml is missing description for v$(VER)" >&2; \
	  exit 1; \
	fi

check_git_status:
	@if [ "$$(git status --porcelain | wc -l)" -gt 0 ]; then \
	  echo "Git working copy is not up to date" >&2; \
	  exit 1; \
	fi
	@if ! git diff-index --quiet HEAD --; then \
	  echo "Git working tree is not clean" >&2; \
	  exit 1; \
	fi
	@ver_is_prerelease=no; \
	case "$(VER)" in \
	  *-*) ver_is_prerelease=yes ;; \
	esac; \
	if [ "$$ver_is_prerelease" = "yes" ] && [ "$(GIT_BRANCH)" = "main" ]; then \
	  echo "release must be on feature branch for prerelease" >&2; \
	  exit 1; \
	fi; \
	if [ "$$ver_is_prerelease" = "no" ] && [ "$(GIT_BRANCH)" != "main" ]; then \
	  echo "release must be on main branch for non-prerelease" >&2; \
	  exit 1; \
	fi
	@changelog_branch="$$(yq e '.["v$(VER)"].branch // "main"' CHANGELOG.yml)"; \
	if [ "$$changelog_branch" != "$(GIT_BRANCH)" ]; then \
	  echo "git branch $(GIT_BRANCH) does not match branch $$changelog_branch" \
	       "in CHANGELOG.yml for prerelease $(VER)" >&2; \
	  exit 1; \
	fi

github_unrelease:
	gh release delete -y v$(VER)
	git push --delete origin v$(VER)

github_release: check_version check_git_status
	gh release delete -y v$(VER) || true
	git push --delete origin v$(VER) || true
	gh release create --title v$(VER) --target $(GIT_BRANCH) \
	  --notes "$$(yq e '.["v$(VER)"].description' CHANGELOG.yml)" \
	  $(if $(eq $(GIT_BRANCH),main),,--prerelease) \
	  v$(VER)

docker_install_pushrm:
	@[ -e ~/.docker/cli-plugins/docker-pushrm ] && exit 0
	curl -o ~/.docker/cli-plugins/docker-pushrm -fsSL $(PUSHRM_URL)
	chmod +x ~/.docker/cli-plugins/docker-pushrm

docker_release_edge:
	docker buildx imagetools create -t $(IMAGE):edge $(IMAGE):$(VER)

docker_release_latest:
	docker buildx imagetools create -t $(IMAGE):latest $(IMAGE):$(VER)
	docker pushrm $(IMAGE) -s "$(SHORT_DESC)"
	docker buildx imagetools create -t $(IMAGE):latest $(IMAGE):$(VER)
	docker buildx imagetools create -t $(IMAGE):$(MINOR) $(IMAGE):$(VER)
	docker buildx imagetools create -t $(IMAGE):$(MAJOR) $(IMAGE):$(VER)

docker_release: check_version test-all-clean build-all-clean docker_install_pushrm
	docker buildx imagetools create --tag $(IMAGE):$(VER) $(IMAGE):build
	docker pull $(IMAGE):$(VER)
	@[ "$(VER)" == "$(EDGE)" ] && $(MAKE) docker_release_edge VERSION=$(VER)
	@[ "$(VER)" == "$(LATEST)" ] && $(MAKE) docker_release_latest VERSION=$(VER)

release: github_release docker_release

.PHONY: static

static:
	rm -rf $(STATIC_OUT)
	mkdir -p $(STATIC_OUT)
	set -e; \
	for platform in $(STATIC_PLATFORMS); do \
	  suffix=$${platform//\//-}; \
	  dest="$(STATIC_OUT)/$$suffix"; \
	  docker buildx build --progress plain \
			--platform $$platform --target static-artifact \
			--output type=local,dest=$$dest .; \
	  mv $$dest/envhttpd-static $(STATIC_OUT)/envhttpd-$$suffix; \
	  rm -rf $$dest; \
	done

.PHONY: \
  build \
  build-all \
  static \
  test \
  test-all \
  info \
  release \
  github_release \
  github_unrelease \
  check_version \
  check_git_status \
  docker_release \
  docker_release_edge \
  docker_release_latest \
  docker_install_pushrm
