#include <stdio.h>
#include <malloc.h>

#include<kmem_cache.h>

size_t void_sz = sizeof(void *);
size_t kmem_bufctl_sz = sizeof(kmem_bufctl);
size_t kmem_slab_sz = sizeof(kmem_slab);
size_t kmem_cache_sz = sizeof(kmem_cache);

int
__align_offset(size_t size, int align)
{
	if(!align)
		return size;
	else 
		return align - size % align;
}

void
__init_slab(void *ptr, void *slab, int buf_sz, int buf_amt)
{	
	struct kmem_slab *sp = ptr;

	sp -> slab          = slab;
	sp -> refcnt        = 0;
	sp -> next          = NULL;
	sp -> prev          = NULL;
	sp -> fl_head       = slab_idx;
	sp -> fl_tail       = slab_idx + buf_sz * (buf_amt - 1);
}

void 
__init_buf(void *slab, int buf_sz, int buf_amt)
{
	int i;
	
	void *curr, *next;
		
	for(i = 0; i < buf_amt; i++){
		curr   = slab + buf_sz * i ;	
		next   = curr + buf_sz;
		*curr = next;
	}
	
	*curr = slab;	
}

void *
__create_slab(struct kmem_cache *cp)
{
	void *slab = memalign(SLAB_SIZE, SLAB_SIZE);

	void *sp_new = slab + cp -> buf_amt * cp -> buf_sz;

	__init_slab(sp, slab, buf_sz, buf_amt);
	__init_buf(slab, buf_sz, buf_amt);
	
	cp -> sp -> next = sp_new;
	sp_new -> prev = cp -> sp;	
	cp -> sp = sp_new;
	
	return sp_new;
}

void
__sort_slab(struct kmem_slab *sp)
{
	if(!sp)
		return;

	struct kmem_slab *temp = sp -> prev;
	
	if( temp && temp -> refcnt < sp -> refcnt){
		sp -> prev -> next = sp -> next;
		sp -> next -> prev = sp -> prev;
			
		temp -> prev -> next = sp;
		sp -> prev = temp -> prev;
			
		sp -> next = temp;
		temp -> prev = sp;		
	}
}

void
__rem_buf(struct kmem_slab *sp)
{	
	sp -> fl_tail = *(sp -> fl_tail);
}

void
__add_buf(struct kmem_slab *sp, void *buf)
{	
	*(sp -> fl_tail) = buf;
	sp -> fl_tail = buf;
}

struct kmem_cache *
kmem_cache_create(char *name, size_t size, int align, kc_fn_t constructor, kc_fn_t destructor)
{
	kmem_cache *cp = memalign(SLAB_SIZE, SLAB_SIZE);
	
	cp -> name          = name;
	cp -> buf_sz        = size + __align_offset(size, align);
	cp -> buf_amt       = (SLAB_SIZE - kmem_slab_sz) / buf_sz;
	cp -> align         = align;
	cp -> constructor   = constructor;
	cp -> destructor    = destructor;	
	cp -> sp            = __create_slab(cp);
	
	return cp;
}

void *
kmem_cache_alloc(struct kmem_cache *cp, int flags)
{
	struct kmem_slab *sp = cp -> sp;

	if( sp -> buf_cnt == sp -> buf_amt && !sp -> next){	
		sp = __create_slab(cp);
	}
	
	void *object = sp -> fl_head -> object;
	sp -> refcnt++;
	__rem_buf(sp);
	__sort_slab(sp);
	
	return sp -> fl_head -> object;
}

void 
kmem_cache_free(struct kmem_cache *cp, void *buf)
{
	int buf_sz  = cp -> buf_sz;
	int buf_amt = cp -> buf_amt;

	struct kmem_slab *sp = buf + buf_sz *(buf_amt - (buf % SLAB_SIZE) / buf_sz);
	
	sp -> refcnt--;
	__add_buf(sp, buf);
	__sort_slab(sp -> next);
}

void 
kmem_cache_destroy(struct kmem_cache *cp)
{
	struct kmem_slab *sp = cp -> sp;
	
	while(cp -> next){
		cp = cp -> next;
		free( cp -> prev -> slab_idx);
	}
	
	free(cp -> slab_idx);
}