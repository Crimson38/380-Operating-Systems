CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -Wconversion -Werror -Wno-format-truncation -std=c11 -O2
CPPFLAGS :=

#Part 1
CPPFLAGS += -DMALLOC_LAB_IMPLICIT
#Part 2
#CPPFLAGS += -DMALLOC_LAB_EXPLICIT
#Part 3
#CPPFLAGS += -DMALLOC_LAB_SEG

SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

.PHONY : clean run

mdriver: $(OBJS)
	$(LINK.c) $(OBJS) -o mdriver

run : mdriver
	./mdriver -gvV

clock.o: clock.c clock.h
fcyc.o: fcyc.c clock.h fcyc.h
fsecs.o: fsecs.c fsecs.h fcyc.h clock.h ftimer.h config.h
ftimer.o: ftimer.c ftimer.h
mdriver.o: mdriver.c config.h fsecs.h memlib.h mm.h
memlib.o: memlib.c config.h memlib.h
mm.o: mm.c memlib.h mm.h

clean:
	rm -f *~ *.o mdriver
