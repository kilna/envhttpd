#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fnmatch.h>
#include <sys/utsname.h>
#include <ctype.h>

#define DEFAULT_PORT 8111
#define MAX_PATTERNS 100
#define MAX_ENV_VARS 1000
#define DEFAULT_HOSTNAME "localhost"

// Global configuration variables
int server_port = DEFAULT_PORT;
int debug = 0;
int daemonize = 0;
const char *hostname = DEFAULT_HOSTNAME;

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

// Function to add patterns
void add_patterns(const char *spec, PatternType type) {
  if (pattern_action_count < MAX_PATTERNS) {
    pattern_actions[pattern_action_count].type = type;
    pattern_actions[pattern_action_count].pattern = strdup(spec);
    pattern_action_count++;
  }
}

// Function to load environment variables
void load_environment() {
  extern char **environ;
  for (char **env = environ; *env && env_var_count < MAX_ENV_VARS; ++env) {
    char *env_entry = strdup(*env);
    char *key = strtok(env_entry, "=");
    char *value = strtok(NULL, "");
    if (key && value) {
      int include = 1;
      for (int i = 0; i < pattern_action_count; i++) {
        if (fnmatch(pattern_actions[i].pattern, key, 0) == 0) {
          include = (pattern_actions[i].type == PATTERN_INCLUDE);
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

// Function to escape JSON
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

// Function to escape HTML
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

// Function to escape YAML
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

// Function to escape environment variables
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

// Function to escape URL
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

// Function to serve the homepage
static int serve_homepage(struct MHD_Connection *connection) {
  char *html = strdup("<html><head><title>envhttpd</title></head><body><table>");
  if (!html) return MHD_NO;

  for (int i = 0; i < env_var_count; i++) {
    char *escaped_key = escape_json(env_vars[i].key);
    char *escaped_value = escape_json(env_vars[i].value);
    if (!escaped_key || !escaped_value) {
      free(html);
      free(escaped_key);
      free(escaped_value);
      return MHD_NO;
    }

    char *new_html;
    if (asprintf(&new_html, "%s<tr><td><strong>%s</strong></td><td><pre>%s</pre></td></tr>\n",
                 html, escaped_key, escaped_value) == -1) {
      free(html);
      free(escaped_key);
      free(escaped_value);
      return MHD_NO;
    }

    free(html);
    free(escaped_key);
    free(escaped_value);
    html = new_html;
  }

  char *final_html;
  if (asprintf(&final_html, "%s</table></body></html>", html) == -1) {
    free(html);
    return MHD_NO;
  }
  free(html);

  struct MHD_Response *response = MHD_create_response_from_buffer(strlen(final_html), final_html, MHD_RESPMEM_MUST_FREE);
  if (!response) {
    free(final_html);
    return MHD_NO;
  }

  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}

// Function to serve JSON
static int serve_json(struct MHD_Connection *connection, int pretty) {
  char *json = pretty ? strdup("{\n") : strdup("{");
  if (!json) return MHD_NO;

  for (int i = 0; i < env_var_count; i++) {
    char *escaped_value = escape_json(env_vars[i].value);
    if (!escaped_value) {
      free(json);
      return MHD_NO;
    }
    char *new_json;
    if (pretty) {
      if (asprintf(&new_json, "%s  \"%s\": \"%s\",\n", json, env_vars[i].key, escaped_value) == -1) {
        free(json);
        free(escaped_value);
        return MHD_NO;
      }
    } else {
      if (asprintf(&new_json, "%s\"%s\":\"%s\",", json, env_vars[i].key, escaped_value) == -1) {
        free(json);
        free(escaped_value);
        return MHD_NO;
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

  struct MHD_Response *response = MHD_create_response_from_buffer(strlen(json), json, MHD_RESPMEM_MUST_FREE);
  if (!response) {
    free(json);
    return MHD_NO;
  }

  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}

// Function to serve YAML
static int serve_yaml(struct MHD_Connection *connection) {
  char *yaml = strdup("");
  if (!yaml) return MHD_NO;

  for (int i = 0; i < env_var_count; i++) {
    char *new_yaml;
    if (asprintf(&new_yaml, "%s%s: \"%s\"\n", yaml, env_vars[i].key, env_vars[i].value) == -1) {
      free(yaml);
      return MHD_NO;
    }
    free(yaml);
    yaml = new_yaml;
  }

  struct MHD_Response *response = MHD_create_response_from_buffer(strlen(yaml), yaml, MHD_RESPMEM_MUST_FREE);
  if (!response) {
    free(yaml);
    return MHD_NO;
  }

  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}

// Function to serve shell script
static int serve_shell(struct MHD_Connection *connection, int export) {
  char *shell = strdup("");
  if (!shell) return MHD_NO;

  for (int i = 0; i < env_var_count; i++) {
    char *new_shell;
    if (export) {
      if (asprintf(&new_shell, "%sexport %s=\"%s\"\n", shell, env_vars[i].key, env_vars[i].value) == -1) {
        free(shell);
        return MHD_NO;
      }
    } else {
      if (asprintf(&new_shell, "%s%s=\"%s\"\n", shell, env_vars[i].key, env_vars[i].value) == -1) {
        free(shell);
        return MHD_NO;
      }
    }
    free(shell);
    shell = new_shell;
  }

  struct MHD_Response *response = MHD_create_response_from_buffer(strlen(shell), shell, MHD_RESPMEM_MUST_FREE);
  if (!response) {
    free(shell);
    return MHD_NO;
  }

  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}

// Function to serve a file
static int serve_file(struct MHD_Connection *connection, const char *file_path, const char *content_type) {
  FILE *file = fopen(file_path, "rb");
  if (!file) {
    const char *response_str = "Not Found";
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(response_str), (void *)response_str, MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    return ret;
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  char *file_content = malloc(file_size);
  if (!file_content) {
    fclose(file);
    return MHD_NO;
  }
  fread(file_content, 1, file_size, file);
  fclose(file);

  struct MHD_Response *response = MHD_create_response_from_buffer(file_size, file_content, MHD_RESPMEM_MUST_FREE);
  if (!response) {
    free(file_content);
    return MHD_NO;
  }
  MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, content_type);
  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

// Function to serve system information
static int serve_sys(struct MHD_Connection *connection) {
  struct utsname sys_info;
  if (uname(&sys_info) < 0) {
    const char *response_str = "Could not retrieve system information";
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(response_str), (void *)response_str, MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
    MHD_destroy_response(response);
    return ret;
  }

  char response[256];
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

  struct MHD_Response *response_obj = MHD_create_response_from_buffer(strlen(response), response, MHD_RESPMEM_PERSISTENT);
  if (!response_obj) return MHD_NO;
  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response_obj);
  MHD_destroy_response(response_obj);
  return ret;
}

// Function to get the value of an environment variable
char* get_env_var_value(const char *key) {
  for (int i = 0; i < env_var_count; i++) {
    if (strcmp(env_vars[i].key, key) == 0) {
      return env_vars[i].value;
    }
  }
  return NULL;
}

// Function to handle variable requests
static int serve_var(struct MHD_Connection *connection, const char *var_name) {
  char *value = get_env_var_value(var_name);
  if (value) {
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(value), value, MHD_RESPMEM_PERSISTENT);
    if (!response) return MHD_NO;
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
  } else {
    const char *response_str = "Variable Not Found";
    struct MHD_Response *response = MHD_create_response_from_buffer(strlen(response_str), (void *)response_str, MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    return ret;
  }
}

// Callback function to handle requests
static enum MHD_Result request_handler(void *cls, struct MHD_Connection *connection,
                                       const char *url, const char *method,
                                       const char *version, const char *upload_data,
                                       size_t *upload_data_size, void **con_cls) {
  if (strcmp(method, "GET") == 0) {
    if (strcmp(url, "/") == 0) {
      return serve_homepage(connection);
    } else if (strncmp(url, "/var/", 5) == 0) {
      return serve_var(connection, url + 5);
    } else if (strcmp(url, "/json") == 0) {
      return serve_json(connection, 0);
    } else if (strcmp(url, "/json?pretty") == 0) {
      return serve_json(connection, 1);
    } else if (strcmp(url, "/yaml") == 0) {
      return serve_yaml(connection);
    } else if (strcmp(url, "/sh") == 0) {
      return serve_shell(connection, 0);
    } else if (strcmp(url, "/sh?export") == 0) {
      return serve_shell(connection, 1);
    } else if (strcmp(url, "/sys") == 0) {
      return serve_sys(connection);
    } else if (strcmp(url, "/icon.png") == 0) {
      return serve_file(connection, "/var/www/icon.png", "image/png");
    }
  }

  const char *response_str = "404 Not Found";
  struct MHD_Response *response = MHD_create_response_from_buffer(strlen(response_str), (void *)response_str, MHD_RESPMEM_PERSISTENT);
  int ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
  MHD_destroy_response(response);

  return ret;
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
        printf("  -x PATTERN   Exclude env vars matching the specified PATTERN.\n");
        printf("  -d           Run the server as a daemon in the background.\n");
        printf("  -D           Enable debug mode logging and text/plain responses.\n");
        printf("  -H HOSTNAME  Specify the hostname of the server.\n");
        printf("  -h           Display this help message and exit.\n\n");
        printf("Example paths:\n");
        printf("  /            Serve the homepage with env vars in HTML format.\n");
        printf("  /json        Serve env vars in JSON format.\n");
        printf("  /json?pretty Serve env vars in pretty JSON format.\n");
        printf("  /yaml        Serve env vars in YAML format.\n");
        printf("  /sh          Serve env vars as shell script.\n");
        printf("  /sh?export   Serve env vars as shell script with export.\n\n");
        printf("Copyright (c) 2023 Your Company Name. All rights reserved.\n");
        exit(EXIT_SUCCESS);
      case 'H':
        hostname = optarg;
        break;
      default:
        fprintf(stderr, "Usage: %s [-p port] [-i include_pattern|...] [-x exclude_pattern|...] [-d] [-D] [-H hostname]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  load_environment(); // Load env vars once at startup

  struct MHD_Daemon *daemon;
  daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, server_port, NULL, NULL,
                            &request_handler, NULL, MHD_OPTION_END);
  if (NULL == daemon) {
    fprintf(stderr, "Failed to start server\n");
    return 1;
  }

  printf("Server is running at http://%s:%d\n", hostname, server_port);
  getchar(); // Wait for user input to stop the server

  MHD_stop_daemon(daemon);
  return 0;
}