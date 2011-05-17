/*
 * hashmap.c
 * Copyright (c) 2009 Vedant Kumar <vminch@gmail.com>
 * 		- with thanks to nvteighen and tinivole of ubuntuforums.org
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "hashmap.h"

// hashmaps need a hash function, an equality function, and a destructor
VOID mk_hmap(hashmap* hmap, uint32_t (*hash_fn)(key),
				BOOLEAN (*eq_fn)(key, key)
			#ifdef HMAP_DESTRUCTORS
				, VOID (*del_fn)(val)
			#endif
				) {
					
    
	hmap->map = (key_val_pair*) ExAllocatePool(PagedPool, HMAP_PAGE_SIZE);
    RtlZeroMemory(hmap->map, HMAP_PAGE_SIZE);
	hmap->size = 0;
	hmap->capacity = HMAP_PRESET_SIZE;
	hmap->hash_fn = hash_fn;
	hmap->eq_fn = eq_fn;
#ifdef HMAP_DESTRUCTORS
	hmap->del_fn = del_fn;
#endif
#ifdef HMAP_THREAD_SAFE
	sem_init(&hmap->lock, 0, 1);
#endif
}

VOID free_hmap(hashmap* hmap) {
#ifdef HMAP_THREAD_SAFE
	sem_wait(&hmap->lock);
#endif

#ifdef HMAP_DESTRUCTORS
	static uint32_t it;
	for (it=0; it < hmap->size; ++it) {
		if (hmap->map[it].v != NULL) {
			hmap->del_fn(hmap->map[it].v);
		}
	}
#endif

    ExFreePool(hmap->map);
	
#ifdef HMAP_THREAD_SAFE
	sem_post(&hmap->lock);
#endif
}

// open addressed hashmap insertion function
static VOID __oa_hmap_add(key_val_pair* map, uint32_t size, 
                          uint32_t (*hash_fn)(key),
                          key in, val out) {
	static uint32_t hash;
	hash = hash_fn(in) % size;
	
	while (map[hash].v != NULL) {
		hash = (hash + 1) % size;
	}
	
	map[hash].k = in;
	map[hash].v = out;
}

BOOLEAN __hmap_add(hashmap* hmap, key in, val out) {
#ifdef HMAP_THREAD_SAFE
	sem_wait(&hmap->lock);
#endif

    // assume (at least for now) that the initial page is large
    // enough that the hash map will not need to increase in size
    /*
	// performace degrades after a certain load
	if (((float) hmap->size) / hmap->capacity > 0.70) {
		key_val_pair* temp = (key_val_pair*) malloc(hmap->capacity * HMAP_GROWTH_RATE);
		if (temp != NULL) {
			hmap->capacity *= HMAP_GROWTH_RATE;
		} else {
		#ifdef HMAP_THREAD_SAFE
			sem_post(&hmap->lock);
		#endif
			// we're out of memory
			return FALSE;
		}
		
		// re-posn all elements
		static uint32_t it;
		for (it=0; it < hmap->capacity; ++it) {
			if (hmap->map[it].v != NULL) {
				__oa_hmap_add(temp, hmap->capacity, hmap->hash_fn, in, out);
			}
		}
		
		// swap out the old map with the new one
		free(hmap->map);
		hmap->map = temp;
	}
	*/

	__oa_hmap_add(hmap->map, hmap->capacity, hmap->hash_fn, in, out);
	hmap->size += 1;

#ifdef HMAP_THREAD_SAFE
	sem_post(&hmap->lock);
#endif

	return TRUE;
}

val __hmap_get(hashmap* hmap, key in) {
#ifdef HMAP_THREAD_SAFE
	sem_wait(&hmap->lock);
#endif

	static uint32_t hash;
	hash = hmap->hash_fn(in) % hmap->capacity;
	
	while (hmap->map[hash].v != NULL) {
		if (hmap->eq_fn(in, hmap->map[hash].k)) {
		#ifdef HMAP_THREAD_SAFE
			sem_post(&hmap->lock);
		#endif			
			
			return hmap->map[hash].v;
		}
		
		hash = (hash + 1) % hmap->capacity;
	}

	
#ifdef HMAP_THREAD_SAFE
	sem_post(&hmap->lock);
#endif
	
	return NULL;
}

// Dan Bernstein's string hash function (djb2)
uint32_t str_hash_fn(key in) {
	static uint32_t hash;
	static WCHAR c;
	
	hash = 5381;

	while (c != '\0') {
		c = *(WCHAR*) in++;
		hash = ((hash << 5) + hash) + c;
	}

	return hash;
}

BOOLEAN str_eq_fn(key a, key b) {
	return (wcscmp((WCHAR*) a, (WCHAR*) b) == 0) ? TRUE : FALSE;
}

VOID module_struct_delete(val VoidModuleStruct) {
    PMODULE_STRUCT ModuleStruct = (PMODULE_STRUCT) VoidModuleStruct;
    PIRP_LIST CurrentEntry, NextEntry;

    ExFreePool(ModuleStruct->ModuleName);
    ExFreePool(&ModuleStruct->ModuleLock);

    //
    //  Free the module and userspace IRP lists, completing each one
    //  (what else is there to do with them?)
    //

    CurrentEntry = ModuleStruct->ModuleIrpList;
    while(CurrentEntry) {
        NextEntry = CurrentEntry->Next;
        IoCompleteRequest(CurrentEntry->Irp, IO_NO_INCREMENT);
        ExFreePool(CurrentEntry);
        CurrentEntry = NextEntry;
    }

    CurrentEntry = ModuleStruct->UserspaceIrpList;
    while(CurrentEntry) {
        NextEntry = CurrentEntry->Next;
        IoCompleteRequest(CurrentEntry->Irp, IO_NO_INCREMENT);
        ExFreePool(CurrentEntry);
        CurrentEntry = NextEntry;
    }

    CurrentEntry = ModuleStruct->OutstandingIrpList;
    while(CurrentEntry) {
        NextEntry = CurrentEntry->Next;
        IoCompleteRequest(CurrentEntry->Irp, IO_NO_INCREMENT);
        ExFreePool(CurrentEntry);
        CurrentEntry = NextEntry;
    }

    ExFreePool(ModuleStruct);
}

