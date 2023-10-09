CFLAGS := -Wall -Wextra -Werror -std=c99 -pedantic

SRCS := $(wildcard lib/*.c)
OBJS := $(SRCS:.c=.o)

all: CFLAGS += -O2
all: libmdview.a mdv
	mkdir -p out
	mv libmdview.a out
	mv mdv out
	cp lib/mdview.h out

debug: CFLAGS += -g -O0 -fsanitize=address -fsanitize=undefined
debug: libmdview.a mdv
	mkdir -p out
	mv libmdview.a out
	mv mdv out
	cp lib/mdview.h out

%.o: %.c
	$(CC) $(CFLAGS) -fvisibility=hidden -c -o $@ $<

libmdview.a: $(OBJS)
	$(AR) rcs libmdview.a $(OBJS)

mdv: libmdview.a mdv.c
	$(CC) $(CFLAGS) -L. -o mdv mdv.c -lmdview

.PHONY: clean
clean:
	rm -rf $(OBJS) *.dSYM out/
