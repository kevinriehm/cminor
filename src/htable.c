#include <stdint.h>

#include "htable.h"

// Implements the 64-bit FNV-1a algorithm
size_t htable_hash(size_t nbins, str_t key) {
	uint64_t h;

	h = 0xcbf29ce484222325ull;
	for(size_t i = 0; i < key.n; i++)
		h = 0x100000001b3*(h^key.v[i]);

	return h%nbins;
}

// Resizes bins to newnbins by rehashing all the keys currently in bins
void htable_resize(size_t *nbins, htable_bin_header_t ***bins,
	size_t newnbins) {
	size_t newbini;
	htable_bin_header_t **newbins;
	htable_bin_header_t *entry, *nextentry;

	newbins = calloc(newnbins,sizeof *newbins);

	for(size_t bini = 0; bini < *nbins; bini++) {
		if(entry = (*bins)[bini], !entry)
			continue;

		do {
			nextentry = entry->next;

			newbini = htable_hash(newnbins,entry->key);

			// Brand new bin?
			if(!newbins[newbini]) {
				entry->next = entry;
				newbins[newbini] = entry;
			} else { // Add to front of existing bin
				entry->next = newbins[newbini]->next;
				newbins[newbini]->next = entry;
				newbins[newbini] = entry;
			}

			entry = nextentry;
		} while(entry != (*bins)[bini]);
	}

	*nbins = newnbins;
	*bins = newbins;
}

// Locates key in bins; if it is there, rotates that bin to put key's
// struct at the front; if it is not there, and insert is true, inserts it at
// the front of the bin
bool htable_locate(size_t nbins, htable_bin_header_t **bins, str_t key,
	bool insert, size_t entrysize) {
	size_t bini;
	htable_bin_header_t *entry;

	bini = htable_hash(nbins,key);

	entry = bins[bini];

	// Search the bin (a cirular linked list)
	do {
		if(!entry)
			break;

		if(str_eq(key,entry->key)) {
			bins[bini] = entry;
			return true;
		}

		entry = entry->next;
	} while(entry != bins[bini]);

	// It's not there; add it if requested
	if(insert) {
		entry = calloc(1,entrysize);
		entry->key = key;

		// Brand new bin?
		if(!bins[bini]) {
			entry->next = entry;
			bins[bini] = entry;
		} else { // Add to front of existing bin
			entry->next = bins[bini]->next;
			bins[bini]->next = entry;
			bins[bini] = entry;
		}
	}

	return false;
}

