CC := gcc
RM := rm -f
CFLAGS += -O2 -Wall
CFLAGS += -DSERV_PORT=8080

ifdef DEBUG
	CFLAGS += -DDEBUG
endif

ifeq ($(OS),Windows_NT)
	CFLAGS += -DWINDOWS -I ../netlibmini/include -I include/windows
	LDFLAGS += -lnetlib -lws2_32 -L ../netlibmini
	EXE := .exe
else
	UNAME := $(shell uname)
	CFLAGS += -DUNIX -DFORKED -I ../netlibmini/include
	LDFLAGS += -lnetlib -L ../netlibmini
	ifeq ($(UNAME), Solaris)
		LDFLAGS += -lsocket
	endif
	EXE =
endif

BIN = phttpd_s$(EXE)
VBIN = phttpd_v$(EXE)

TARGETS = $(VBIN) $(BIN)

all: $(TARGETS)

clean:
	$(RM) $(TARGETS)

$(VBIN): src/server.c
	$(CC) -o $@ -s $< -DBASE_DIR=\"./vhosts\" -DVHOST $(CFLAGS) $(LDFLAGS)

$(BIN): src/server.c
	$(CC) -o $@ -s $< -DBASE_DIR=\"./htdocs\" $(CFLAGS) $(LDFLAGS)
