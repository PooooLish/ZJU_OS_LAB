# lab1/lib/Makefile
C_SRC       = $(wildcard *.c)
OBJ	     = $(patsubst %.c,%.o,$(C_SRC))
GCC	     = riscv64-linux-gnu-gcc

all:$(OBJ)
	
%.o:%.c
	${GCC} ${CFLAG} -c $<
clean:
	$(shell rm *.o)