#include <stdio.h>

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
__init_cache(void *ptr, char *name, int buf_sz, int align, kc_fn_t constructor, kc_fn_t destructor, void *sp)
{
	struct kmem_cache *cp = ptr;
	
	cp -> name          = name;
	cp -> buf_sz         = buf_sz;
	cp -> align         = align;
	cp -> constructor   = constructor;
	cp -> destructor    = destructor;
	cp -> sp            = sp;
}

void
__init_slab(void *ptr, void *freelist)
{	
	struct kmem_slab *sp = ptr;

	sp -> refcnt        = 0;
	sp -> next          = NULL;
	sp -> prev          = NULL;
	sp -> freelist      = freelist;
}

void 
__init_buf(void *slab, int buf_sz, int buf_amt)
{
	int i;
	
	void *next, *prev;
		
	for(i = 1; i <= buf_amt; i++){
		next   = slab + buf_sz * i - void_sz * 2;
		prev   = next + void_sz;	

		*next  = slab + buf_sz * i;
		*prev  = slab + buf_sz * (i - 1);
	}
	
	*next = slab;
	prev = slab + buf_sz - void_sz;
	*prev = slab + buf_sz * (buf_amt - 1);
	
}

void *
__create_slab(int buf_sz)
{
	void *slab = malloc(SLAB_SIZE);
	
	int buf_amt = (SLAB_SIZE - kmem_slab_sz) / bufsz;
	
	void *sp = slab + buf_amt * bufsz;

	__init_slab(sp, bp);
	__init_buf(slab, buf_sz, buf_amt);
	
	return sp;
}

void
__inc_slab(struct kmem_slab *sp)
{
	struct kmem_slab *temp = sp;
	
	while(temp -> prev){
		if(temp -> refcnt < sp -> refcnt){
			sp -> prev -> next = sp -> next;
			sp -> next -> prev = sp -> prev;
			
			temp -> prev -> next = sp;
			sp -> prev = temp -> prev;
			
			sp -> next = temp;
			temp -> prev = sp;
			
			return;
		}
	}

}

void
__dec_slab(struct kmem_slab *sp)
{
}

void
__rem_buf(struct kmem_slab *sp, void *buf, int buf_sz)
{
	void *curr_next, *curr_prev, *next_next, *next_prev, *prev_next, *prev_prev;
	
	curr_next = buf + buf_sz - void_sz * 2;
	curr_prev = next + void_sz;
	
	next_next = *curr_next + buf_sz - void_sz * 2;
	next_prev = next_next + void_sz;
	
	prev_next = *curr_prev + buf_sz - void_sz * 2;
	prev_prev = prev_next + buf_sz;
	
	*next_prev = *curr_prev;
	*prev_next = *curr_next;
	
	sp -> freelist = *curr_next;	
}

void
__add_buf(struct kmem_slab *sp, void *buf, int buf_sz)
{

}


struct kmem_cache *
kmem_cache_create(char *name, size_t size, int align, kc_fn_t constructor, kc_fn_t destructor)
{
	void *slab = malloc(SLAB_SIZE);
	
	int buf_sz = size + __align_offset(size, align) + void_sz * 2 + WORD;
	
	int buf_amt = (SLAB_SIZE - kmem_cache_sz - kmem_slab_sz) / buf_sz;
	
	void *cp = slab + buf_amt * buf_sz;
	void *sp = cp + kmem_cache_sz;
	
	__init_cache(cp, name, buf_sz, align, constructor, destructor, sp);
	__init_slab(sp, slab);
	__init_buf(slab, buf_sz, buf_amt);

	return cp;
}

void *
kmem_cache_alloc(struct kmem_cache *cp, int flags)
{
	struct kmem_slab *sp = cp -> sp;

	if( sp -> buf_cnt == sp -> buf_amt){
		if (!sp -> next)
			struct kmem_slab *sp_new = __create_slab(cp -> buf_sz);
			sp -> next = sp_new;
			sp_new -> prev = sp;
			sp = sp_new;
		}
		sp = sp -> next;
		cp -> sp = sp;
	}
	
	void *object = sp -> freelist -> object;
	sp -> refcnt++;
	__rem_buf(sp, sp -> freelist, cp -> buf_sz);
	__inc_slab(sp);
	
	return sp -> freelist -> object;
	
}

void 
kmem_cache_free(struct kmem_cache *cp, void *buf)
{
	struct kmem_slab *sp = cp -> sp;
	
	sp -> refcnt--;
	__add_buf(sp, buf, cp -> buf_sz);
	__dec_slab(sp);
	
}

void 
kmem_cache_destroy(struct kmem_cache *cp)
{
}
