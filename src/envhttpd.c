#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>
#include <getopt.h>
#include <fnmatch.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include "template.h"
#include "icon.h"

#define PORT 8111
#define BUFFER_SIZE 1024
#define MAX_PATTERNS 100
#define MAX_ENV_VARS 1000
#define DEFAULT_HOSTNAME "localhost"

// Configuration variables
int server_port = PORT;
int debug = 0;
int daemonize = 0;
char *hostname = DEFAULT_HOSTNAME;

// Define a structure to hold pattern and its type
typedef enum {
  PATTERN_INCLUDE,
  PATTERN_EXCLUDE
} PatternType;

typedef struct {
  PatternType type;
  char *pattern;
} PatternAction;

// Array to store pattern actions
PatternAction pattern_actions[MAX_PATTERNS];
int pattern_action_count = 0;

// Structure to hold env vars
typedef struct {
  char *key;
  char *value;
} EnvVar;

// Array to store filtered env vars
EnvVar env_vars[MAX_ENV_VARS];
int env_var_count = 0;

// Function prototypes
void handle_client(int client_socket);
void add_patterns(char *spec, PatternType type);
void load_environment();
void send_response(int client_socket, const char *content_type, const char *response);
void send_binary_response(int client_socket, const char *content_type, const unsigned char *data, size_t len);
void send_error_response(int client_socket, const char *status, const char *message);
/* void serve_file(int client_socket, const char *file_path, const char *content_type); */
void handle_get_request(int client_socket, const char *path);
void handle_var_request(int client_socket, const char *var_name);
void serve_homepage(int client_socket);
void serve_json(int client_socket, int pretty);
void serve_yaml(int client_socket);
void serve_shell(int client_socket, int export_mode);
void serve_sys(int client_socket);
char *escape_json(const char *input);
char *escape_html(const char *input);
char *escape_yaml(const char *input);
char *escape_env(const char *input);
char *escape_url(const char *src);
char* get_env_var_value(const char *key);
int needs_yaml_quoting(const char *value);

static volatile sig_atomic_t got_sigterm = 0;

static void sigchld_handler(int sig) {
  (void)sig;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

static void sigterm_handler(int sig) {
  (void)sig;
  got_sigterm = 1;
}

int main(int argc, char *argv[]) {
  int opt;
  while ((opt = getopt(argc, argv, "p:i:x:dDhH:")) != -1) {
    switch (opt) {
      case 'p':
        server_port = atoi(optarg);
        break;
      case 'i':
        add_patterns(optarg, PATTERN_INCLUDE);
        break;
      case 'x':
        add_patterns(optarg, PATTERN_EXCLUDE);
        break;
      case 'd':
        daemonize = 1;
        break;
      case 'D':
        debug = 1;
        break;
      case 'h':
        printf("Usage: %s [OPTIONS]\n\n", argv[0]);
        printf("envhttpd is a lightweight HTTP server designed to expose env vars\n");
        printf("in HTML, JSON, YAML, and evaluatable shell formats. It allows\n");
        printf("filtering of environment variables based on inclusion and exclusion\n");
        printf("patterns to control what is exposed.\n\n");
        printf("Options:\n");
        printf("  -p PORT      Specify the port number the server listens on.\n");
        printf("               Default is 8111.\n");
        printf("  -i PATTERN   Include env vars matching the specified PATTERN.\n");
        printf("               Supports glob patterns (e.g., APPNAME_*, \n");
        printf("               Default behavior is to include all env vars except\n");
        printf("               PATH and HOME since they're largely not relevant outside\n");
        printf("               of a container.\n");
        printf("  -x PATTERN   Exclude env vars matching the specified PATTERN.\n");
        printf("               Supports glob patterns (e.g., DEBUG*, TEMP).\n");
        printf("  -d           Run the server as a daemon in the background.\n");
        printf("               (Does not make sense in a docker container)\n");
        printf("  -D           Enable debug mode logging and text/plain responses.\n");
        printf("  -H HOSTNAME  Specify the hostname of the server.\n");
        printf("  -h           Display this help message and exit.\n\n");
        printf("Endpoints:\n");
        printf("  /             Displays a web page listing all included env vars.\n");
        printf("  /json         Gets env vars in JSON format.\n");
        printf("  /json?pretty  Gets env vars in pretty-printed JSON format.\n");
        printf("  /yaml         Gets env vars in YAML format.\n");
        printf("  /sh           Gets env vars in shell evaluatable format.\n");
        printf("  /sh?export    Gets env vars as shell with `export` prefix.\n");
        printf("  /var/VARNAME  Gets the value of the specified env var.\n");
        printf("\n");
        printf("envhttpd - Copyright © 2024 Kilna, Anthony https://github.com/kilna/envhttpd\n");
        exit(EXIT_SUCCESS);
      case 'H':
        hostname = optarg;
        break;
      default:
        fprintf(
          stderr,
          "Usage: %s [-p port] [-i include_pattern|...] [-x exclude_pattern|...]"
          " [-d] [-D] [-H hostname]\n",
          argv[0]
        );
        exit(EXIT_FAILURE);
    }
  }
  load_environment(); // Load env vars once at startup

  // Output the server link upon startup
  printf("Server is running at http://%s:%d\n", hostname, server_port);
  fflush(stdout);

  // Daemonize if requested
  if (daemonize) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork failed"); exit(EXIT_FAILURE); }
    if (pid > 0) { exit(EXIT_SUCCESS); } // Parent exits
    if (setsid() < 0) { perror("setsid failed"); exit(EXIT_FAILURE); }
    // Redirect standard file descriptors to /dev/null
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
  }
  int server_fd, client_socket, max_sd, activity;
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  fd_set readfds;
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }
  int opt_val = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt_val, sizeof(opt_val))) {
    perror("setsockopt failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(server_port);
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }
  if (listen(server_fd, 3) < 0) {
    perror("listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }
  {
    struct sigaction sa = {0};
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);
    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
  }
  FD_ZERO(&readfds); // Set of socket descriptors
  while (1) {
    if (got_sigterm) break;
    FD_ZERO(&readfds); // Clear the socket set
    FD_SET(server_fd, &readfds); // Add server socket to set
    max_sd = server_fd;
    if (debug) { printf("Waiting for new connections...\n"); fflush(stdout); }
    // Wait for an activity on one of the sockets
    activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
    if (activity < 0) {
      if (errno == EINTR) { if (got_sigterm) break; continue; }
      perror("select error");
    }
    if (FD_ISSET(server_fd, &readfds)) {
      if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept failed");
        continue;
      }
      if (debug) {
        printf("Accepted new connection.\n");
        fflush(stdout);
      }
      handle_client(client_socket); // Handle the client in the same loop
    }
  }
  close(server_fd);
  return 0;
}

void handle_client(int client_socket) {
  if (debug) {
    printf("Handling new client (socket %d).\n", client_socket);
    fflush(stdout);
  }
  char buffer[BUFFER_SIZE];
  ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
  if (bytes_read < 0) {
    perror("recv failed");
    close(client_socket);
    return;
  }
  buffer[bytes_read] = '\0';
  char method[8];
  char path[BUFFER_SIZE];
  sscanf(buffer, "%s %s", method, path);
  if (debug) {
    printf("Received request: Method=%s, Path=%s\n", method, path);
    fflush(stdout);
  }
  if (strcmp(method, "GET") == 0) {
    handle_get_request(client_socket, path);
  } else {
    send_error_response(client_socket, "405 Method Not Allowed", "Method Not Allowed");
  }
  close(client_socket);
}

void handle_get_request(int client_socket, const char *path) {
  if (strcmp(path, "/") == 0) {
    serve_homepage(client_socket);
  } else if (strcmp(path, "/icon.png") == 0) {
    send_binary_response(client_socket, "image/png", icon_png, (size_t)icon_png_len);
  } else if (strncmp(path, "/var/", 5) == 0) {
    handle_var_request(client_socket, path + 5);
  } else if (strcmp(path, "/json") == 0 || strcmp(path, "/json?pretty") == 0) {
    serve_json(client_socket, strcmp(path, "/json?pretty") == 0);
  } else if (strcmp(path, "/yaml") == 0) {
    serve_yaml(client_socket);
  } else if (strcmp(path, "/sh") == 0 || strcmp(path, "/sh?export") == 0) {
    serve_shell(client_socket, strcmp(path, "/sh?export") == 0);
  } else if (strcmp(path, "/sys") == 0) {
    serve_sys(client_socket);
  } else {
    send_error_response(client_socket, "404 Not Found", "Not Found");
  }
}

void handle_var_request(int client_socket, const char *var_name) {
  char *end = strchr(var_name, '?');
  if (end) { *end = '\0'; }
  if (debug) {
    printf("Fetching environment variable: %s\n", var_name);
  }
  char *value = get_env_var_value(var_name);
  if (value) {
    send_response(client_socket, "text/plain", value);
  } else {
    send_error_response(client_socket, "404 Not Found", "Variable Not Found");
  }
}

void serve_homepage(int client_socket) {
  char *title;
  if (asprintf(&title, "%s - envhttpd", hostname) == -1) {
    perror("asprintf failed");
    close(client_socket);
    return;
  }

  char *table_rows = strdup("");
  if (!table_rows) {
    perror("strdup failed");
    free(title);
    close(client_socket);
    return;
  }
  for (int i = 0; i < env_var_count; i++) {
    char *escaped_key = escape_html(env_vars[i].key);
    char *escaped_value = escape_html(env_vars[i].value);
    if (!escaped_key || !escaped_value) {
      perror("escape_html failed");
      free(title);
      free(table_rows);
      free(escaped_key);
      free(escaped_value);
      close(client_socket);
      return;
    }

    char *url_encoded_key = escape_url(env_vars[i].key);
    if (!url_encoded_key) {
      perror("URL encoding failed");
      free(title);
      free(table_rows);
      free(escaped_key);
      free(escaped_value);
      close(client_socket);
      return;
    }

    char *new_table_rows;
    if (asprintf(&new_table_rows, "%s<tr><td><strong><a href=\"/var/%s\" title=\"Raw %s environment variable contents\">%s</a></strong></td><td><pre>%s</pre></td></tr>\n",
                 table_rows, url_encoded_key, escaped_key, escaped_key, escaped_value) == -1) {
      perror("asprintf failed");
      free(title);
      free(table_rows);
      free(escaped_key);
      free(escaped_value);
      free(url_encoded_key);
      close(client_socket);
      return;
    }

    free(table_rows);
    free(escaped_key);
    free(escaped_value);
    free(url_encoded_key);
    table_rows = new_table_rows;
  }

  char *html;
  if (asprintf(&html, template, title, table_rows) == -1) {
    perror("asprintf failed");
    free(title);
    free(table_rows);
    close(client_socket);
    return;
  }

  free(title);
  free(table_rows);

  send_response(client_socket, "text/html", html);
  free(html);
}

void serve_json(int client_socket, int pretty) {
  char *content_type = debug ? "text/json" : "application/json";
  char *json = pretty ? strdup("{\n") : strdup("{");
  if (!json) {
    perror("strdup failed");
    close(client_socket);
    return;
  }
  for (int i = 0; i < env_var_count; i++) {
    char *escaped_value = escape_json(env_vars[i].value);
    if (!escaped_value) {
      perror("escape_json failed");
      free(json);
      close(client_socket);
      return;
    }
    char *new_json;
    if (pretty) {
      if (asprintf(&new_json, "%s  \"%s\": \"%s\",\n", json, env_vars[i].key, escaped_value) == -1) {
        perror("asprintf failed");
        free(json);
        free(escaped_value);
        close(client_socket);
        return;
      }
    } else {
      if (asprintf(&new_json, "%s\"%s\":\"%s\",", json, env_vars[i].key, escaped_value) == -1) {
        perror("asprintf failed");
        free(json);
        free(escaped_value);
        close(client_socket);
        return;
      }
    }
    free(json);
    free(escaped_value);
    json = new_json;
  }
  if (strlen(json) > 3) {
    if (pretty) { json[strlen(json) - 2] = '\n'; }
    json[strlen(json) - 1] = '}';
  } else {
    strcpy(json, "{}");
  }
  send_response(client_socket, content_type, json);
  free(json);
}

void serve_yaml(int client_socket) {
  char *content_type = debug ? "text/yaml" : "application/yaml";
  size_t yaml_size = 0;
  for (int i = 0; i < env_var_count; i++) {
    yaml_size += strlen(env_vars[i].key) + strlen(env_vars[i].value) * 2 + 5;
  }
  char *yaml = malloc(yaml_size + 4 + 1);
  if (!yaml) {
    perror("malloc failed");
    close(client_socket);
    return;
  }
  strcpy(yaml, "---\n");
  for (int i = 0; i < env_var_count; i++) {
    char *escaped_key;
    if (needs_yaml_quoting(env_vars[i].key)) {
      escaped_key = escape_yaml(env_vars[i].key);
    } else {
      escaped_key = strdup(env_vars[i].key);
    }
    char *escaped_value;
    if (needs_yaml_quoting(env_vars[i].value)) {
      escaped_value = escape_yaml(env_vars[i].value);
    } else {
      escaped_value = strdup(env_vars[i].value);
    }
    if (!escaped_key || !escaped_value) {
      perror("escape_yaml failed");
      free(yaml);
      free(escaped_key);
      free(escaped_value);
      close(client_socket);
      return;
    }
    strcat(yaml, escaped_key);
    strcat(yaml, ": ");
    strcat(yaml, escaped_value);
    strcat(yaml, "\n");
    free(escaped_key);
    free(escaped_value);
  }
  send_response(client_socket, content_type, yaml);
  free(yaml);
}

void serve_shell(int client_socket, int export_mode) {
  size_t env_size = 0;
  for (int i = 0; i < env_var_count; i++) {
    env_size += strlen(env_vars[i].key) + strlen(env_vars[i].value) * 2 + 3;
    if (export_mode) {
      env_size += 7;
    }
  }
  char *env_content = malloc(env_size + 1);
  if (!env_content) {
    perror("malloc failed");
    close(client_socket);
    return;
  }
  env_content[0] = '\0';
  for (int i = 0; i < env_var_count; i++) {
    if (export_mode) {
      strcat(env_content, "export ");
    }
    strcat(env_content, env_vars[i].key);
    strcat(env_content, "=\"");
    char *escaped_value = escape_env(env_vars[i].value);
    if (!escaped_value) {
      perror("escape_env failed");
      free(env_content);
      close(client_socket);
      return;
    }
    strcat(env_content, escaped_value);
    strcat(env_content, "\"\n");
    free(escaped_value);
  }
  send_response(client_socket, "text/plain", env_content);
  free(env_content);
}

void serve_sys(int client_socket) {
  struct utsname sys_info;
  if (uname(&sys_info) < 0) {
    perror("uname failed");
    send_error_response(client_socket, "500 Internal Server Error", "Could not retrieve system information");
    return;
  }

  char response[BUFFER_SIZE];
  snprintf(response, sizeof(response),
           "System Name: %s\n"
           "Node Name: %s\n"
           "Release: %s\n"
           "Version: %s\n"
           "Machine: %s\n",
           sys_info.sysname,
           sys_info.nodename,
           sys_info.release,
           sys_info.version,
           sys_info.machine);

  send_response(client_socket, "text/plain", response);
}

void add_patterns(char *spec, PatternType type) {
  if (pattern_action_count < MAX_PATTERNS) {
    pattern_actions[pattern_action_count].type = type;
    pattern_actions[pattern_action_count].pattern = strdup(spec);
    pattern_action_count++;
  }
}

void load_environment() {
  extern char **environ;
  for (char **env = environ; *env && env_var_count < MAX_ENV_VARS; ++env) {
    char *env_entry = strdup(*env);
    char *key = strtok(env_entry, "=");
    char *value = strtok(NULL, "");
    if (key && value) {
      int include = 1;
      if (strcmp(key, "PATH") == 0 || strcmp(key, "HOME") == 0) {
        include = 0;
      }
      for (int i = 0; i < pattern_action_count; i++) {
        if (fnmatch(pattern_actions[i].pattern, key, 0) == 0) {
          if (pattern_actions[i].type == PATTERN_INCLUDE) {
            include = 1;
          } else if (pattern_actions[i].type == PATTERN_EXCLUDE) {
            include = 0;
          }
        }
      }
      if (include) {
        env_vars[env_var_count].key = strdup(key);
        env_vars[env_var_count].value = strdup(value);
        env_var_count++;
      }
    }
    free(env_entry);
  }
}

void send_response(int client_socket, const char *content_type, const char *response) {
  size_t response_len = strlen(response);
  int header_length = snprintf(NULL, 0,
                               "HTTP/1.1 200 OK\r\n"
                               "Content-Type: %s; charset=utf-8\r\n"
                               "Content-Length: %zu\r\n"
                               "Hostname: %s\r\n"
                               "\r\n",
                               content_type, response_len, hostname) + 1;
  char *header_buffer = malloc(header_length);
  if (!header_buffer) {
    perror("malloc failed");
    return;
  }
  snprintf(header_buffer, header_length,
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: %s; charset=utf-8\r\n"
           "Content-Length: %zu\r\n"
           "Hostname: %s\r\n"
           "\r\n",
           content_type, response_len, hostname);
  if (send(client_socket, header_buffer, header_length - 1, 0) < 0) {
    perror("send headers failed");
    free(header_buffer);
    return;
  }
  free(header_buffer);
  size_t total_sent = 0;
  size_t total_length = response_len;
  const char *response_ptr = response;

  while (total_sent < total_length) {
    size_t chunk_size = (
        (total_length - total_sent) < BUFFER_SIZE
        ? (total_length - total_sent)
        : BUFFER_SIZE
    );
    ssize_t sent = send(client_socket, response_ptr + total_sent, chunk_size, 0);
    if (sent < 0) {
      perror("send body failed");
      break;
    }
    total_sent += sent;
  }
}

void send_binary_response(int client_socket, const char *content_type, const unsigned char *data, size_t len) {
  int header_length = snprintf(NULL, 0,
                               "HTTP/1.1 200 OK\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %zu\r\n"
                               "Hostname: %s\r\n"
                               "\r\n",
                               content_type, len, hostname) + 1;
  char *header_buffer = malloc(header_length);
  if (!header_buffer) {
    perror("malloc failed");
    return;
  }
  snprintf(header_buffer, header_length,
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %zu\r\n"
           "Hostname: %s\r\n"
           "\r\n",
           content_type, len, hostname);
  if (send(client_socket, header_buffer, header_length - 1, 0) < 0) {
    perror("send headers failed");
    free(header_buffer);
    return;
  }
  free(header_buffer);
  size_t total_sent = 0;
  while (total_sent < len) {
    size_t chunk_size = (len - total_sent) < (size_t)BUFFER_SIZE ? (len - total_sent) : (size_t)BUFFER_SIZE;
    ssize_t sent = send(client_socket, data + total_sent, chunk_size, 0);
    if (sent < 0) {
      perror("send body failed");
      break;
    }
    total_sent += (size_t)sent;
  }
}

void send_error_response(int client_socket, const char *status, const char *message) {
  char buffer[BUFFER_SIZE];
  snprintf(buffer, sizeof(buffer),
           "HTTP/1.1 %s\r\n"
           "Content-Type: text/plain; charset=utf-8\r\n"
           "Content-Length: %zu\r\n"
           "\r\n"
           "%s\n", status, strlen(message) + 1, message);
  send(client_socket, buffer, strlen(buffer), 0);
}

/*
void serve_file(int client_socket, const char *file_path, const char *content_type) {
  FILE *file = fopen(file_path, "rb");
  if (!file) {
    perror("fopen failed");
    send_error_response(client_socket, "404 Not Found", "Not Found");
    return;
  }
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *file_content = malloc(file_size);
  if (!file_content) {
    perror("malloc failed");
    fclose(file);
    close(client_socket);
    return;
  }
  fread(file_content, 1, file_size, file);
  fclose(file);
  char header[BUFFER_SIZE];
  snprintf(header, sizeof(header),
           "HTTP/1.1 200 OK\r\n"
           "Content-Type: %s\r\n"
           "Content-Length: %ld\r\n"
           "\r\n", content_type, file_size);
  send(client_socket, header, strlen(header), 0);
  send(client_socket, file_content, file_size, 0);
  free(file_content);
}
*/

char *escape_json(const char *input) {
  size_t len = strlen(input);
  char *escaped = malloc(len * 2 + 1);
  char *p = escaped;
  for (const char *s = input; *s; s++) {
    switch (*s) {
      case '\\': *p++ = '\\'; *p++ = '\\'; break;
      case '\"': *p++ = '\\'; *p++ = '\"'; break;
      case '\b': *p++ = '\\'; *p++ = 'b'; break;
      case '\f': *p++ = '\\'; *p++ = 'f'; break;
      case '\n': *p++ = '\\'; *p++ = 'n'; break;
      case '\r': *p++ = '\\'; *p++ = 'r'; break;
      case '\t': *p++ = '\\'; *p++ = 't'; break;
      default: *p++ = *s; break;
    }
  }
  *p = '\0';
  return escaped;
}

char *escape_html(const char *input) {
  size_t len = strlen(input);
  char *escaped = malloc(len * 6 + 1);
  if (!escaped) { return NULL; }
  char *p = escaped;
  for (const char *s = input; *s; s++) {
    switch (*s) {
      case '&': strcpy(p, "&amp;"); p += 5; break;
      case '<': strcpy(p, "&lt;"); p += 4; break;
      case '>': strcpy(p, "&gt;"); p += 4; break;
      case '\"': strcpy(p, "&quot;"); p += 6; break;
      case '\'': strcpy(p, "&#39;"); p += 5; break;
      default: *p++ = *s; break;
    }
  }
  *p = '\0';
  return escaped;
}

char *escape_yaml(const char *input) {
  size_t len = strlen(input);
  char *escaped = malloc(len * 2 + 3);
  if (!escaped) { return NULL; }
  char *p = escaped;
  *p++ = '\"';
  for (const char *s = input; *s; s++) {
    if (*s == '\"' || *s == '\\') { *p++ = '\\'; }
    if (*s == '\n') { *p++ = '\\'; *p++ = 'n'; } else { *p++ = *s; }
  }
  *p++ = '\"';
  *p = '\0';
  return escaped;
}

char *escape_env(const char *input) {
  size_t len = strlen(input);
  char *escaped = malloc(len * 2 + 1);
  if (!escaped) { return NULL; }
  char *p = escaped;
  for (const char *s = input; *s; s++) {
    if (*s == '\\' || *s == '\"' || *s == '\n') { *p++ = '\\'; }
    *p++ = *s;
  }
  *p = '\0';
  return escaped;
}

char *escape_url(const char *src) {
  size_t src_len = strlen(src);
  char *enc = malloc(src_len * 3 + 1);
  if (!enc) { return NULL; }
  char *penc = enc;
  for (; *src; src++) {
    if (isalnum((unsigned char)*src) || 
        *src == '-' || *src == '_' || *src == '.' || *src == '~') {
      *penc++ = *src;
    } else {
      sprintf(penc, "%%%02X", (unsigned char)*src);
      penc += 3;
    }
  }
  *penc = '\0';
  return enc;
}

char* get_env_var_value(const char *key) {
  for (int i = 0; i < env_var_count; i++) {
    if (strcmp(env_vars[i].key, key) == 0) { return env_vars[i].value; }
  }
  return NULL;
}

int needs_yaml_quoting(const char *value) {
  const char *special_chars = ":{}[],&*#?|-<>=!%@\\\"'\n";
  for (const char *s = value; *s; s++) {
    if (strchr(special_chars, *s)) {
      return 1;
    }
  }
  const char *reserved_words[] = {
    "true", "false", "null", "yes", "no", "on", "off", NULL
  };
  for (int i = 0; reserved_words[i] != NULL; i++) {
    if (strcasecmp(value, reserved_words[i]) == 0) {
      return 1;
    }
  }
  if (value[0] == '\0') {
    return 1;
  }
  return 0;
}