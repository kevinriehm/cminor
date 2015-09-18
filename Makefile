CM_CSRC = cminor.c util.c
CM_LSRC = scan.l

CM_CFLAGS = -g -Wall -Wextra -Wno-parentheses -std=c99 \
	-D_POSIX_C_SOURCE=200809L -Isrc $(CFLAGS)

CM_DEPS = $(CM_CSRC:.c=.d) $(CM_LSRC:.l=.yy.d)
CM_OBJS = $(CM_CSRC:.c=.o) $(CM_LSRC:.l=.yy.o)

CBUILD = $(CC) $(CM_CFLAGS) -MMD -MF dep/$*.d -c -o $@ $<

LEX = flex

.PHONY: clean
.PRECIOUS: %/ gen/%.yy.c

all: cminor

cminor: $(addprefix obj/,$(CM_OBJS)) | obj/
	$(CC) $(CM_CFLAGS) -o $@ $^

dep/:
	mkdir -p $@

gen/:
	mkdir -p $@

obj/:
	mkdir -p $@

dep/%.yy.d: gen/%.yy.c | dep/
	$(CC) $(CM_CFLAGS) -MM -MG -MT obj/$*.yy.o -MF $@ $<

gen/%.yy.c: src/%.l | gen/
	$(LEX) -t $< > $@

obj/%.o: gen/%.c | dep/ obj/
	$(CBUILD)

obj/%.o: src/%.c | dep/ obj/
	$(CBUILD)

-include $(addprefix dep/,$(CM_DEPS))

clean:
	rm -rf cminor
	rm -rf dep
	rm -rf gen
	rm -rf obj

