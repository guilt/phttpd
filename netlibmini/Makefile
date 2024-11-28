CC ?= gcc
AR ?= ar
RM ?= rm -f
MKDIR ?= mkdir -p

CFLAGS += -O2 -Wall $(DISTCFLAGS)

ifdef DEBUG
	CFLAGS += -DDEBUG
endif

ifeq ($(OS),Windows_NT)
	CFLAGS += -DWINDOWS
else
	CFLAGS += -DUNIX
endif

CFLAGS += -D_FILE_OFFSET_BITS=64

OBJECTS=obj/netlib.o
TARGETS=libnetlib.a

all: obj $(TARGETS)

clean:
	$(RM) $(TARGETS) $(OBJECTS)

obj: 
	$(MKDIR) obj

libnetlib.a: $(OBJECTS)
	$(AR) rs $@ $<

obj/%.o: src/%.c
	$(CC) -I include/ -o $@ -c $< -DNET_CR $(CFLAGS)
