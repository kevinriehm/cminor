#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "reg.h"
#include "util.h"
#include "vector.h"

typedef struct {
	bool active;

	int index;

	int freenext;
} frame_slot_t;

typedef_vector_t(frame_slot_t);

typedef struct {
	bool active;

	enum {
		VREG_ARRAY,
		VREG_FUNCTION,
		VREG_GLOBAL,
		VREG_POINTER,
		VREG_REGISTER,
		VREG_SUBSCRIPT
	} type;

	bool isreal;
	reg_real_t real;

	bool persistent; // Survives reg_free()?
	int *lvalue; // Where the reference keeps its reg
	int slot; // Location in stack frame
	int offset; // Element within slot
	size_t size; // Size for arrays
	int subreg; // For pointers, the register with the actual pointer

	str_t name; // For globals and functions

	str_t refstr;

	// Free list traversal
	int freenext;

	// LRU list traversal
	int lrunext;
	int lruprev;
} vreg_t;

typedef_vector_t(vreg_t);
typedef_vector_t(vector_t(vreg_t));

static bool regreals[16];

static size_t framesize; // Number of words of local stack space in block
static size_t maxframesize; // Number of words of local stack space in function
static vector_t(size_t) framesizes; // Stack space at each block level

static int vregfree;

static int vreglru;
static int vreglrutail;
static bool vregtouchable;

static vector_t(vreg_t) vregs;
static vector_t(vector_t(vreg_t)) vreglvalues;

static int framefree;

static vector_t(frame_slot_t) frame;

static reg_real_t realhint = REG_NONE; // Hint for the next real to allocate

static char *vreg_name(vreg_t *);

// Helper function to get an empty stack frame slot
static frame_slot_t *frame_slot_alloc() {
	frame_slot_t *slot;

	if(framefree < 0) { // Need new slot
		framesize++;
		if(framesize > maxframesize)
			maxframesize = framesize;

		vector_append(frame,(frame_slot_t) {
			.index = framesize
		});

		slot = frame.v + frame.n - 1;
	} else { // Reuse existing slot
		slot = frame.v + framefree;
		framefree = slot->freenext;
	}

	slot->active = true;

	return slot;
}

// Helper function to get an empty vreg slot
static vreg_t *vreg_alloc() {
	vreg_t *vreg;

	if(vregfree < 0) { // Need new slot
		vector_append(vregs,(vreg_t) {
			.refstr = str_new("",0)
		});

		vreg = vregs.v + vregs.n - 1;
	} else { // Reuse existing slot
		vreg = vregs.v + vregfree;
		vregfree = vreg->freenext;
	}

	vreg->active = true;
	vreg->lvalue = NULL;

	// Append to end of LRU list
	if(vreglrutail < 0) {
		vreg->lrunext = -1;
		vreg->lruprev = -1;
		vreglru = vreglrutail = vreg - vregs.v;
	} else {
		vreg->lrunext = -1;
		vreg->lruprev = vreglrutail;
		vreglrutail = vregs.v[vreglrutail].lrunext = vreg - vregs.v;
	}

	return vreg;
}

static vreg_t vreg_copy(vreg_t vreg) {
	vreg.refstr = str_new("",0);
	return vreg;
}

// Return the virtual register currently residing in slot (if there is one)
static vreg_t *vreg_in_slot(int slot) {
	for(size_t i = 0; i < vregs.n; i++)
		if(vregs.v[i].active
			&& !vregs.v[i].isreal && vregs.v[i].slot == slot)
			return vregs.v + i;

	return NULL;
}

static bool vreg_is_real(vreg_t *vreg) {
	return vreg->isreal
		|| vreg->type == VREG_POINTER && vregs.v[vreg->subreg].isreal;
}

// Move the value in from to to (to may overlap with another virtual register)
static void vreg_move(vreg_t *from, vreg_t *to, FILE *f) {
	int reg;
	vreg_t *vreg;
	bool oldtouchable;

	oldtouchable = vregtouchable;
	vregtouchable = false;

	if(to->isreal) {
		if(from->isreal && from->real == to->real)
			return;

		reg_vacate_v(1,&to->real,f);
		fprintf(f,"\tmov %s, %s\n",vreg_name(from),vreg_name(to));
	} else {
		if(!from->isreal && from->slot == to->slot)
			return;

		vreg = vreg_in_slot(to->slot);
		reg = vreg || !from->isreal ? reg_alloc(f) : -1;

		if(vreg) {
			fprintf(f,"\tmov %s, %s\n",
				vreg_name(vreg),reg_name(reg));
			vreg->slot = frame_slot_alloc() - frame.v;
			fprintf(f,"\tmov %s, %s\n",
				reg_name(reg),vreg_name(vreg));
		}

		if(from->isreal)
			fprintf(f,"\tmov %s, %s\n",
				vreg_name(from),vreg_name(to));
		else {
			fprintf(f,"\tmov %s, %s\n",
				vreg_name(from),reg_name(reg));
			fprintf(f,
				"\tmov %s, %s\n",reg_name(reg),vreg_name(to));
		}

		reg_free(reg);
	}

	vregtouchable = oldtouchable;
}

// Helper function to spill the least-recently used actual register
static reg_real_t vreg_spill_lru(FILE *f) {
	int lru;
	vreg_t *vreg;
	frame_slot_t *slot;

	// Pick the virtual register to spill
	for(lru = vreglru;
		lru >= 0 && !reg_is_real(lru); lru = vregs.v[lru].lrunext);

	if(lru < 0) // Should never happen
		die("registers exhausted");

	// Spill the register
	slot = frame_slot_alloc();
	fprintf(f,"\tmov %s, %i(%%rbp)\n",reg_name(lru),-8*slot->index);

	vreg = vregs.v + lru;
	vreg->isreal = false;
	vreg->slot = slot - frame.v;

	return vreg->real;
}

// Helper function to move a virtual register to the end of the LRU list
static void vreg_touch(vreg_t *vreg) {
	ptrdiff_t index;

	index = vreg - vregs.v;

	if(!vregtouchable || vreglrutail == index
		|| index < 0 || index >= (ptrdiff_t) vregs.n)
		return;

	if(vreg->lruprev < 0) {
		if(vreg->lrunext >= 0)
			vreglru = vreg->lrunext;
	} else vregs.v[vreg->lruprev].lrunext = vreg->lrunext;

	if(vreg->lrunext >= 0)
		vregs.v[vreg->lrunext].lruprev = vreg->lruprev;

	if(vreglrutail >= 0)
		vregs.v[vreglrutail].lrunext = vreg - vregs.v;

	vreg->lruprev = vreglrutail;
	vreg->lrunext = -1;

	vreglrutail = vreg - vregs.v;
}

static char *vreg_name(vreg_t *vreg) {
	vreg_touch(vreg);

	switch(vreg->type) {
	case VREG_ARRAY:
		str_ensure_cap(&vreg->refstr,ceil(log10(INT_MAX)) + 6);
		sprintf(vreg->refstr.v,
			"%i(%%rbp",-8*frame.v[vreg->slot].index);
		break;

	case VREG_FUNCTION:
		str_ensure_cap(&vreg->refstr,vreg->name.n);
		sprintf(vreg->refstr.v,"%s",vreg->name.v);
		break;

	case VREG_GLOBAL:
		str_ensure_cap(&vreg->refstr,vreg->name.n + 6);
		sprintf(vreg->refstr.v,"%s(%%rip)",vreg->name.v);
		break;

	case VREG_POINTER:
		str_ensure_cap(&vreg->refstr,5);
		sprintf(vreg->refstr.v,"(%s",reg_name(vreg->subreg));
		break;

	case VREG_REGISTER:
		if(vreg->isreal) {
			str_ensure_cap(&vreg->refstr,5);
			sprintf(vreg->refstr.v,"%s",reg_name_real(vreg->real));
		} else {
			str_ensure_cap(
				&vreg->refstr,ceil(log10(INT_MAX)) + 7);
			sprintf(vreg->refstr.v,
				"%i(%%rbp)",-8*frame.v[vreg->slot].index);
		}
		break;

	case VREG_SUBSCRIPT:
		str_ensure_cap(&vreg->refstr,ceil(log10(INT_MAX)) + 7);
		sprintf(vreg->refstr.v,"%i(%%rbp)",
			-8*(frame.v[vreg->slot].index - vreg->offset));
		break;
	}

	return vreg->refstr.v;
}

// Helper function to select an unoccupied actual register
static reg_real_t reg_find_real() {
	static int precedence[] = {
		[REG_RAX] =  0,
		[REG_RBX] =  4,
		[REG_RCX] =  3,
		[REG_RDX] =  3,
		[REG_RSI] =  2,
		[REG_RDI] =  2,
		[REG_RBP] = -1,
		[REG_RSP] = -1,
		[REG_R8]  =  2,
		[REG_R9]  =  2,
		[REG_R10] =  1,
		[REG_R11] =  1,
		[REG_R12] =  4,
		[REG_R13] =  4,
		[REG_R14] =  4,
		[REG_R15] =  4,

		[REG_NONE] = -1
	};

	reg_real_t real = REG_NONE;

	if(realhint != REG_NONE && !regreals[realhint]) {
		real = realhint;
		regreals[real] = true;
		realhint = REG_NONE;
		return real;
	}

	for(int i = 0; i < 16; i++) {
		if(!regreals[i] && precedence[i] > precedence[real])
			real = i;
	}

	if(real != REG_NONE)
		regreals[real] = true;

	return real;
}

// Sets up the register state for the beginning of a function which needs
// framestart words of and locals
void reg_reset() {
	static bool first = true;

	if(first) {
		first = false;
		vector_init(vregs);
		vector_init(frame);
	}

	// Free all the virtual registers
	vregfree = -1;
	vreglru = -1;
	vreglrutail = -1;
	vregtouchable = true;

	vregs.n = 0;

	// Reset the stack
	framesize = 0;
	maxframesize = 0;
	vector_init(framesizes);

	framefree = -1;

	// Reset the real registers
	regreals[REG_RAX] = false; // Scratch
	regreals[REG_RBX] = false; // Scratch
	regreals[REG_RCX] = false; // Argument 4
	regreals[REG_RDX] = false; // Argument 3
	regreals[REG_RSI] = false; // Argument 2
	regreals[REG_RDI] = false; // Argument 1
	regreals[REG_RBP] = true;  // Base pointer
	regreals[REG_RSP] = true;  // Stack pointer
	regreals[REG_R8]  = false; // Argument 5
	regreals[REG_R9]  = false; // Argument 6
	regreals[REG_R10] = false; // Caller-saved
	regreals[REG_R11] = false; // Caller-saved
	regreals[REG_R12] = false; // Scratch
	regreals[REG_R13] = false; // Scratch
	regreals[REG_R14] = false; // Scratch
	regreals[REG_R15] = false; // Scratch
}

// Returns the total size of the stack, including both locals and spill space
size_t reg_frame_size() {
	return (maxframesize + 1)/2*2;
}

// Either finds a unused register or makes room by spilling an occupied one
int reg_alloc(FILE *f) {
	vreg_t *vreg = vreg_alloc();

	vreg->type = VREG_REGISTER;
	vreg->persistent = false;

	// Allocate any available register
	if(vreg->real = reg_find_real(), vreg->real != REG_NONE) {
		vreg->isreal = true;
		return vreg - vregs.v;
	}

	// No register available, so spill the one least-recently used
	vreg->real = vreg_spill_lru(f);
	vreg->isreal = true;

	return vreg - vregs.v;
}

// Create a pseudo-register referring to a newly stack-allocated array
int reg_assign_array(size_t size) {
	vreg_t *vreg = vreg_alloc();

	framesize += size;
	if(framesize > maxframesize)
		maxframesize = framesize;

	vector_append(frame,(frame_slot_t) {
		.active = true,
		.index = framesize
	});

	vreg->type = VREG_ARRAY;
	vreg->isreal = false;
	vreg->persistent = true;
	vreg->slot = frame.n - 1;
	vreg->size = size;
	vreg->offset = 0;

	return vreg - vregs.v;
}

// Create a pseudo-register referring to a function
int reg_assign_function(str_t name) {
	vreg_t *vreg = vreg_alloc();

	vreg->type = VREG_FUNCTION;
	vreg->isreal = false;
	vreg->persistent = false;
	vreg->name = name;

	return vreg - vregs.v;
}

// Create a pseudo-register referring to a global variable
int reg_assign_global(str_t name) {
	vreg_t *vreg = vreg_alloc();

	vreg->type = VREG_GLOBAL;
	vreg->isreal = false;
	vreg->persistent = true;
	vreg->name = name;

	return vreg - vregs.v;
}

// Create a virtual register initially referring to an already-allocated local
int reg_assign_local(int index) {
	vreg_t *vreg = vreg_alloc();

	vector_append(frame,(frame_slot_t) {
		.active = true,
		.index = index
	});

	vreg->type = VREG_REGISTER;
	vreg->isreal = false;
	vreg->persistent = true;
	vreg->slot = frame.n - 1;

	return vreg - vregs.v;
}

// Create a pseudo-register wrapping a virtual register containing a pointer
int reg_assign_pointer(int subreg) {
	vreg_t *vreg = vreg_alloc();

	vreg->type = VREG_POINTER;
	vreg->isreal = false;
	vreg->persistent = false;
	vreg->subreg = subreg;

	return vreg - vregs.v;
}

// Create a virtual register initially referring to the actual register real
int reg_assign_real(reg_real_t real) {
	vreg_t *vreg = vreg_alloc();

	regreals[real] = true;

	vreg->type = VREG_REGISTER;
	vreg->isreal = true;
	vreg->persistent = false;
	vreg->real = real;

	return vreg - vregs.v;
}

// Create a pseudo-register referring to a word within an array
int reg_assign_subscript(int base, int offset) {
	vreg_t *vreg = vreg_alloc();
	vreg_t *vbase = vregs.v + base;

	vreg_touch(vbase);

	vreg->type = VREG_SUBSCRIPT;
	vreg->isreal = false;
	vreg->persistent = false;
	vreg->slot = vbase->slot;
	vreg->offset = vbase->offset + offset;

	return vreg - vregs.v;
}

// Release the virtual register reg
void reg_free(int reg) {
	vreg_t *vreg = vregs.v + reg;

	if(reg < 0)
		return;

	// Arrays and locals exist through entire functions
	if(vreg->persistent)
		return;

	if(vreg->type == VREG_POINTER)
		reg_free(vreg->subreg);

	if(vreg->type == VREG_REGISTER) {
		if(vreg->isreal)
			regreals[vreg->real] = false;
		else {
			frame.v[vreg->slot].active = false;
			frame.v[vreg->slot].freenext = framefree;
			framefree = vreg->slot;
		}
	}

	if(vreg->lruprev < 0)
		vreglru = vreg->lrunext;
	else vregs.v[vreg->lruprev].lrunext = vreg->lrunext;

	if(vreg->lrunext < 0)
		vreglrutail = vreg->lruprev;
	else vregs.v[vreg->lrunext].lruprev = vreg->lruprev;

	vreg->active = false;
	vreg->freenext = vregfree;
	vregfree = vreg - vregs.v;
}

// Release even virtual registers storing locals
void reg_free_persistent(int reg) {
	if(reg < 0)
		return;

	vregs.v[reg].persistent = false;

	reg_free(reg);
}

void reg_block_enter() {
	vector_append(framesizes,framesize);
}

void reg_block_leave() {
	framesize = framesizes.v[--framesizes.n];
}

// Preserve the locations of all virtual registers with lvalues
void reg_record_lvalues() {
	vector_t(vreg_t) saved;

	vector_init(saved);

	for(size_t i = 0; i < vregs.n; i++)
		if(vregs.v[i].active && vregs.v[i].lvalue)
			vector_append(saved,vreg_copy(vregs.v[i]));

	vector_append(vreglvalues,saved);
}

// Restore all virtual registers with lvalues to their old locations
void reg_restore_lvalues(FILE *f) {
	vector_t(vreg_t) saved;

	saved = vreglvalues.v[--vreglvalues.n];

	for(size_t si = 0; si < saved.n; si++) {
		for(size_t vi = 0; vi < vregs.n; vi++) {
			if(vregs.v[vi].active
				&& saved.v[si].lvalue == vregs.v[vi].lvalue) {
				vreg_move(vregs.v + vi,saved.v + si,f);

				if(vregs.v[vi].isreal)
					regreals[vregs.v[vi].real] = false;

				vregs.v[vi].isreal = saved.v[si].isreal;
				vregs.v[vi].real = saved.v[si].real;
				vregs.v[vi].slot = saved.v[si].slot;

				if(vregs.v[vi].isreal)
					regreals[vregs.v[vi].real] = true;

				break;
			}
		}
	}

	vector_free(saved);
}

// Returns whether the virtual register currently resides in an actual register
bool reg_is_real(int reg) {
	return vreg_is_real(vregs.v + reg);
}

// Returns whether the virtual register holds a pointer value
bool reg_is_pointer(int reg) {
	return vregs.v[reg].type == VREG_POINTER;
}

// Returns whether the virtual register represents a persistent (named) value
bool reg_is_persistent(int reg) {
	return vregs.v[reg].persistent;
}

// Returns where reg is stored
int *reg_get_lvalue(int reg) {
	return vregs.v[reg].lvalue;
}

// Sets where reg is stored
void reg_set_lvalue(int reg, int *lvalue) {
	vregs.v[reg].lvalue = lvalue;
}

// Moves the virtual register to an actual register, if it is not there already
void reg_make_real(int reg, FILE *f) {
	reg_real_t real;

	vreg_t *vreg = vregs.v + reg;

	if(vreg->type == VREG_POINTER)
		vreg = vregs.v + vreg->subreg;

	if(vreg_is_real(vreg) || vreg->type != VREG_REGISTER)
		return;

	real = vreg_spill_lru(f);

	fprintf(f,"\tmov %s, %s\n",vreg_name(vreg),reg_name_real(real));

	vreg->isreal = true;
	vreg->real = real;
}

// Convenience function to ensure at least one of reg1 and reg2 is real
void reg_make_one_real(int reg1, int reg2, FILE *f) {
	if(!reg_is_real(reg1) && !reg_is_real(reg2))
		reg_make_real(reg1,f);
}

// Convenience function to ensure reg is non-persistent
void reg_make_temporary(int *reg, FILE *f) {
	int newreg;

	vreg_t *vreg = vregs.v + *reg;

	vreg_touch(vreg);

	if(vreg->persistent) {
		newreg = reg_alloc(f);
		fprintf(f,"\tmov %s, %s\n",reg_name(*reg),reg_name(newreg));
		*reg = newreg;
	}
}

// Convenience function to ensure reg1 is non-persistent
// and at least one of reg1 and reg2 is real
void reg_make_one_temporary(int *reg1, int *reg2, FILE *f, bool *swapped) {
	int reg;
	vreg_t *vreg;

	vreg_t *vreg1 = vregs.v + *reg1;
	vreg_t *vreg2 = vregs.v + *reg2;

	vreg_touch(vreg1);
	vreg_touch(vreg2);

	// Swap them if reg2 is closer to what we want
	if(vreg1->persistent && !vreg2->persistent
		|| !vreg1->persistent && !vreg2->persistent && !vreg1->isreal) {
		reg = *reg1;
		*reg1 = *reg2;
		*reg2 = reg;

		vreg = vreg1;
		vreg1 = vreg2;
		vreg2 = vreg;

		if(swapped)
			*swapped = true;
	} else if(swapped)
		*swapped = false;

	// Make sure reg1 is temporary
	if(vreg1->persistent) {
		reg = reg_alloc(f);
		fprintf(f,"\tmov %s, %s\n",reg_name(*reg1),reg_name(reg));
		*reg1 = reg;
		vreg1 = vregs.v + *reg1;
	}

	// Make sure reg1 is real
	if(!vreg1->isreal)
		reg_make_real(*reg1,f);
}

// Promotes a standard virtual register to a local variable
void reg_make_persistent(int reg) {
	vreg_t *vreg = vregs.v + reg;

	vreg_touch(vreg);

	vreg->persistent = true;
}

// Try to allocate real for the next virtual register
void reg_hint(reg_real_t real) {
	realhint = real;
}

// Move the n virtual registers in regs to the actual registers in reals
// (or, if a reg is -1, simply vacate the corresponding real)
void reg_map_v(int n, int *regs, reg_real_t *reals, FILE *f) {
	vreg_t *vreg;
	reg_real_t real;
	vector_t(reg_real_t) emptyreals;

	// First, move the regs already in registers
	for(int i = 0; i < n; i++) {
		vreg = vregs.v + regs[i];

		if(regs[i] < 0 || !vreg->isreal || vreg->real == reals[i])
			continue;

		if(regreals[reals[i]])
			fprintf(f,"\txchg %s, %s\n",
				reg_name(regs[i]),reg_name_real(reals[i]));
		else fprintf(f,"\tmov %s, %s\n",
			reg_name(regs[i]),reg_name_real(reals[i]));

		for(size_t vi = 0; vi < vregs.n; vi++)
			if(vregs.v[vi].active && vregs.v[vi].isreal
				&& vregs.v[vi].real == reals[i])
				vregs.v[vi].real = vreg->real;

		vreg->real = reals[i];
	}

	// We now block off the reals to prevent accidentally filling them
	for(int i = 0; i < n; i++)
		regreals[i] = true;

	// Next, move regs in from the stack
	for(int i = 0; i < n; i++) {
		vreg = vregs.v + regs[i];

		if(regs[i] < 0 || vreg->isreal)
			continue;

		for(size_t vi = 0; vi < vregs.n; vi++) {
			if(vregs.v[vi].active && vregs.v[vi].isreal
				&& vregs.v[vi].real == reals[i]) {
				if(real = reg_find_real(), real == REG_NONE) {
					fprintf(f,"\txchg %s, %s\n",
						reg_name(regs[i]),
						reg_name(vi));

					vregs.v[vi].isreal = false;
					vregs.v[vi].slot = vreg->slot;

					vreg->isreal = true;
					vreg->real = reals[i];
				} else {
					fprintf(f,"\tmov %s, %s\n",
						reg_name(vi),
						reg_name_real(real));
					fprintf(f,"\tmov %s, %s\n",
						reg_name(regs[i]),
						reg_name_real(reals[i]));

					vregs.v[vi].real = real;

					vreg->isreal = true;
					vreg->real = reals[i];
				}

				goto next_stack_reg;
			}
		}

		fprintf(f,"\tmov %s, %s\n",
			reg_name(regs[i]),reg_name_real(reals[i]));

	next_stack_reg:
		continue;
	}

	// Finally, vacate the remaining reals
	vector_init(emptyreals);

	for(int i = 0; i < n; i++)
		if(regs[i] < 0)
			vector_append(emptyreals,reals[i]);

	reg_vacate_v(emptyreals.n,emptyreals.v,f);

	vector_free(emptyreals);
}

// If any of the n actual registers in reals currently hold virtual registers,
// move those virtual registers somewhere else (other registers or stack)
void reg_vacate_v(int n, reg_real_t *reals, FILE *f) {
	vreg_t *vreg;
	reg_real_t real;
	bool oldtouchable;
	frame_slot_t *slot;

	// Block these off for the duration of the process
	for(int i = 0; i < n; i++)
		regreals[reals[i]] = true;

	// Vacate the registers
	oldtouchable = vregtouchable;
	vregtouchable = false;
	for(int mru = vreglrutail; mru >= 0; mru = vregs.v[mru].lruprev) {
		for(int i = 0; vregs.v[mru].isreal && i < n; i++) {
			vreg = vregs.v + mru;

			if(!vreg->active || vreg->real != reals[i])
				continue;

			// Vacate to another register, if possible
			if(real = reg_find_real(), real != REG_NONE) {
				fprintf(f,"\tmov %s, %s\n",
					reg_name(mru),reg_name_real(real));

				vreg->real = real;
			} else { // Otherwise, spill to the stack
				slot = frame_slot_alloc();
				fprintf(f,"\tmov %s, %i(%%rbp)\n",
					reg_name(mru),-8*slot->index);

				vreg->isreal = false;
				vreg->slot = slot - frame.v;
			}

			break;
		}
	}
	vregtouchable = oldtouchable;

	// The registers are now free for use
	for(int i = 0; i < n; i++)
		regreals[reals[i]] = false;
}

char *reg_name(int reg) {
	if(reg < 0)
		return "<should never happen>";

	return vreg_name(vregs.v + reg);
}

char *reg_name_8l(int reg) {
	static char *realnames[] = {
		[REG_RAX] = "%al",
		[REG_RBX] = "%bl",
		[REG_RCX] = "%cl",
		[REG_RDX] = "%dl",
		[REG_RSI] = "%sil",
		[REG_RDI] = "%dil",
		[REG_RBP] = "%bpl",
		[REG_RSP] = "%spl",
		[REG_R8]  = "%r8b",
		[REG_R9]  = "%r9b",
		[REG_R10] = "%r10b",
		[REG_R11] = "%r11b",
		[REG_R12] = "%r12b",
		[REG_R13] = "%r13b",
		[REG_R14] = "%r14b",
		[REG_R15] = "%r15b"
	};

	vreg_t *vreg = vregs.v + reg;

	vreg_touch(vreg);

	if(vreg->type == VREG_REGISTER && vreg->isreal)
		return realnames[vreg->real];
	else return reg_name(reg);
}

char *reg_name_real(reg_real_t real) {
	static char *realnames[] = {
		[REG_RAX] = "%rax",
		[REG_RBX] = "%rbx",
		[REG_RCX] = "%rcx",
		[REG_RDX] = "%rdx",
		[REG_RSI] = "%rsi",
		[REG_RDI] = "%rdi",
		[REG_RBP] = "%rbp",
		[REG_RSP] = "%rsp",
		[REG_R8]  = "%r8",
		[REG_R9]  = "%r9",
		[REG_R10] = "%r10",
		[REG_R11] = "%r11",
		[REG_R12] = "%r12",
		[REG_R13] = "%r13",
		[REG_R14] = "%r14",
		[REG_R15] = "%r15"
	};

	return realnames[real];
}

