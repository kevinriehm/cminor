CM_CSRC = cminor.c util.c
CM_LSRC = tokenize.l
CM_YSRC = parse.y

CM_CFLAGS = -g -Wall -Wextra -Wpedantic -Wno-parentheses -std=c99 \
	-D_POSIX_C_SOURCE=200112L -I. -Isrc $(CFLAGS)

CM_DEPS = $(CM_CSRC:.c=.d) $(CM_LSRC:.l=.yy.d) $(CM_YSRC:.y=.tab.d)
CM_OBJS = $(CM_CSRC:.c=.o) $(CM_LSRC:.l=.yy.o) $(CM_YSRC:.y=.tab.o)

CBUILD = $(CC) $(CM_CFLAGS) -MMD -MF dep/$*.d -c -o $@ $<

.PHONY: clean
.PRECIOUS: %/ gen/%.yy.c

all: cminor

cminor: $(addprefix obj/,$(CM_OBJS)) | obj/
	$(CC) $(CM_CFLAGS) -o $@ $^

%/:
	mkdir -p $*

dep/%.yy.d: gen/%.yy.c | dep/
	$(CC) $(CM_CFLAGS) -MM -MG -MT obj/$*.yy.o -MF $@ $<

gen/%.tab.c gen/%.tab.h: src/%.y | gen/
	$(YACC) -d -b gen/$* $<

gen/%.yy.c: src/%.l | gen/
	$(LEX) -t $< > $@

obj/%.o: gen/%.c | dep/ gen/
	$(CBUILD)

obj/%.o: src/%.c | dep/ obj/
	$(CBUILD)

-include $(addprefix dep/,$(CM_DEPS))

clean:
	rm -rf cminor
	rm -rf dep
	rm -rf gen
	rm -rf obj

