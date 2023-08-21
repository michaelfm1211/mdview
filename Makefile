CFLAGS := -Wall -Wextra -Werror -std=c99 -pedantic

all: CFLAGS += -O2
all: libmdview.a mdv

debug: CFLAGS += -g -O0 -fsanitize=address -fsanitize=undefined
debug: libmdview.a mdv

libmdview.a: mdview.c mdview.h
	$(CC) $(CFLAGS) -c -o mdview.o mdview.c
	$(AR) rcs libmdview.a mdview.o

mdv: libmdview.a mdv.c
	$(CC) $(CFLAGS) -L. -lmdview -o mdv mdv.c

.PHONY: clean
clean:
	rm -f *.o *.a mdview
