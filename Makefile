#

CFLAGS=-Wall -g
PROGS=vdidiag
SRC=$(wildcard *.c)
INC=$(wildcard *.h)

all:	$(PROGS)

$(PROGS).o: $(SRC) $(INC)

clean:
	rm -f *~ *.o $(PROGS)
