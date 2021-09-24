# Makefile for wbox
# Copyright (C) 2007 Salvatore Sanfilippo <antirez@invece.org>
# All Rights Reserved
# Under the GPL license version 2

DEBUG?= -g
CFLAGS?= -O2 -Wall -W -DSDS_ABORT_ON_OOM
CCOPT= $(CFLAGS)

OBJ = adlist.o ae.o anet.o dict.o redis.o sds.o picol.o
PRGNAME = redis-server

all: redis-server

# Deps (use make dep to generate this)
picol.o: picol.c picol.h
adlist.o: adlist.c adlist.h
ae.o: ae.c ae.h
anet.o: anet.c anet.h
dict.o: dict.c dict.h
redis.o: redis.c ae.h sds.h anet.h dict.h adlist.h
sds.o: sds.c sds.h

redis-server: $(OBJ)
	$(CC) -o $(PRGNAME) $(CCOPT) $(DEBUG) $(OBJ)
	@echo ""
	@echo "Hint: To run the test-redis.tcl script is a good idea."
	@echo "Launch the redis server with ./redis-server, then in another"
	@echo "terminal window enter this directory and run 'make test'."
	@echo ""

.c.o:
	$(CC) -c $(CCOPT) $(DEBUG) $(COMPILE_TIME) $<

clean:
	rm -rf $(PRGNAME) *.o

dep:
	$(CC) -MM *.c

test:
	tclsh test-redis.tcl
