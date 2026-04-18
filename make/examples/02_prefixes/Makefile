STAMPER = MKSTAMP.EXE
FAILER = MKFAIL.EXE

.PHONY: demo

demo:
	@$(STAMPER) silent.log hidden
	-$(FAILER) 3
	$(STAMPER) done.log ok
