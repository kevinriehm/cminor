#include <stdbool.h>

#include "reg.h"
#include "util.h"

static bool regs[16] = {
	true,  // rax: result
	false, // rbx: scratch
	true,  // rcx: argument 4
	true,  // rdx: argument 3
	true,  // rsi: argument 2
	true,  // rdi: argument 1
	true,  // rbp: base pointer
	true,  // rsp: stack pointer
	true,  // r8 : argument 5
	true,  // r9 : argument 6
	false, // r10: scratch
	false, // r11: scratch
	false, // r12: scratch
	false, // r13: scratch
	false, // r14: scratch
	false  // r15: scratch
};

int reg_alloc() {
	for(int i = 0; i < 16; i++) {
		if(!regs[i]) {
			regs[i] = true;
			return i;
		}
	}

	die("registers exhausted");
	return -1;
}

void reg_free(int reg) {
	regs[reg] = false;
}

char *reg_name(int reg) {
	static char *names[] = {
		"%rax", "%rbx", "%rcx", "%rdx",
		"%rsi", "%rdi", "%rbp", "%rsp",
		 "%r8",  "%r9", "%r10", "%r11",
		"%r12", "%r13", "%r14", "%r15"
	};

	return names[reg];
}

char *reg_name_8l(int reg) {
	static char *names[] = {
		  "%al",   "%bl",   "%cl",   "%dl",
		 "%sil",  "%dil",  "%bpl",  "%spl",
		 "%r8b",  "%r9b", "%r10b", "%r11b",
		"%r12b", "%r13b", "%r14b", "%r15b"
	};

	return names[reg];
}

char *reg_name_arg(int index) {
	static char *names[] = {
		"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"
	};

	return names[index];
}

