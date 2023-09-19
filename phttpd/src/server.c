#include <netlib.h>

#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#ifdef UNIX
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif /* UNIX */

#ifdef WINDOWS
#define strcasecmp stricmp
#endif /* WINDOWS */

#ifdef DEBUG
#define logPrintf(...)                                     \
  printf("%s:%d:%s(): ", __FILE__, __LINE__, __FUNCTION__); \
  printf(__VA_ARGS__);                                      \
  printf("\n");                                             \
  fflush(stdout)
#else
#define logPrintf(...)
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

static fd in;

static void parseRequest(char buff[LINE_SIZE], int *http_method, char url[LINE_SIZE],
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

static void parseHeader(char buff[LINE_SIZE], char header[LINE_SIZE],
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

static char *getContentType(char file[LINE_SIZE]) {
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

static void getRangeValues(char value[LINE_SIZE], intlen *start, intlen *end) {
  char *vptr = value;
  *start = -1;
  *end = -1;
  if (strncmp(vptr, "bytes", 5)) return;
  vptr += 5;
  if (!isdigit(*vptr)) ++vptr;
  if (!*vptr) return;
  if (!sscanf(vptr, "%lld", start)) return;
  for (; *vptr && *vptr != '-'; ++vptr)
    ;
  if (!*vptr) return;
  ++vptr;
  if (!*vptr) return;
  sscanf(vptr, "%lld", end);
}

static void mapURL(char url[LINE_SIZE],
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

static void serveInvalidVersion(fd *out) {
  Writeline(*out, "HTTP/1.0 505 HTTP Version Not Supported");
  Writeline(*out, "Connection: close");
  Writeline(*out, "");
}

static void serveInvalidMethod(fd *out) {
  Writeline(*out, "HTTP/1.0 405 Method Not Allowed");
  Writeline(*out, "Connection: close");
  Writeline(*out, "");
}

static void serveRedirect(fd *out, char url[LINE_SIZE], int http_version) {
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

static void serveNotFound(fd *out, int http_version) {
  if (http_version == HTTP_VERSION_1_0) {
    Writeline(*out, "HTTP/1.0 404 Not Found");
    Writeline(*out, "Connection: close");
  } else if (http_version == HTTP_VERSION_1_1) {
    Writeline(*out, "HTTP/1.1 404 Not Found");
    Writeline(*out, "Content-Length: 0");
  }
  Writeline(*out, "");
}

static void serveRequest(fd *out, char url[LINE_SIZE],
#ifdef VHOST
                   char vhost[LINE_SIZE],
#endif /* VHOST */
                   intlen start, intlen end, int http_version, int head_only) {
  int send_redir = 0;
  char filename[LINE_SIZE];
  char buff[LINE_SIZE];
  file_fd furl = -1;
  intlen flen = -1, clen = -1;
  int partial = 0, partial_unsatisfiable = 0;

#ifdef VHOST
  mapURL(url, vhost, filename, &send_redir);
#else
  mapURL(url, filename, &send_redir);
#endif /* VHOST */
  logPrintf("URL: %s -> File: %s", url, filename);

  if (send_redir) {
    serveRedirect(out, url, http_version);
    return;
  }

#ifdef WINDOWS
  furl = open(filename, O_RDONLY | _O_BINARY);
#else
  furl = open(filename, O_RDONLY);
#endif /* WINDOWS */
  if (furl < 0) {
    serveNotFound(out, http_version);
    return;
  }

  flen = lseek(furl, 0, SEEK_END);
  lseek(furl, 0, SEEK_SET);
  clen = flen;

  logPrintf("Start: %lld End: %lld", start, end);
  if (start >= 0) {
    partial = 1;

    if (start >= flen) {
      partial_unsatisfiable = 1;
    } else if (end < 0) {
      end = flen - 1;
    } else if (end >= start && end < flen) {
    }

    if(!partial_unsatisfiable) {
      lseek(furl, start, SEEK_SET);
      clen = (end - start + 1);
    }
  } else if (end >= 0) {
    partial = 1;

    start = flen - end;
    end = flen - 1;

    lseek(furl, start, SEEK_SET);
    clen = (end - start + 1);
  }

  if(partial_unsatisfiable) {
      logPrintf("Unsatifiable, Length: %lld", flen);
      Writeline(*out, "HTTP/1.0 416 Range not Satisfiable");
      Writeline(*out, "Connection: close");
      Writeline(*out, "");
      Close(&furl);

      return;
  }

  if(partial) {
      if (http_version == HTTP_VERSION_1_0) {
        Writeline(*out, "HTTP/1.0 206 Partial Content");
      } else if (http_version == HTTP_VERSION_1_1) {
        Writeline(*out, "HTTP/1.1 206 Partial Content");
      }
  } else {
      if (http_version == HTTP_VERSION_1_0) {
        Writeline(*out, "HTTP/1.0 200 OK");
      } else if (http_version == HTTP_VERSION_1_1) {
        Writeline(*out, "HTTP/1.1 200 OK");
      }
  }

  if (http_version == HTTP_VERSION_1_0) {
    Writeline(*out, "Connection: close");
  } else if (http_version == HTTP_VERSION_1_1) {
    Writeline(*out, "Connection: keep-alive");
  }

  {
  logPrintf("Content-Length: %lld", clen);
  snprintf(buff, LINE_SIZE, "Content-Length: %lld", clen);
  Writeline(*out, buff);
  }

  if (partial) {
    logPrintf("Content-Range: bytes %lld-%lld/%lld", start, end, flen);
    snprintf(buff, LINE_SIZE, "Content-Range: bytes %lld-%lld/%lld", start, end,
             flen);
    Writeline(*out, buff);
    Writeline(*out, "Accept-Ranges: bytes");
  }

  {
    char *ctype = getContentType(filename);
    logPrintf("Content-Type: %s", ctype);
    snprintf(buff, LINE_SIZE, "Content-Type: %s", ctype);
    Writeline(*out, buff);
  }

  Writeline(*out, "Cache-Control: no-cache");
  Writeline(*out, "");

  if (!head_only) Relayn(furl, *out, flen);

  Close(&furl);
}

static int handleMe(fd *accept) {
  int persist_done;
  char buff[LINE_SIZE], url[LINE_SIZE], header[LINE_SIZE], value[LINE_SIZE];
#ifdef VHOST
  char vhost[LINE_SIZE];
#endif /* VHOST */
  int http_method, http_version;
  intlen start = -1, end = -1;

  do {
    persist_done = 1;
#ifdef VHOST
    *vhost = 0;
#endif /*VHOST */

    *buff = 0;
    Readline(*accept, buff, LINE_SIZE);

    if (!*buff) return NET_ERROR;
    parseRequest(buff, &http_method, url, &http_version);

    *buff = 0;
    Readline(*accept, buff, LINE_SIZE);

    while (*buff) {
      parseHeader(buff, header, value);
      logPrintf("[%s]: %s", header, value);

      if (!strcmp(header, "Connection"))
        if (!strcmp(value, "close"))
          http_version = HTTP_VERSION_1_0; /* Force compatibility. */

      if (!strcmp(header, "Range")) getRangeValues(value, &start, &end);

#ifdef VHOST
      if (!strcmp(header, "Host"))
        strncpy(vhost, value, LINE_SIZE * sizeof(char));
#endif /* VHOST */

      *buff = 0;
      Readline(*accept, buff, LINE_SIZE);
    }

    if (http_version == HTTP_VERSION_1_0) persist_done = 0;

    if (http_method == HTTP_INVALID) {
      serveInvalidMethod(accept);
      persist_done = 0;
    } else if (http_version == HTTP_VERSION_INVALID) {
      serveInvalidVersion(accept);
      persist_done = 0;
    } else if (http_method == HTTP_GET) {
#ifdef VHOST
      serveRequest(accept, url, vhost, start, end, http_version, 0);
#else
      serveRequest(accept, url, start, end, http_version, 0);
#endif /* VHOST */
    } else if (http_method == HTTP_HEAD) {
#ifdef VHOST
      serveRequest(accept, url, vhost, start, end, http_version, 1);
#else
      serveRequest(accept, url, start, end, http_version, 1);
#endif /* VHOST */
    }
  } while (persist_done);

  return NET_OK;
}

void interrupt_handler(int sig) {
  logPrintf("Interrupted ...");
  Close(&in);
  logPrintf("Bind Closed.");
}

int main() {
  fd accept;
#ifdef FORKED
  pid_t pid;
  int status;
#endif /* FORKED */

  if (Initialize() != NET_OK) {
    logPrintf("Initialization Error");
    return -1;
  }

#ifdef FORKED
#ifndef DEBUG
  pid = fork();
  if (pid < 0) {
    logPrintf("Fork Error");
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
      logPrintf("Bind Error");
      return -1;
    }
  }

  while(in >= 0) {
    char host[MISC_SIZE], service[MISC_SIZE];
    logPrintf("Accept ...");
    if (Accept(&in, &accept, host, service) < 0) {
      logPrintf("Accept Error.");
      continue;
    }
    logPrintf("Accepted: %s:%s", host, service);
#ifdef FORKED
    pid = fork();
    if (pid < 0) {
      logPrintf("Fork Error");
      Close(&accept);
      continue;
    }
    if (!pid) {
      Close(&in);
      logPrintf("Child: %d", getpid());
      status = handleMe(&accept);
      Close(&accept);
      return status;
    } else {
      Close(&accept);
      waitpid(pid, &status, WNOHANG);
    }
#else
    handleMe(&accept);
    Close(&accept);
#endif /* FORKED */
  }
  Close(&in);
  return 0;
}
