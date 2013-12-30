#include <stdio.h>
#include <malloc.h>
#include <assert.h>

#include <kmem_cache.h>

#define INTER_FG 0.125

size_t void_sz = sizeof(void *);
size_t kmem_bufctl_sz = sizeof(struct kmem_bufctl);
size_t kmem_slab_sz = sizeof(struct kmem_slab);
size_t kmem_cache_sz = sizeof(struct kmem_cache);

int
__align_offset(size_t size, int align)
{
	return !align ? 0 : (align - size % align);
}

int 
__slab_sz(int obj_sz)
{
	int i, obj_amt;
	double inter_fg;

	for(i = 1; i < 10; i++){
		obj_amt = i * PAGE_SZ / obj_sz;

		inter_fg = 1 - ((double)obj_sz * obj_amt) / (i * PAGE_SZ);

		if( inter_fg < INTER_FG)
			return i * PAGE_SZ;
	}

	return i * PAGE_SZ;
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
	void *slab = memalign(cp -> slab_sz, cp -> slab_sz);

	assert(!((int)slab % PAGE_SZ));

	struct kmem_slab *sp_new = slab + cp -> buf_amt * cp -> buf_sz;

	printf("__create_slab: slab %d, sp %d, offset %d\n",slab, sp_new, (int)sp_new - (int)slab);


	__init_slab(sp_new, slab, cp -> buf_sz, cp -> buf_amt);
	__init_buf(slab, cp -> buf_sz, cp -> buf_amt);

	return sp_new;
}

void
__swap_slab(struct kmem_cache *cp, struct kmem_slab *curr)      //swap the curr and curr -> next
{
	struct kmem_slab *next = curr -> next;

	if( next && next -> refcnt > curr -> refcnt){

		printf("__swap_slab: curr refcnt %d, next refcnt %d\n", curr->refcnt, next->refcnt);
		
		if(cp -> sp_curr = curr)
			cp -> sp_curr = next;

		if(curr -> prev){				
			curr -> prev -> next = next;
			next -> prev = curr -> prev;
		}else{
			cp -> sp_head = next;
			next -> prev = NULL;
		}
	
		if(next -> next)
			next -> next -> prev = curr;

		curr -> next = next -> next;
		
		curr -> prev = next;
		next -> next = curr;	
	}
}

struct kmem_cache *
kmem_cache_create(char *name, size_t size, int align, kc_fn_t constructor, kc_fn_t destructor)
{
	struct kmem_cache *cp = memalign(PAGE_SZ, PAGE_SZ);
	
	cp -> name          = name;
	cp -> slab_sz       = __slab_sz(size);
	cp -> buf_sz        = size + __align_offset(size, align);
	cp -> buf_amt       = (PAGE_SZ - kmem_slab_sz) / cp -> buf_sz;
	cp -> align         = align;
	cp -> constructor   = constructor;
	cp -> destructor    = destructor;	
	
	//printf("kmem_cache_create: c_sz %d, s_sz %d, b_sz %d\n",kmem_cache_sz, kmem_slab_sz,kmem_bufctl_sz);
	printf("kmem_cache_create: cp %d, slab_sz %d, buf_sz %d, buf_amt %d\n",cp,cp->slab_sz, cp->buf_sz,cp->buf_amt);

	cp -> sp_head       = __create_slab(cp);
	cp -> sp_curr       = cp -> sp_head;
	return cp;
}

void *
kmem_cache_alloc(struct kmem_cache *cp, int flags)
{
	struct kmem_slab *sp = cp -> sp_curr;

	if(sp -> refcnt == cp -> buf_amt){	
		if( !sp -> next){
			sp -> next = __create_slab(cp);
			sp -> next -> prev = sp;	
		}
		sp = sp -> next;
		cp -> sp_curr = sp;
	}

	sp -> refcnt++;
	void *object = sp -> fl_head;
	void **temp = sp -> fl_head;

	if( sp -> refcnt == cp -> buf_amt)
		sp -> fl_head = sp -> fl_tail = NULL;
	else
		sp -> fl_head = temp[0];
	
	printf("kmem_cache_alloc: refcnt %d, fl_head %d\n", sp->refcnt, sp->fl_head);

	if(sp -> prev)	
		__swap_slab(cp, sp -> prev);
	
	return object;
}

void 
kmem_cache_free(struct kmem_cache *cp, void *buf)
{
	int offset  = cp -> buf_sz * ( cp -> buf_amt - ( (int)buf % PAGE_SZ) / cp -> buf_sz);
	
	struct kmem_slab *sp = buf + offset;
	
	printf("kmem_cache_free: sp %d, buf %d, buf offset %d\n", sp, buf, offset);

	if( sp -> refcnt == cp -> buf_amt){
		sp -> fl_head = sp -> fl_tail = buf;
	}else{
		void **temp = sp -> fl_tail;
		temp[0] = buf;
		sp -> fl_tail = buf;
	}

	sp -> refcnt--;

	__swap_slab(cp, sp);

	
	//printf("kmem_cache_free: refcnt %d, fl_tail %d\n", sp->refcnt, sp->fl_tail);
}

void 
kmem_cache_destroy(struct kmem_cache *cp)
{
	struct kmem_slab *sp = cp -> sp_head;
	
	while(sp -> next){
		sp = sp -> next;
		free( sp -> prev -> slab);
	}
	
	free(sp -> slab);
	free(cp);
}

void 
__print_slab(struct kmem_cache *cp)
{
	struct kmem_slab *sp = cp -> sp_head;
	
	printf("curr sp %d, slab list: %d %d, ", cp -> sp_curr, sp, sp -> refcnt);	

	while(sp -> next){
		sp = sp -> next;
		printf("%d, %d, ", sp, sp -> refcnt);
	}
	
	printf("\n");
}
