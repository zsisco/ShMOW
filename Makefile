CFLAGS+= -Wall
LDADD+= -lX11 -lm 
LDFLAGS=
EXEC=shmow

PREFIX?= /usr
BINDIR?= $(PREFIX)/bin

CC=gcc

all: $(EXEC)

shmow: shmow.o
	$(CC) $(LDFLAGS) -Os -o $@ $+ $(LDADD)

install: all
	install -Dm 755 shmow $(DESTDIR)$(BINDIR)/shmow

clean:
	rm -f shmow *.o
