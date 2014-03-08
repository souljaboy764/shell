CC = gcc
CFLAGS = -w
PROG = terminal

SRCS = bash.c

all: $(PROG)

$(PROG):	$(SRCS)
	$(CC) $(CFLAGS) -o $(PROG) $(SRCS) $(LIBS)

clean:
	rm -f $(PROG)
