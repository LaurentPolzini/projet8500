CC=gcc
CFLAGS=-std=c99 -Wextra -Wall -Werror -pedantic
LDFLAGS=-lm

ifeq ($(DEBUG),yes)
	CFLAGS += -g
	LDFLAGS +=
else
	CFLAGS += -O3 -DNDEBUG
	LDFLAGS +=
endif

EXEC=main
SRC= $(wildcard *.c)
OBJ= $(SRC:.c=.o)

all: 
ifeq ($(DEBUG),yes)
	@echo "Generating in debug mode"
else
	@echo "Generating in release mode"
endif
	@$(MAKE) $(EXEC)

$(EXEC): $(OBJ)
	@$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	@$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean mrproper

clean:
	@rm -rf *.o $(EXEC) *.exe
	
main.o: pannel.h calculateur.h afdx.h a429.h
pannel.o: pannel.h
calculateur.o: calculateur.h pannel.h
agregateur.o: agregateur.h calculateur.h
a429.o: a429.h
afdx.o: afdx.h
testsPannel.o: testsPannel.c pannel.h
testsCalculateur.o: testsCalculateur.c calculateur.h pannel.h
tests_a429.o: tests_a429.c a429.h
tests_afdx.o: tests_afdx.c afdx.h
