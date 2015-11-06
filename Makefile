CM_CSRC = cminor.c arg.c decl.c expr.c stmt.c type.c util.c
CM_LSRC = scan.l
CM_YSRC = parse.y

CM_CFLAGS = -g -Wall -Wextra -Wno-missing-field-initializers -Wno-parentheses \
	-std=c99 -D_POSIX_C_SOURCE=200809L -I. -Isrc $(CFLAGS)

CM_DEPS = $(CM_CSRC:.c=.d) $(CM_LSRC:.l=.yy.d) $(CM_YSRC:.y=.tab.d)
CM_OBJS = $(CM_CSRC:.c=.o) $(CM_LSRC:.l=.yy.o) $(CM_YSRC:.y=.tab.o)

CBUILD = $(CC) $(CM_CFLAGS) -MMD -MF dep/$*.d -c -o $@ $<
DBUILD = $(CC) $(CM_CFLAGS) -MM -MG -MT obj/$*.o -MF $@ $<

LEX = flex
YACC = bison

.PHONY: clean test
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

dep/%.d: gen/%.c | dep/
	$(DBUILD)

dep/%.d: src/%.c | dep/
	$(DBUILD)

gen/%.tab.c gen/%.tab.h: src/%.y | gen/
	cp $< $(@D)/
	cd $(@D)/; $(YACC) -db $* $(<F)

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

test: cminor
	@test/run_tests.sh

