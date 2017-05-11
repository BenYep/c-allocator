# A simple makefile for managing build of project composed of C source files.
#
# Julie Zelenski, for CS107, April 2009
#

# It is likely that default C compiler is already gcc, but explicitly
# set, just to be sure
CC = gcc

# The CFLAGS variable sets compile flags for gcc:
#  -g          compile with debug information
#  -Wall       give verbose compiler warnings
#  -O0         do not optimize generated code
#  -std=gnu99  use the Gnu C99 standard language definition
#  -m32        compile for IA32 (even on 64-bit arch)
#  -pedantic   enforce ANSI compliance
CFLAGS = -g -Wall -O3 -std=gnu99 -m32 -pedantic

# The LDFLAGS variable sets flags for linker
#  -m32	  link with IA32 libraries
LDFLAGS = -m32

HEADERS = fcyc.h segment.h allocator.h
SOURCES = fcyc.c segment.c allocator.c test-allocator.c simple.c
OBJECTS = $(SOURCES:.c=.o)

TARGETS = test-allocator simple

all: $(TARGETS)

test-allocator:test-allocator.o allocator.o segment.o fcyc.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

simple: allocator.o segment.o simple.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# In make's default rules, a .o automatically depends on its .c file
# (so editing the .c will cause recompilation into its .o file).
# The line below creates additional dependencies, most notably that it
# will cause the .c to reocmpiled if any included .h file changes.

Makefile.dependencies:: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -MM $(SOURCES) > Makefile.dependencies

-include Makefile.dependencies

# Phony means not a "real" target, it doesn't build anything
# The phony target "clean" that is used to remove all compiled object files.

.PHONY: clean

clean:
	@rm -f $(TARGETS) $(OBJECTS) core Makefile.dependencies