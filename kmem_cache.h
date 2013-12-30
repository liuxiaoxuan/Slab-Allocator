#ifndef KMEM_CACHE_H
#define KMEM_CACHE_H

#include <stdlib.h>

#define PAGE_SZ 4096
#define BUF_SZ 512         //boundary size of small obj and big obj

#define WORD 4
#define KM_SLEEP 0
#define KM_NOSLEEP 1

typedef void (*kc_fn_t)(void *, size_t);

struct kmem_bufctl {
	struct kmem_bufctl *next;
	struct kmem_bufctl *prev;
	void *buffer;
	void *sp;
};

struct kmem_slab {
	void *slab;

	struct kmem_slab *next;
	struct kmem_slab *prev;
	
	struct kmem_bufctl *bp_head;
	struct kmem_bufctl *bp_tail;
	
	void *fl_head;
	void *fl_tail;
	
	int refcnt;
	void *bufstart;
};

struct kmem_cache {
	char *name;

	int slab_sz;
	int buf_sz;
	int buf_amt;
	int align;

	kc_fn_t constructor;
	kc_fn_t destructor;

	struct kmem_slab *sp_full;
	struct kmem_slab *sp_part;
	struct kmem_slab *sp_empt;
};

struct kmem_cache *
kmem_cache_create(char *name, size_t size, int align, kc_fn_t constructor, kc_fn_t destructor);

void *
kmem_cache_alloc(struct kmem_cache *cp, int flags);

void 
kmem_cache_free(struct kmem_cache *cp, void *buf);

void 
kmem_cache_destroy(struct kmem_cache *cp);


void 
__print_slab(struct kmem_cache *cp);


#endif
