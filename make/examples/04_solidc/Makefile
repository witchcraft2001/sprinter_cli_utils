NAME = hello
SRC = $(NAME).c
TMC = $(NAME).tmc
ASM = $(NAME).asm
REL = $(NAME).rel
OUT = $(NAME)

.PHONY: all
.PHONY: clean

all: $(OUT)

$(TMC): $(SRC)
	cc1 -m $(SRC)

$(ASM): $(TMC)
	cc2 $(TMC)

$(REL): $(ASM)
	as $(ASM)

$(OUT): $(REL)
	ld $(OUT),clib/l/gXMAIN /x

clean:
	del $(OUT)
	del $(REL)
	del $(ASM)
	del $(TMC)
