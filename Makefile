CC = gcc
CFLAGS = -w
PROG = shell

SRCS = shell.c

all: $(PROG)

$(PROG):	$(SRCS)
	$(CC) $(CFLAGS) -o $(PROG) $(SRCS) $(LIBS)

clean:
	rm -f $(PROG)
