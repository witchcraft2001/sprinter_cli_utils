NAME = hello
SRC = $(NAME).c
TMC = $(NAME).tmc
ASM = $(NAME).asm
REL = $(NAME).rel
OUT = $(NAME).EXE
DELTOOL = MKDEL.EXE

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
	ld $(REL),clib/l/gXMAIN /x

clean:
	-$(DELTOOL) $(OUT)
	-$(DELTOOL) $(REL)
	-$(DELTOOL) $(ASM)
	-$(DELTOOL) $(TMC)
