CXX = g++
CFLAGS = -Wall -Werror
CFLAGS += -g
#CFLAGS += -O3
LIBS    =

CORE = bdfe
OBJS = main.o bdf.o rterm.o
HFILES = Makefile rterm.h
CFILES = bdf.c rterm.c main.c

all: $(CORE)

$(CORE): $(OBJS) $(CFILES) $(HFILES)
	$(CXX) $(CFLAGS) -o $(CORE) $(OBJS) $(LIBS)

clean:
	rm -f $(CORE)
	rm -f *.o

%.o: %.c $(HFILES)
	$(CXX) -c $(CFLAGS) -DOSSD_TARGET=OSSD_IF_LINUX $< -o $@


