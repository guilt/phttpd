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

#define strCaseCmp stricmp
#define prInt "%I64d"
#define scanIntLength(srcString, value) sscanf(srcString, prInt, value)

#else

#define strCaseCmp strcasecmp
#define prInt "%lld"
#define scanIntLength(srcString, value) sscanf(srcString, prInt, value)

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

static void parseRequest(char lineBuffer[LINE_SIZE], int *httpMethod, char url[LINE_SIZE],
                   int *httpVersion) {
  int i = 0, j = 0;
  if (!lineBuffer || !httpMethod || !url || !httpVersion) return;

  *httpMethod = HTTP_INVALID;
  *url = 0;
  *httpVersion = HTTP_VERSION_INVALID;

  for (; lineBuffer[i] && isspace(lineBuffer[i]); ++i)
    ;

  if (lineBuffer[i] == 'G' && lineBuffer[i + 1] == 'E' && lineBuffer[i + 2] == 'T') {
    *httpMethod = HTTP_GET;
    i += 3;
  }
  if (lineBuffer[i] == 'H' && lineBuffer[i + 1] == 'E' && lineBuffer[i + 2] == 'A' &&
      lineBuffer[i + 2] == 'D') {
    *httpMethod = HTTP_HEAD;
    i += 4;
  }
  if (lineBuffer[i] && !isspace(lineBuffer[i])) *httpMethod = HTTP_INVALID;

  if (*httpMethod == HTTP_INVALID) return;

  for (; lineBuffer[i] && isspace(lineBuffer[i]); ++i)
    ;

  for (; lineBuffer[i] && !isspace(lineBuffer[i]); ++i) url[j++] = lineBuffer[i];
  url[j] = 0;

  for (; lineBuffer[i] && isspace(lineBuffer[i]); ++i)
    ;

  if (lineBuffer[i] == 'H' && lineBuffer[i + 1] == 'T' && lineBuffer[i + 2] == 'T' &&
      lineBuffer[i + 3] == 'P') {
    if (lineBuffer[i + 4] == '/') {
      if (lineBuffer[i + 5] == '1' && lineBuffer[i + 6] == '.') {
        if (!lineBuffer[i + 8] || isspace(lineBuffer[i + 8])) {
          if (lineBuffer[i + 7] == '1') {
            *httpVersion = HTTP_VERSION_1_1;
          } else if (lineBuffer[i + 7] == '0') {
            *httpVersion = HTTP_VERSION_1_0;
          }
        }
      }
    }
  }
}

static void parseHeader(char lineBuffer[LINE_SIZE], char header[LINE_SIZE],
                  char value[LINE_SIZE]) {
  int i = 0, j = 0, k = 0;
  if (!lineBuffer || !header || !value) return;

  *header = 0;
  *value = 0;

  for (; lineBuffer[i] && isspace(lineBuffer[i]); ++i)
    ;

  for (; lineBuffer[i] && lineBuffer[i] != ':' && !isspace(lineBuffer[i]); ++i)
    header[j++] = lineBuffer[i];
  header[j] = 0;
  if (lineBuffer[i] == ':') ++i;

  for (; lineBuffer[i] && isspace(lineBuffer[i]); ++i)
    ;

  for (; lineBuffer[i]; ++i) value[k++] = lineBuffer[i];
  value[k] = 0;
}

static char *getContentType(char file[LINE_SIZE]) {
  int index = -1;
  int i = 0;
#define FILE_TYPES 70
  static char fileTypes[FILE_TYPES * 2][MISC_SIZE] = {

      "bat",  "application/x-bat",
      "7z",   "application/x-7z-compressed",
      "bz2",  "application/x-bzip2",
      "exe",  "application/octet-stream",
      "gz",   "application/x-gzip",
      "json", "application/json",
      "ps",   "application/postscript",
      "pdf",  "application/pdf",
      "rar",  "application/vnd.rar",
      "rm",   "application/vnd.rn-realmedia",
      "swf",  "application/x-shockwave-flash",
      "tar",  "application/x-tar",
      "xml",  "application/xml",
      "zip",  "application/zip",

      "wav",  "audio/x-wav",
      "mp3",  "audio/mpeg3",
      "m4a",  "audio/mp4",
      "mp4",  "video/mp4",
      "opus", "audio/ogg",
      "ram",  "audio/vnd.rn-realaudio",
      "rpm",  "audio/x-pn-realaudio-plugin",
      "snd",  "audio/basic",

      "avi",  "video/x-msvideo",
      "flv",  "video/flv",
      "mkv",  "video/x-matroska",
      "mov",  "video/quicktime",
      "mp4",  "video/mp4",
      "rv",   "video/vnd.rn-realvideo",
      "wmv",  "video/x-ms-wmv",
      "webm", "video/webm",

      "avif", "image/avif",
      "bmp",  "image/bmp",
      "gif",  "image/gif",
      "ico",  "image/vnd.microsoft.icon",
      "jpg",  "image/jpeg",
      "jpeg", "image/jpeg",
      "png",  "image/png",
      "svg",  "image/svg+xml",
      "tif",  "image/tiff",
      "tiff", "image/tiff",
      "webp", "image/webp",

      "bash", "text/x-shellscript",
      "c",    "text/plain",
      "cc",   "text/plain",
      "cpp",  "text/plain",
      "css",  "text/css",
      "cu",   "text/plain",
      "h",    "text/plain",
      "hip",  "text/plain",
      "hpp",  "text/plain",
      "htm",  "text/html",
      "html", "text/html",
      "java", "text/plain",
      "js",   "text/javascript",
      "md",   "text/markdown",
      "go",   "text/plain",
      "rs",   "text/plain",
      "s",    "text/plain",
      "pl",   "text/plain",
      "pm",   "text/plain",
      "ps1",  "text/plain",
      "py",   "text/plain",
      "txt",  "text/plain",
      "sh",   "text/x-shellscript",
      "zsh",  "text/x-shellscript",

      "ttf",  "font/ttf",
      "eot",  "font/eot",
      "otf",  "font/otf",
      "woff", "font/woff",
      "woff2","font/woff2",
  };
  while (i < LINE_SIZE && file[i]) {
    if (file[i] == '.') index = i;
    ++i;
  }
  if (index >= 0) {
    ++index;
    int f = 0, kf = 0;
    for (; f < FILE_TYPES; ++f, kf += 2) {
      if (!strCaseCmp(file + index, fileTypes[kf])) return fileTypes[kf + 1];
    }
  }
  return "content/unknown";
}

static void getRangeValues(char value[LINE_SIZE], intlen *start, intlen *end) {
  char *valuePtr = value;
  *start = -1;
  *end = -1;
  if (strncmp(valuePtr, "bytes", 5)) return;
  valuePtr += 5;
  if (!isdigit(*valuePtr)) ++valuePtr;
  if (!*valuePtr) return;
  if (!scanIntLength(valuePtr, start)) return;
  for (; *valuePtr && *valuePtr != '-'; ++valuePtr)
    ;
  if (!*valuePtr) return;
  ++valuePtr;
  if (!*valuePtr) return;
  if (!scanIntLength(valuePtr, end)) return;
}

static void mapURL(char url[LINE_SIZE],
#ifdef VHOST
             char vhost[LINE_SIZE],
#endif /* VHOST */
             char file[LINE_SIZE], int *sendRedir) {
  int a, b, i = 0, j = 0;
  int reIndex = 0, baseDirLength = 0, dataFileLength = 0;
  char c, *baseDirPtr = BASE_DIR, *dataFilePtr = DEFAULT_FILE;
#ifdef VHOST
  int k = 0;
  char *dvaluePtr = DEFAULT_VHOST;
#endif /* VHOST */

  if (!url || !*url || !file || !sendRedir) return;
#ifdef VHOST
  if (!vhost) return;
#endif /* VHOST */
  *sendRedir = 0;

  for (; baseDirPtr[baseDirLength] && baseDirLength < (LINE_SIZE - 1); ++baseDirLength)
    file[baseDirLength] = baseDirPtr[baseDirLength];
  if (baseDirLength < (LINE_SIZE - 1) && file[baseDirLength - 1] != '/') {
    file[baseDirLength++] = '/';
    file[baseDirLength] = 0;
  }
#ifdef VHOST
  if (!*vhost) strncpy(vhost, dvaluePtr, LINE_SIZE);
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
  for (; k < LINE_SIZE && baseDirLength < (LINE_SIZE - 1) && c && c != ':';) {
    if (c == '/') {
      if (file[baseDirLength] == '.' || file[baseDirLength] == '/')
        ;
      else
        file[baseDirLength] = '/';
    } else if (c == '.') {
      if (file[baseDirLength] == '.' || file[baseDirLength] == '/')
        ;
      else
        file[baseDirLength] = '.';
    } else {
      if (file[baseDirLength] == '.' || file[baseDirLength] == '/') ++baseDirLength;
      file[baseDirLength++] = c;
      file[baseDirLength] = 0;
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
  if (baseDirLength < (LINE_SIZE - 1) && file[baseDirLength - 1] != '/') {
    file[baseDirLength++] = '/';
    file[baseDirLength] = 0;
  }
#endif /* VHOST */

  j = baseDirLength - 1;

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
    reIndex = i;
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
      if (file[j - 1] == '/' || j == baseDirLength) {
        for (; dataFilePtr[dataFileLength]; ++dataFileLength) {
          if (j < (LINE_SIZE - 1)) file[j++] = dataFilePtr[dataFileLength];
        }
        file[j] = 0;
      } else {
        *sendRedir = 1;
        if (reIndex < (LINE_SIZE - 1)) {
          url[reIndex++] = '/';
          url[reIndex] = 0;
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

static void serveRedirect(fd *out, char url[LINE_SIZE], int httpVersion) {
  char lineBuffer[LINE_SIZE];
  snprintf(lineBuffer, LINE_SIZE, "Location: %s", url);
  if (httpVersion == HTTP_VERSION_1_0) {
    Writeline(*out, "HTTP/1.0 301 Moved Permanently");
    Writeline(*out, "Connection: close");
  } else if (httpVersion == HTTP_VERSION_1_1) {
    Writeline(*out, "HTTP/1.1 301 Moved Permanently");
    Writeline(*out, "Content-Length: 0");
  }
  Writeline(*out, lineBuffer);
  Writeline(*out, "");
}

static void serveNotFound(fd *out, int httpVersion) {
  if (httpVersion == HTTP_VERSION_1_0) {
    Writeline(*out, "HTTP/1.0 404 Not Found");
    Writeline(*out, "Connection: close");
  } else if (httpVersion == HTTP_VERSION_1_1) {
    Writeline(*out, "HTTP/1.1 404 Not Found");
    Writeline(*out, "Content-Length: 0");
  }
  Writeline(*out, "");
}

static void serveRequest(fd *out, char url[LINE_SIZE],
#ifdef VHOST
  char vhost[LINE_SIZE],
#endif /* VHOST */
  intlen start, intlen end, int httpVersion, int headOnly) {
  int sendRedir = 0;
  char filename[LINE_SIZE];
  char lineBuffer[LINE_SIZE];
  file_fd furl = -1;
  intlen fileLength = -1, responseLength = -1;
  int partial = 0, partialUnsatisfiable = 0;

#ifdef VHOST
  mapURL(url, vhost, filename, &sendRedir);
#else
  mapURL(url, filename, &sendRedir);
#endif /* VHOST */
  logPrintf("URL: %s -> File: %s", url, filename);

  if (sendRedir) {
    serveRedirect(out, url, httpVersion);
    return;
  }

#ifdef WINDOWS
  furl = open(filename, O_RDONLY | _O_BINARY);
#else
  furl = open(filename, O_RDONLY);
#endif /* WINDOWS */
  if (furl < 0) {
    serveNotFound(out, httpVersion);
    return;
  }

  fileLength = lseek(furl, 0, SEEK_END);
  lseek(furl, 0, SEEK_SET);
  responseLength = fileLength;

  logPrintf("Start: " prInt " End: " prInt, start, end);
  if (start >= 0) {
    partial = 1;

    if (start >= fileLength) {
      partialUnsatisfiable = 1;
    } else if (end < 0) {
      end = fileLength - 1;
    } else if (end >= start && end < fileLength) {
    }

    if(!partialUnsatisfiable) {
      lseek(furl, start, SEEK_SET);
      responseLength = (end - start + 1);
    }
  } else if (end >= 0) {
    partial = 1;

    start = fileLength - end;
    end = fileLength - 1;

    lseek(furl, start, SEEK_SET);
    responseLength = (end - start + 1);
  }

  if(partialUnsatisfiable) {
      logPrintf("Unsatifiable, Length: " prInt, fileLength);
      Writeline(*out, "HTTP/1.0 416 Range not Satisfiable");
      Writeline(*out, "Connection: close");
      Writeline(*out, "");
      Close(&furl);

      return;
  }

  if(partial) {
      if (httpVersion == HTTP_VERSION_1_0) {
        Writeline(*out, "HTTP/1.0 206 Partial Content");
      } else if (httpVersion == HTTP_VERSION_1_1) {
        Writeline(*out, "HTTP/1.1 206 Partial Content");
      }
  } else {
      if (httpVersion == HTTP_VERSION_1_0) {
        Writeline(*out, "HTTP/1.0 200 OK");
      } else if (httpVersion == HTTP_VERSION_1_1) {
        Writeline(*out, "HTTP/1.1 200 OK");
      }
  }

  if (httpVersion == HTTP_VERSION_1_0) {
    Writeline(*out, "Connection: close");
  } else if (httpVersion == HTTP_VERSION_1_1) {
    Writeline(*out, "Connection: keep-alive");
  }

  {
  logPrintf("Content-Length: " prInt, responseLength);
  snprintf(lineBuffer, LINE_SIZE, "Content-Length: " prInt, responseLength);
  Writeline(*out, lineBuffer);
  }

  if (partial) {
    logPrintf("Content-Range: bytes "prInt "-" prInt "/" prInt, start, end, fileLength);
    snprintf(lineBuffer, LINE_SIZE, "Content-Range: bytes " prInt "-" prInt "/" prInt, start, end, fileLength);
    Writeline(*out, lineBuffer);
    Writeline(*out, "Accept-Ranges: bytes");
  }

  {
    char *ctype = getContentType(filename);
    logPrintf("Content-Type: %s", ctype);
    snprintf(lineBuffer, LINE_SIZE, "Content-Type: %s", ctype);
    Writeline(*out, lineBuffer);
  }

  Writeline(*out, "Cache-Control: no-cache");
  Writeline(*out, "");

  if (!headOnly) Relayn(furl, *out, responseLength);

  Close(&furl);
}

static int handleMe(fd *accept) {
  int persistDone;
  char lineBuffer[LINE_SIZE], url[LINE_SIZE], header[LINE_SIZE], value[LINE_SIZE];
#ifdef VHOST
  char vhost[LINE_SIZE];
#endif /* VHOST */
  int httpMethod, httpVersion;
  intlen start = -1, end = -1;

  do {
    persistDone = 1;
#ifdef VHOST
    *vhost = 0;
#endif /*VHOST */

    *lineBuffer = 0;
    Readline(*accept, lineBuffer, LINE_SIZE);

    if (!*lineBuffer) return NET_ERROR;
    parseRequest(lineBuffer, &httpMethod, url, &httpVersion);

    *lineBuffer = 0;
    Readline(*accept, lineBuffer, LINE_SIZE);

    while (*lineBuffer) {
      parseHeader(lineBuffer, header, value);
      logPrintf("[%s]: %s", header, value);

      if (!strcmp(header, "Connection"))
        if (!strcmp(value, "close"))
          httpVersion = HTTP_VERSION_1_0; /* Force compatibility. */

      if (!strcmp(header, "Range")) getRangeValues(value, &start, &end);

#ifdef VHOST
      if (!strcmp(header, "Host"))
        strncpy(vhost, value, LINE_SIZE * sizeof(char));
#endif /* VHOST */

      *lineBuffer = 0;
      Readline(*accept, lineBuffer, LINE_SIZE);
    }

    if (httpVersion == HTTP_VERSION_1_0) persistDone = 0;

    if (httpMethod == HTTP_INVALID) {
      serveInvalidMethod(accept);
      persistDone = 0;
    } else if (httpVersion == HTTP_VERSION_INVALID) {
      serveInvalidVersion(accept);
      persistDone = 0;
    } else if (httpMethod == HTTP_GET) {
#ifdef VHOST
      serveRequest(accept, url, vhost, start, end, httpVersion, 0);
#else
      serveRequest(accept, url, start, end, httpVersion, 0);
#endif /* VHOST */
    } else if (httpMethod == HTTP_HEAD) {
#ifdef VHOST
      serveRequest(accept, url, vhost, start, end, httpVersion, 1);
#else
      serveRequest(accept, url, start, end, httpVersion, 1);
#endif /* VHOST */
    }
  } while (persistDone);

  return NET_OK;
}

void interruptHandler(int sig) {
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
    fd toClose;
    toClose = STDIN_FILENO;
    Close(&toClose);
    toClose = STDOUT_FILENO;
    Close(&toClose);
    toClose = STDERR_FILENO;
    Close(&toClose);
  }
#endif /* DEBUG */
#endif /* FORKED */

#ifdef FORKED
  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, interruptHandler);
#endif /* FORKED */
  signal(SIGINT, interruptHandler);
  signal(SIGTERM, interruptHandler);

  srand(time(NULL));

  {
    char lineBuffer[LINE_SIZE];
    snprintf(lineBuffer, LINE_SIZE, "%u", SERV_PORT);

    if (Bind(&in, SERV_IP, lineBuffer) < 0) {
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
