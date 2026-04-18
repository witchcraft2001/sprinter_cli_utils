STAMPER = mkstamp.exe
FAILER = mkfail.exe

.PHONY: demo

demo:
	@$(STAMPER) silent.log hidden
	-$(FAILER) 3
	$(STAMPER) done.log ok
