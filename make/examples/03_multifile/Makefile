STAMPER = MKSTAMP.EXE

.PHONY: all clean touch_a touch_b rebuild_sources

all: app.bin report.txt

a.src:
	$(STAMPER) a.src source_a

b.src:
	$(STAMPER) b.src source_b

c.src:
	$(STAMPER) c.src source_c

a.obj: a.src
	$(STAMPER) a.obj object_a

b.obj: b.src
	$(STAMPER) b.obj object_b

c.obj: c.src
	$(STAMPER) c.obj object_c

app.bin: a.obj b.obj c.obj
	$(STAMPER) app.bin linked_app

report.txt: app.bin a.obj b.obj c.obj
	$(STAMPER) report.txt build_report

touch_a:
	$(STAMPER) a.src source_a_updated

touch_b:
	$(STAMPER) b.src source_b_updated

rebuild_sources:
	$(STAMPER) a.src source_a_rebuild
	$(STAMPER) b.src source_b_rebuild
	$(STAMPER) c.src source_c_rebuild

clean:
	$(STAMPER) clean.log cleaned
