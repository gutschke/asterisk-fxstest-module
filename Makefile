CPPFLAGS=-Wall -Wextra -Werror -Wno-unused-parameter -D_GNU_SOURCE \
         -DAST_MODULE=\"fxstest\"
CFLAGS=-g -O3 -std=gnu99 -fPIC
LDFLAGS=

all: fxstest.o fxstest.so

install: fxstest.so
	sudo cp fxstest.so /usr/lib/asterisk/modules/

clean:
	$(RM) fxstest.o fxstest.so

.c.o:
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $<

fxstest.so: fxstest.o
	$(LD) -o $@ -shared -soname $@ $<
