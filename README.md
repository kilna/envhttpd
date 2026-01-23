# envhttpd <img src="https://raw.github.com/kilna/envhttpd/master/icon.png?raw=true" style="height: 1em; vertical-align: middle; max-height: 32px;" />

**HTTP Server for Environment Variables**

[![DockerHub kilna/envhttpd](https://img.shields.io/badge/DockerHub-kilna/envhttpd-blue?logo=docker)](https://hub.docker.com/r/kilna/envhttpd)
[![Docker Image Version](https://img.shields.io/docker/v/kilna/envhttpd?sort=semver)](https://hub.docker.com/r/kilna/envhttpd)
[![Docker Image Size (tag)](https://img.shields.io/docker/image-size/kilna/envhttpd/latest)](https://hub.docker.com/r/kilna/envhttpd)
[![Docker Pulls](https://img.shields.io/docker/pulls/kilna/envhttpd?style=social)](https://hub.docker.com/r/kilna/envhttpd)
[![Docker Stars](https://img.shields.io/docker/stars/kilna/envhttpd?style=social)](https://hub.docker.com/r/kilna/envhttpd)

[![GitHub kilna/envhttpd](https://img.shields.io/badge/GitHub-kilna/envhttpd-green?logo=github)](https://github.com/kilna/envhttpd)
[![GitHub forks](https://img.shields.io/github/forks/kilna/envhttpd?style=social)](https://github.com/kilna/envhttpd/forks)
[![GitHub watchers](https://img.shields.io/github/watchers/kilna/envhttpd?style=social)](https://github.com/kilna/envhttpd/watchers)
[![GitHub Repo stars](https://img.shields.io/github/stars/kilna/envhttpd?style=social)](https://github.com/kilna/envhttpd/stargazers)

A web server that delivers environment variables as HTML, JSON, YAML, and
shell-evaluable script, and discrete curl-able endpoints. It allows filtering of
environment variables based on inclusion and exclusion patterns to control what
is exposed.

With a docker image weighing in at under 500kb (that less than half a megabyte),
it is ideal to use as a queryable pod metadata sidecar in Kubernetes, or any
situation in which you need to expose simple data without a lot of overhead.

## Examples

All of the below use a docker container...  when running locally just replace
`docker run ... kilna/envhttpd` with `envhttpd` and make sure the environment
variables are available (e.g. `export foo=bar`).

### Running the container

Fire up a container...

```
$ docker run -e foo=bar -e yo=bro -p 8111:8111 -d kilna/envhttpd
Server is running at http://localhost:8111
```

Now point your browser at http://localhost:8111 and you should see something
like this:

![envhttpd screenshot](https://raw.github.com/kilna/envhttpd/master/screenshot.png?raw=true)

#### Providing command line options to the container

Everything after the image name `kilna/envhttpd` is passed as arguments to the
`envhttpd` binary (see [Command-Line Help](#command-line-help) for more details).

```
$ docker run -e foo=bar -e yo=bro -p 8222:8222 -d kilna/envhttpd -p 8222 -i foo -x yo -H envhttpd.local
Server is running at http://envhttpd.local:8222
```

### Text

Get a specific environment variable's value as plain UTF-8 text:

```
$ curl localhost:8111/var/foo
bar
```

### JSON

Get all included environment variables as a JSON dictionary:

```
$ curl localhost:8111/json
{"foo":"bar","yo":"bro"}

$ curl localhost:8111/json?pretty
{
  "foo": "bar",
  "yo": "bro"
}
```

### YAML

Get all included environment variables as a YAML dictionary:

```
$ curl localhost:8111/yaml
---
foo: bar
yo: bro
```

### Shell

Get all included environment variables as a shell-evaluable script:

```
$ curl localhost:8111/sh
foo="bar"
yo="bro"

$ curl localhost:8111/sh?export
export foo="bar"
export yo="bro"
```

### Kubernetes

See the [kubernetes example](./kubernetes/) for [pod](./kubernetes/pod/) and
[sidecar deployment](./kubernetes/sidecar/) under the `kubernetes/` folder.

## Command-Line Help

```
$ envhttpd -h
Usage: envhttpd [OPTIONS]

envhttpd is a lightweight HTTP server designed to expose env vars
in HTML, JSON, YAML, and evaluatable shell formats. It allows
filtering of environment variables based on inclusion and exclusion
patterns to control what is exposed.

Options:
  -p PORT      Specify the port number the server listens on.
               Default is 8111.
  -i PATTERN   Include env vars matching the specified PATTERN.
               Supports glob patterns (e.g., APPNAME_*).
               Default behavior is to include all env vars except
               PATH and HOME since they're largely not relevant outside
               of a container.
  -x PATTERN   Exclude env vars matching the specified PATTERN.
               Supports glob patterns (e.g., DEBUG*, TEMP).
  -d           Run the server as a daemon in the background.
               (Does not make sense in a docker container)
  -D           Enable debug mode logging and text/plain responses.
  -H HOSTNAME  Specify the hostname of the server.
  -h           Display this help message and exit.

Endpoints:
  /             Displays a web page listing all included env vars.
  /json         Gets env vars in JSON format.
  /json?pretty  Gets env vars in pretty-printed JSON format.
  /yaml         Gets env vars in YAML format.
  /sh           Gets env vars in shell evaluatable format.
  /sh?export    Gets env vars as shell with `export` prefix.
  /var/VARNAME  Gets the value of the specified env var.

envhttpd, Copyright © 2024 Kilna, Anthony https://github.com/kilna/envhttpd
```

## License

[MIT Link License](./LICENSE)

## Author

[Kilna, Anthony](http://github.com/kilna)
[kilna@kilna.com](mailto:kilna@kilna.com)
