# envhttpd

**A Dockerized HTTPD to Serve Environment Variables**

[![DockerHub kilna/envhttpd](https://img.shields.io/badge/DockerHub-kilna/envhttpd-blue?logo=docker)](https://hub.docker.com/r/kilna/envhttpd)
[![Docker Image Version](https://img.shields.io/docker/v/kilna/envhttpd?sort=semver)](https://hub.docker.com/r/kilna/envhttpd)
[![Docker Image Size (tag)](https://img.shields.io/docker/image-size/kilna/envhttpd/latest)](https://hub.docker.com/r/kilna/envhttpd)
[![Docker Pulls](https://img.shields.io/docker/pulls/kilna/envhttpd?style=social)](https://hub.docker.com/r/kilna/envhttpd)
[![Docker Stars](https://img.shields.io/docker/stars/kilna/envhttpd?style=social)](https://hub.docker.com/r/kilna/envhttpd)

[![GitHub kilna/envhttpd](https://img.shields.io/badge/GitHub-kilna/envhttpd-green?logo=github)](https://github.com/kilna/envhttpd)
[![GitHub forks](https://img.shields.io/github/forks/kilna/envhttpd?style=social)](https://github.com/kilna/envhttpd/forks)
[![GitHub watchers](https://img.shields.io/github/watchers/kilna/envhttpd?style=social)](https://github.com/kilna/envhttpd/watchers)
[![GitHub Repo stars](https://img.shields.io/github/stars/kilna/envhttpd?style=social)](https://github.com/kilna/envhttpd/stargazers)

A docker image of a web server that delivers environment variables as JSON and
discrete curl-able endpoints; weighing in at less than 1mb, it is ideal to use
as a queryable pod metadata sidecar in Kubernetes, or any situation in which
you need to expose simple data without a lot of overhead.

## Usage

### Local testing

Fire up a container and curl against it...

```
$ docker run -e foo=bar -e yo=bro -p 8111:8111 -d kilna/envhttpd
f92be2e047b1bf68ce7c82a7b8c3358e9b4fceb0e826df6b3952c651d25096e1

# Get all included environment variables as a JSON dictionary
$ curl localhost:8111
{
  "foo": "bar",
  "yo": "bro"
}

# Get a specific value individually
$ curl localhost:8111/foo
bar
```

### Kubernetes

See the [kubernetes example](./kubernetes/) for [pod](./kubernetes/pod/) and
[sidecar deployment](./kubernetes/sidecar/) under the `kubernetes/` folder.

## Configuration

### Environment Variables

You can pass in the following environment variables to configure `envhttpd`

* `ENVHTTPD_PORT`: Which TCP port to run the web server on
* `ENVHTTPD_INCLUDE`: A bar-delimited list of shell glob matching patterns for
  environment variables to be served from the web server. Defaults to `*`, which
  will match any environment variable not on the `ENVHTTPD_EXCLUDE`
* `ENVHTTPD_EXCLUDE`: Likewise, a bar-delimited list of globs for variables to
  exclude from being served. Defaults to excude common system and envhttpd
  variables.

### /etc/envhttpd.conf

Likewise there are counterparts to the above environment variables in
`/etc/envhttpd.conf` called `port`, `include` and `exclude`. You can provide
values by mounting your own configuration file to this path. ENVHTTPD_*
environment variables will override those provided in the conf file.

```
# What port number to run the HTTP server on
port=8111

# Which environment variables to include, shell style globbing, bar delimited
# This list is processed first, a variable must match in order to be output
include=*

# Which environment variables to exclude, shell style globbing, bar delimited
# This list is processed second, a variable cannot match this list to be output
# Default is all base system variables along with ENVHTTPD_*
exclude=HOME|HOSTNAME|PATH|PWD|ENVHTTPD_*
```

## Known Issues / Caveats

* All content (even JSON) is served as text/plain, it is a limitation of
  busybox httpd that only one index file can be created. I may switch to CGI
  at a later point to remedy.
* Special characters will be stripped from environment variable names for
  direct endpoints.

## Author

[Kilna, Anthony](http://github.com/kilna)
[kilna@kilna.com](mailto:kilna@kilna.com)


