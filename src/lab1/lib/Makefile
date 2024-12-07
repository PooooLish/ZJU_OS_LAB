C_SRC       = $(sort $(wildcard *.c))
OBJ		    = $(patsubst %.c,%.o,$(C_SRC))
CROSS_=riscv64-linux-gnu-
GCC=${CROSS_}gcc

all: $(OBJ)

%.o: %.c
	${GCC}  ${CFLAG} -c $<

clean:
	$(shell rm *.o 2>/dev/null)