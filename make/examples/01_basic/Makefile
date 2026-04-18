STAMPER = MKSTAMP.EXE
OUT := out.txt

.PHONY: all clean

all: $(OUT)

source.txt:
	$(STAMPER) source.txt source

$(OUT): source.txt
	$(STAMPER) $(OUT) product

clean:
	$(STAMPER) clean.log cleaned
