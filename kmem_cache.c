#include <stdio.h>
#include <malloc.h>
#include <assert.h>

#include <kmem_cache.h>

size_t void_sz = sizeof(void *);
size_t kmem_bufctl_sz = sizeof(struct kmem_bufctl);
size_t kmem_slab_sz = sizeof(struct kmem_slab);
size_t kmem_cache_sz = sizeof(struct kmem_cache);

int
__align_offset(size_t size, int align)
{
	return !align ? 0 : (align - size % align);
}

void
__init_slab(void *ptr, void *slab, int buf_sz, int buf_amt)
{	
	struct kmem_slab *sp = ptr;

	sp -> slab          = slab;
	sp -> refcnt        = 0;
	sp -> next          = NULL;
	sp -> prev          = NULL;
	sp -> fl_head       = slab;
	sp -> fl_tail       = slab + buf_sz * (buf_amt - 1);
}

void 
__init_buf(void *slab, int buf_sz, int buf_amt)
{
	int i;

	void **curr;
	
	//printf("__init_buf: slab %d\n", slab);

	for(i = 0; i < buf_amt; i++){
		curr    = slab + buf_sz * i ;	
		curr[0] =  slab + buf_sz * (i + 1);

		//printf("__init_buf: curr_addr %d, curr_val %d\n", &curr[0], curr[0]);
	}
		
	curr[0]  = slab;
}

void *
__create_slab(struct kmem_cache *cp)
{
	void *slab = memalign(SLAB_SIZE, SLAB_SIZE);

	assert(!((int)slab % SLAB_SIZE));

	struct kmem_slab *sp_new = slab + cp -> buf_amt * cp -> buf_sz;

	//printf("__create_slab: slab %d, sp %d, offset %d\n",slab, sp_new, (int)sp_new - (int)slab);


	__init_slab(sp_new, slab, cp -> buf_sz, cp -> buf_amt);
	__init_buf(slab, cp -> buf_sz, cp -> buf_amt);
	
	if(cp -> sp){
		cp -> sp -> next = sp_new;
		sp_new -> prev = cp -> sp;	
		cp -> sp = sp_new;
	}else{
		cp -> sp = sp_new;
	}

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

struct kmem_cache *
kmem_cache_create(char *name, size_t size, int align, kc_fn_t constructor, kc_fn_t destructor)
{
	struct kmem_cache *cp = memalign(SLAB_SIZE, SLAB_SIZE);
	
	cp -> name          = name;
	cp -> buf_sz        = size + __align_offset(size, align);
	cp -> buf_amt       = (SLAB_SIZE - kmem_slab_sz) / cp -> buf_sz;
	cp -> align         = align;
	cp -> constructor   = constructor;
	cp -> destructor    = destructor;	
	
	//printf("kmem_cache_create: c_sz %d, s_sz %d, b_sz %d\n",kmem_cache_sz, kmem_slab_sz,kmem_bufctl_sz);
	//printf("kmem_cache_create: cp %d, buf_sz %d, buf_amt %d\n",cp,cp->buf_sz,cp->buf_amt);

	cp -> sp            = __create_slab(cp);
	
	return cp;
}

void *
kmem_cache_alloc(struct kmem_cache *cp, int flags)
{
	struct kmem_slab *sp = cp -> sp;

	if( sp -> refcnt == cp -> buf_amt && !sp -> next){	
		sp = __create_slab(cp);
	}

	sp -> refcnt++;
	void *object = sp -> fl_head;
	void **temp = sp -> fl_head;

	if( sp -> refcnt == cp -> buf_amt)
		sp -> fl_head = sp -> fl_tail = NULL;
	else
		sp -> fl_head = temp[0];

	//__sort_slab(sp);
	
	//printf("kmem_cache_alloc: refcnt %d, fl_head %d\n", sp->refcnt, sp->fl_head);

	return object;
}

void 
kmem_cache_free(struct kmem_cache *cp, void *buf)
{
	int offset  = cp -> buf_sz * ( cp -> buf_amt - ( (int)buf % SLAB_SIZE) / cp -> buf_sz);
	
	struct kmem_slab *sp = buf + offset;
	
	//printf("kmem_cache_free: sp %d, buf %d, buf offset %d\n", sp, buf, offset);

	if( sp -> refcnt == cp -> buf_amt){
		sp -> fl_head = sp -> fl_tail = buf;
	}else{
		void **temp = sp -> fl_tail;
		temp[0] = buf;
		sp -> fl_tail = buf;
	}

	sp -> refcnt--;
	//__sort_slab(sp -> next);
		
	//printf("kmem_cache_free: refcnt %d, fl_tail %d\n", sp->refcnt, sp->fl_tail);

}

void 
kmem_cache_destroy(struct kmem_cache *cp)
{
	struct kmem_slab *sp = cp -> sp;
	
	while(sp -> next){
		sp = sp -> next;
		free( sp -> prev -> slab);
	}
	
	free(sp -> slab);
	free(cp);
}
