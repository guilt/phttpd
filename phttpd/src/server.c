#include <netlib.h>

#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef UNIX
#include <sys/wait.h>
#include <sys/types.h>
#endif /* UNIX */

#include <time.h>

#ifdef DEBUG
#define log_printf(...)                                     \
  printf("%s:%d:%s(): ", __FILE__, __LINE__, __FUNCTION__); \
  printf(__VA_ARGS__);                                      \
  fflush(stdout)
#else
#define log_printf(...)
#endif /* DEBUG */

#ifndef SERV_IP
#define SERV_IP "0.0.0.0"
#endif /* SERV_IP */

#ifndef SERV_PORT
#define SERV_PORT 80
#endif /* SERV_PORT */

#ifndef BASE_DIR
#define BASE_DIR "./"
#endif /* BASE_DIR */

#ifndef DEFAULT_FILE
#define DEFAULT_FILE "index.html"
#endif /* DEFAULT_FILE */

#ifdef VHOST
#ifndef DEFAULT_VHOST
#define DEFAULT_VHOST "127.0.0.1"
#endif /* DEFAULT_VHOST */
#endif /* VHOST */

#define LINE_SIZE 4096
#define MISC_SIZE 256

#define HTTP_VERSION_INVALID -1
#define HTTP_VERSION_1_0 0
#define HTTP_VERSION_1_1 1

#define HTTP_INVALID -1
#define HTTP_CONNECT 0
#define HTTP_DELETE 1
#define HTTP_GET 2
#define HTTP_HEAD 3
#define HTTP_OPTIONS 4
#define HTTP_POST 5
#define HTTP_PUT 6
#define HTTP_TRACE 7

static int serving;
static fd in;

void parse_request(char buff[LINE_SIZE], int *http_method, char url[LINE_SIZE],
                   int *http_version) {
  int i = 0, j = 0;
  if (!buff || !http_method || !url || !http_version) return;

  *http_method = HTTP_INVALID;
  *url = 0;
  *http_version = HTTP_VERSION_INVALID;

  for (; buff[i] && isspace(buff[i]); ++i)
    ;

  if (buff[i] == 'G' && buff[i + 1] == 'E' && buff[i + 2] == 'T') {
    *http_method = HTTP_GET;
    i += 3;
  }
  if (buff[i] == 'H' && buff[i + 1] == 'E' && buff[i + 2] == 'A' &&
      buff[i + 2] == 'D') {
    *http_method = HTTP_HEAD;
    i += 4;
  }
  if (buff[i] && !isspace(buff[i])) *http_method = HTTP_INVALID;

  if (*http_method == HTTP_INVALID) return;

  for (; buff[i] && isspace(buff[i]); ++i)
    ;

  for (; buff[i] && !isspace(buff[i]); ++i) url[j++] = buff[i];
  url[j] = 0;

  for (; buff[i] && isspace(buff[i]); ++i)
    ;

  if (buff[i] == 'H' && buff[i + 1] == 'T' && buff[i + 2] == 'T' &&
      buff[i + 3] == 'P') {
    if (buff[i + 4] == '/') {
      if (buff[i + 5] == '1' && buff[i + 6] == '.') {
        if (!buff[i + 8] || isspace(buff[i + 8])) {
          if (buff[i + 7] == '1') {
            *http_version = HTTP_VERSION_1_1;
          } else if (buff[i + 7] == '0') {
            *http_version = HTTP_VERSION_1_0;
          }
        }
      }
    }
  }
}

void parse_header(char buff[LINE_SIZE], char header[LINE_SIZE],
                  char value[LINE_SIZE]) {
  int i = 0, j = 0, k = 0;
  if (!buff || !header || !value) return;

  *header = 0;
  *value = 0;

  for (; buff[i] && isspace(buff[i]); ++i)
    ;

  for (; buff[i] && buff[i] != ':' && !isspace(buff[i]); ++i)
    header[j++] = buff[i];
  header[j] = 0;
  if (buff[i] == ':') ++i;

  for (; buff[i] && isspace(buff[i]); ++i)
    ;

  for (; buff[i]; ++i) value[k++] = buff[i];
  value[k] = 0;
}

char *get_content_type(char file[LINE_SIZE]) {
  int index = -1;
  int i = 0;
#define FILE_TYPES 40
  static char file_types[FILE_TYPES * 2][MISC_SIZE] = {
      "exe",  "application/octet-stream",
      "sh",   "text/x-script.sh",
      "ps",   "application/postscript",
      "pdf",  "application/pdf",
      "swf",  "application/x-shockwave-flash",
      "zip",  "application/zip",
      "tar",  "application/x-tar",
      "gz",   "application/x-gzip",
      "bz2",  "application/x-bzip2",
      "snd",  "audio/basic",
      "wav",  "audio/x-wav",
      "mp3",  "audio/mpeg3",
      "rm",   "application/vnd.rn-realmedia",
      "ram",  "audio/vnd.rn-realaudio",
      "rv",   "video/vnd.rn-realvideo",
      "rpm",  "audio/x-pn-realaudio-plugin",
      "mp4",  "video/mp4",
      "avi",  "video/x-msvideo",
      "wmv",  "video/x-ms-wmv",
      "mov",  "video/quicktime",
      "flv",  "video/flv",
      "gif",  "image/gif",
      "jpg",  "image/jpeg",
      "jpeg", "image/jpeg",
      "png",  "image/png",
      "tif",  "image/tiff",
      "tiff", "image/tiff",
      "htm",  "text/html",
      "html", "text/html",
      "js",   "text/javascript",
      "css",  "text/css",
      "c",    "text/plain",
      "s",    "text/plain",
      "cc",   "text/plain",
      "cpp",  "text/plain",
      "java", "text/plain",
      "h",    "text/plain",
      "pl",   "text/plain",
      "pm",   "text/plain",
      "txt",  "text/plain",
  };
  while (i < LINE_SIZE && file[i]) {
    if (file[i] == '.') index = i;
    ++i;
  }
  if (index >= 0) {
    ++index;
    int f = 0, kf = 0;
    for (; f < FILE_TYPES; ++f, kf += 2) {
      if (!strcasecmp(file + index, file_types[kf])) return file_types[kf + 1];
    }
  }
  return "content/unknown";
}

void get_range_values(char value[LINE_SIZE], int *start, int *end) {
  char *vptr = value;
  *start = -1;
  *end = -1;
  if (strncmp(vptr, "bytes", 5)) return;
  vptr += 5;
  if (!isdigit(*vptr)) ++vptr;
  if (!*vptr) return;
  if (!sscanf(vptr, "%d", start)) return;
  for (; *vptr && *vptr != '-'; ++vptr)
    ;
  if (!*vptr) return;
  ++vptr;
  if (!*vptr) return;
  sscanf(vptr, "%d", end);
}

void map_url(char url[LINE_SIZE],
#ifdef VHOST
             char vhost[LINE_SIZE],
#endif /* VHOST */
             char file[LINE_SIZE], int *send_redir) {
  int a, b, i = 0, j = 0;
  int reindex = 0, bdlen = 0, dflen = 0;
  char c, *bdptr = BASE_DIR, *dfptr = DEFAULT_FILE;
#ifdef VHOST
  int k = 0;
  char *dvptr = DEFAULT_VHOST;
#endif /* VHOST */

  if (!url || !*url || !file || !send_redir) return;
#ifdef VHOST
  if (!vhost) return;
#endif /* VHOST */
  *send_redir = 0;

  for (; bdptr[bdlen] && bdlen < (LINE_SIZE - 1); ++bdlen)
    file[bdlen] = bdptr[bdlen];
  if (bdlen < (LINE_SIZE - 1) && file[bdlen - 1] != '/') {
    file[bdlen++] = '/';
    file[bdlen] = 0;
  }
#ifdef VHOST
  if (!*vhost) strncpy(vhost, dvptr, LINE_SIZE);
  c = vhost[k];
  if (c == '%' && isxdigit(a = vhost[k + 1]) && isxdigit(b = vhost[k + 2])) {
    k += 3;
    if (a >= 'a') a -= 32;
    if (a >= 'A')
      a = 10 + a - 'A';
    else if (a >= '0')
      a -= '0';
    if (b >= 'a') b -= 32;
    if (b >= 'A')
      b = 10 + b - 'A';
    else if (b >= '0')
      b -= '0';
    c = (a << 4) + b;
  } else
    ++k;
  for (; k < LINE_SIZE && bdlen < (LINE_SIZE - 1) && c && c != ':';) {
    if (c == '/') {
      if (file[bdlen] == '.' || file[bdlen] == '/')
        ;
      else
        file[bdlen] = '/';
    } else if (c == '.') {
      if (file[bdlen] == '.' || file[bdlen] == '/')
        ;
      else
        file[bdlen] = '.';
    } else {
      if (file[bdlen] == '.' || file[bdlen] == '/') ++bdlen;
      file[bdlen++] = c;
      file[bdlen] = 0;
    }
    c = vhost[k];
    if (c == '%' && isxdigit(a = vhost[k + 1]) && isxdigit(b = vhost[k + 2])) {
      k += 3;
      if (a >= 'a') a -= 32;
      if (a >= 'A')
        a = 10 + a - 'A';
      else if (a >= '0')
        a -= '0';
      if (b >= 'a') b -= 32;
      if (b >= 'A')
        b = 10 + b - 'A';
      else if (b >= '0')
        b -= '0';
      c = (a << 4) + b;
    } else
      ++k;
  }
  if (bdlen < (LINE_SIZE - 1) && file[bdlen - 1] != '/') {
    file[bdlen++] = '/';
    file[bdlen] = 0;
  }
#endif /* VHOST */

  j = bdlen - 1;

  c = url[i];
  if (c == '%' && isxdigit(a = url[i + 1]) && isxdigit(b = url[i + 2])) {
    i += 3;
    if (a >= 'a') a -= 32;
    if (a >= 'A')
      a = 10 + a - 'A';
    else if (a >= '0')
      a -= '0';
    if (b >= 'a') b -= 32;
    if (b >= 'A')
      b = 10 + b - 'A';
    else if (b >= '0')
      b -= '0';
    c = (a << 4) + b;
  } else
    ++i;
  for (; i < LINE_SIZE && j < (LINE_SIZE - 1) && c && c != '?';) {
    reindex = i;
    if (c == '/') {
      if (file[j] == '.' || file[j] == '/')
        ;
      else
        file[j] = '/';
    } else if (c == '.') {
      if (file[j] == '.' || file[j] == '/')
        ;
      else
        file[j] = '.';
    } else {
      if (file[j] == '.' || file[j] == '/') ++j;
      file[j++] = c;
      file[j] = 0;
    }
    c = url[i];
    if (c == '%' && isxdigit(a = url[i + 1]) && isxdigit(b = url[i + 2])) {
      i += 3;
      if (a >= 'a') a -= 32;
      if (a >= 'A')
        a = 10 + a - 'A';
      else if (a >= '0')
        a -= '0';
      if (b >= 'a') b -= 32;
      if (b >= 'A')
        b = 10 + b - 'A';
      else if (b >= '0')
        b -= '0';
      c = (a << 4) + b;
    } else
      ++i;
  }
  file[++j] = 0;
  {
    DIR *dir = opendir(file);
    if (dir) {
      if (file[j - 1] == '/' || j == bdlen) {
        for (; dfptr[dflen]; ++dflen) {
          if (j < (LINE_SIZE - 1)) file[j++] = dfptr[dflen];
        }
        file[j] = 0;
      } else {
        *send_redir = 1;
        if (reindex < (LINE_SIZE - 1)) {
          url[reindex++] = '/';
          url[reindex] = 0;
        }
      }
    }
  }
}

void serve_invalid_version(fd *out) {
  Writeline(*out, "HTTP/1.0 505 HTTP Version Not Supported");
  Writeline(*out, "Connection: close");
  Writeline(*out, "");
}

void serve_invalid_method(fd *out) {
  Writeline(*out, "HTTP/1.0 405 Method Not Allowed");
  Writeline(*out, "Connection: close");
  Writeline(*out, "");
}

void serve_redirect(fd *out, char url[LINE_SIZE], int http_version) {
  char buff[LINE_SIZE];
  snprintf(buff, LINE_SIZE, "Location: %s", url);
  if (http_version == HTTP_VERSION_1_0) {
    Writeline(*out, "HTTP/1.0 301 Moved Permanently");
    Writeline(*out, "Connection: close");
  } else if (http_version == HTTP_VERSION_1_1) {
    Writeline(*out, "HTTP/1.1 301 Moved Permanently");
    Writeline(*out, "Content-Length: 0");
  }
  Writeline(*out, buff);
  Writeline(*out, "");
}

void serve_not_found(fd *out, int http_version) {
  if (http_version == HTTP_VERSION_1_0) {
    Writeline(*out, "HTTP/1.0 404 Not Found");
    Writeline(*out, "Connection: close");
  } else if (http_version == HTTP_VERSION_1_1) {
    Writeline(*out, "HTTP/1.1 404 Not Found");
    Writeline(*out, "Content-Length: 0");
  }
  Writeline(*out, "");
}

void serve_request(fd *out, char url[LINE_SIZE],
#ifdef VHOST
                   char vhost[LINE_SIZE],
#endif /* VHOST */
                   int start, int end, int http_version, int head_only) {
  int send_redir = 0;
  char filename[LINE_SIZE];
  char buff[LINE_SIZE];
  file_fd furl = -1;
  int flen = -1, clen = -1;
  int partial = 0;

#ifdef VHOST
  map_url(url, vhost, filename, &send_redir);
#else
  map_url(url, filename, &send_redir);
#endif /* VHOST */
  log_printf("URL: %s -> File: %s\n", url, filename);

  if (send_redir) {
    serve_redirect(out, url, http_version);
    return;
  }

#ifdef WINDOWS
  furl = open(filename, O_RDONLY | _O_BINARY);
#else
  furl = open(filename, O_RDONLY);
#endif /* WINDOWS */
  if (furl < 0) {
    serve_not_found(out, http_version);
    return;
  }

  flen = (uint32)lseek(furl, 0, SEEK_END);
  lseek(furl, 0, SEEK_SET);
  clen = flen;
  if (start >= 0) {
    if (start >= flen) start = flen - 1;
    if (end >= start && end < flen) {
    } else {
      end = flen - 1;
    }
    lseek(furl, start, SEEK_SET);
    ++partial;
    clen = (end - start + 1);
  } else if (end >= 0) {
    start = flen - end;
    end = flen - 1;
    lseek(furl, start, SEEK_SET);
    ++partial;
    clen = (end - start + 1);
  }
  if (http_version == HTTP_VERSION_1_0) {
    if (partial)
      Writeline(*out, "HTTP/1.0 206 Partial Content");
    else
      Writeline(*out, "HTTP/1.0 200 OK");
    Writeline(*out, "Connection: close");
  } else if (http_version == HTTP_VERSION_1_1) {
    if (partial)
      Writeline(*out, "HTTP/1.1 206 Partial Content");
    else
      Writeline(*out, "HTTP/1.1 200 OK");
    Writeline(*out, "Connection: keep-alive");
  }
  log_printf("Start: %d End: %d\n", start, end);
  log_printf("Content-Length: %d\n", clen);
  snprintf(buff, LINE_SIZE, "Content-Length: %d", clen);
  Writeline(*out, buff);
  if (partial) {
    log_printf("Content-Range: bytes %d-%d/%d\n", start, end, flen);
    snprintf(buff, LINE_SIZE, "Content-Range: bytes %d-%d/%d", start, end,
             flen);
    Writeline(*out, buff);
    Writeline(*out, "Accept-Ranges: bytes");
  }
  {
    char *ctype = get_content_type(filename);
    log_printf("Content-Type: %s\n", ctype);
    snprintf(buff, LINE_SIZE, "Content-Type: %s", ctype);
  }
  Writeline(*out, buff);

  Writeline(*out, "Cache-Control: no-cache");
  Writeline(*out, "");

  if (!head_only) Relayn(furl, *out, flen);

  Close(&furl);
}

int handle_me(fd *accept) {
  int persist_done;
  char buff[LINE_SIZE], url[LINE_SIZE], header[LINE_SIZE], value[LINE_SIZE];
#ifdef VHOST
  char vhost[LINE_SIZE];
#endif /* VHOST */
  int http_method, http_version;
  int start = -1, end = -1;

  do {
    persist_done = 1;
#ifdef VHOST
    *vhost = 0;
#endif /*VHOST */

    *buff = 0;
    Readline(*accept, buff, LINE_SIZE);

    if (!*buff) return NET_ERROR;
    parse_request(buff, &http_method, url, &http_version);

    *buff = 0;
    Readline(*accept, buff, LINE_SIZE);

    while (*buff) {
      parse_header(buff, header, value);
      log_printf("[%s]: %s\n", header, value);

      if (!strcmp(header, "Connection"))
        if (!strcmp(value, "close"))
          http_version = HTTP_VERSION_1_0; /* Force compatibility. */

      if (!strcmp(header, "Range")) get_range_values(value, &start, &end);

#ifdef VHOST
      if (!strcmp(header, "Host"))
        strncpy(vhost, value, LINE_SIZE * sizeof(char));
#endif /* VHOST */

      *buff = 0;
      Readline(*accept, buff, LINE_SIZE);
    }

    if (http_version == HTTP_VERSION_1_0) persist_done = 0;

    if (http_method == HTTP_INVALID) {
      serve_invalid_method(accept);
      persist_done = 0;
    } else if (http_version == HTTP_VERSION_INVALID) {
      serve_invalid_version(accept);
      persist_done = 0;
    } else if (http_method == HTTP_GET) {
#ifdef VHOST
      serve_request(accept, url, vhost, start, end, http_version, 0);
#else
      serve_request(accept, url, start, end, http_version, 0);
#endif /* VHOST */
    } else if (http_method == HTTP_HEAD) {
#ifdef VHOST
      serve_request(accept, url, vhost, start, end, http_version, 1);
#else
      serve_request(accept, url, start, end, http_version, 1);
#endif /* VHOST */
    }
  } while (persist_done && serving > 0);

  return NET_OK;
}

void interrupt_handler(int sig) {
  log_printf("Quitting\n");
  if (serving > 0)
    serving = -1;
  else {
    Close(&in);
    exit(0);
  }
}

int main() {
  fd accept;
#ifdef FORKED
  pid_t pid;
  int status;
#endif /* FORKED */

  if (Initialize() != NET_OK) {
    log_printf("Initialization Error\n");
    return -1;
  }

#ifdef FORKED
#ifndef DEBUG
  pid = fork();
  if (pid < 0) {
    log_printf("Fork Error\n");
    return -1;
  }
  if (!pid) {
    /* Disassociate with Parent. */
    setsid();
  } else {
    return 0;
  }
  {
    fd to_close;
    to_close = STDIN_FILENO;
    Close(&to_close);
    to_close = STDOUT_FILENO;
    Close(&to_close);
    to_close = STDERR_FILENO;
    Close(&to_close);
  }
#endif /* DEBUG */
#endif /* FORKED */

#ifdef FORKED
  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, interrupt_handler);
#endif /* FORKED */
  signal(SIGINT, interrupt_handler);
  signal(SIGTERM, interrupt_handler);

  srand(time(NULL));

  {
    char buff[LINE_SIZE];
    snprintf(buff, LINE_SIZE, "%u", SERV_PORT);

    if (Bind(&in, SERV_IP, buff) < 0) {
      log_printf("Bind Error\n");
      return -1;
    }
  }

  serving = 0;
  while (serving >= 0) {
    char host[MISC_SIZE], service[MISC_SIZE];
    if (Accept(&in, &accept, host, service) < 0) {
      log_printf("Accept Error\n");
      continue;
    }
    log_printf("Accepted: %s:%s\n", host, service);
#ifdef FORKED
    pid = fork();
    if (pid < 0) {
      printf("Fork Error\n");
      Close(&accept);
      continue;
    }
    if (!pid) {
      Close(&in);
      log_printf("Child: %d\n", getpid());
      serving = 1;
      status = handle_me(&accept);
      if (serving > 0) serving = 0;
      Close(&accept);
      return status;
    } else {
      Close(&accept);
      waitpid(pid, &status, WNOHANG);
    }
#else
    serving = 1;
    handle_me(&accept);
    if (serving > 0) serving = 0;
    Close(&accept);
#endif /* FORKED */
  }
  Close(&in);
  return 0;
}
