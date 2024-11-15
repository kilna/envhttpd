#!/bin/sh

set -e -u

cd /tmp

BASE_URL="http://${SERVER_HOST}:${SERVER_PORT}"

ERROR=0

assert_present() {
  if grep -qF "$2" "$1"; then echo "OK: $2 found in $1"; return 0; fi
  echo "Error: $2 not found in $1"; ERROR=$((ERROR + 1)); return 1
}

assert_missing() {
  if ! grep -qF "$2" "$1"; then echo "OK: $2 not found in $1"; return 0; fi
  echo "NOT OK: $2 found in $1"; ERROR=$((ERROR + 1)); return 1
}

for path_file in \
  "/ env.html" \
  "/sys sys.txt" \
  "/var/INCLUDE_ME var_INCLUDE_ME.txt" \
  "/json compact.json" \
  "/json?pretty pretty.json" \
  "/yaml env.yaml" \
  "/sh env.sh" \
  "/sh?export export.sh"
do
  path=$(echo ${path_file} | cut -d' ' -f1)
  file=$(echo ${path_file} | cut -d' ' -f2)
  echo "Saving ${BASE_URL}${path} to ${file}"
  curl -s -i ${BASE_URL}${path} -o ${file} -D ${file}.headers
  #cat ${file}.headers ${file}
done

curl -s -i ${BASE_URL}/var/EXCLUDE_ME -o var_EXCLUDE_ME.txt -D var_EXCLUDE_ME.txt.headers
curl -s -i ${BASE_URL}/404 -o 404.txt -D 404.txt.headers


echo "================================================"
echo "BASE_URL: ${BASE_URL}"
cat sys.txt
echo "================================================"

assert_present env.html.headers "200 OK"
assert_present env.html.headers "Content-Type: text/html"
assert_present env.html "Kilna, Anthony"
assert_missing env.html "HOSTNAME"
assert_missing env.html "EXCLUDE_ME"
assert_present env.html "INCLUDE_ME"

assert_present var_INCLUDE_ME.txt.headers "200 OK"
assert_present var_INCLUDE_ME.txt.headers "Content-Type: text/plain"
assert_present var_INCLUDE_ME.txt "yes"

assert_present sys.txt.headers "200 OK"
assert_present sys.txt.headers "Content-Type: text/plain"
assert_present sys.txt "System Name: Linux"

assert_present compact.json.headers "200 OK"
assert_present compact.json.headers "Content-Type: text/json"
assert_present compact.json "INCLUDE_ME"
assert_present compact.json "yes"
assert_missing compact.json "HOSTNAME"
assert_missing compact.json "EXCLUDE_ME"

assert_present pretty.json.headers "200 OK"
assert_present pretty.json.headers "Content-Type: text/json"
assert_present pretty.json "INCLUDE_ME"
assert_present pretty.json "yes"
assert_missing pretty.json "HOSTNAME"
assert_missing pretty.json "EXCLUDE_ME"

assert_present env.yaml.headers "200 OK"
assert_present env.yaml.headers "Content-Type: text/yaml"
assert_present env.yaml "INCLUDE_ME"
assert_present env.yaml "yes"
assert_missing env.yaml "HOSTNAME"
assert_missing env.yaml "EXCLUDE_ME"

assert_present env.sh.headers "200 OK"
assert_present env.sh.headers "Content-Type: text/plain"
assert_present env.sh "INCLUDE_ME"
assert_present env.sh "yes"
assert_missing env.sh "HOSTNAME"
assert_missing env.sh "EXCLUDE_ME"

assert_present export.sh.headers "200 OK"
assert_present export.sh.headers "Content-Type: text/plain"
assert_present export.sh "export"
assert_present export.sh "INCLUDE_ME"
assert_present export.sh "yes"
assert_missing export.sh "HOSTNAME"
assert_missing export.sh "EXCLUDE_ME"

assert_present 404.txt.headers "404 Not Found"
assert_present 404.txt "Not Found"

assert_present var_EXCLUDE_ME.txt.headers "404 Not Found"
assert_present var_EXCLUDE_ME.txt "Not Found"

exit ${ERROR}
