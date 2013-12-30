#include <stdio.h>
#include <malloc.h>
#include <assert.h>

#include <kmem_cache.h>

#define INTER_FG 0.125

#define INC 1
#define DEC 0

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
		*curr   =  slab + buf_sz * (i + 1);

		//printf("__init_buf: curr_addr %d, curr_val %d\n", &curr[0], curr[0]);
	}
		
	curr[0]  = slab;
}

void *
__create_slab(struct kmem_cache *cp)
{
	void *slab = memalign(cp -> slab_sz, cp -> slab_sz);

	assert(!((int)slab % cp -> slab_sz));

	struct kmem_slab *sp_new = slab + cp -> buf_amt * cp -> buf_sz;

	//printf("__create_slab: slab %d, sp %d, offset %d\n",slab, sp_new, (int)sp_new - (int)slab);

	__init_slab(sp_new, slab, cp -> buf_sz, cp -> buf_amt);
	__init_buf(slab, cp -> buf_sz, cp -> buf_amt);

	return sp_new;
}

void 
__add_slab(struct kmem_slab **head, struct kmem_slab *curr)
{
	if(!(*head)){
		*head = curr;
		return;
	}
		
	struct kmem_slab *next = (*head) -> next;
	
	(*head) -> next = curr;
	curr -> prev = (*head);
	curr -> next = next;

	if(next)
		next -> prev = curr;
}

void
__rem_slab(struct kmem_slab **head, struct kmem_slab *curr)
{
	struct kmem_slab *next = curr -> next;
	struct kmem_slab *prev = curr -> prev;

	if(curr == (*head)){
		(*head) = next;
	}else{
		prev -> next = next;
	}
	
	if(next) 
		next -> prev = prev;
	
	curr -> next = curr -> prev = NULL;
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

	//printf("kmem_cache_create: cp %d, slab_sz %d, buf_sz %d, buf_amt %d\n",cp,cp->slab_sz, cp->buf_sz,cp->buf_amt);
	cp -> sp_full       = NULL;
	cp -> sp_part       = NULL;
	cp -> sp_empt       = NULL;

	return cp;
}

void *
kmem_cache_alloc(struct kmem_cache *cp, int flags)
{
	struct kmem_slab *sp;

	if(!cp -> sp_part){
		if(cp -> sp_empt){
			sp =  cp -> sp_empt;
			__rem_slab(&(cp -> sp_empt), sp);
			__add_slab(&(cp -> sp_part), sp);
		}else{
			cp -> sp_part = __create_slab(cp);
		}
	}

	sp = cp -> sp_part;

	sp -> refcnt++;
	void *object = sp -> fl_head;
	void **temp = sp -> fl_head;

	if( sp -> refcnt == cp -> buf_amt){
		sp -> fl_head = sp -> fl_tail = NULL;	
		__rem_slab(&(cp -> sp_part), sp);
		__add_slab(&(cp -> sp_full), sp);
	}else{
		sp -> fl_head = *temp;
	}

	//printf("kmem_cache_alloc: sp %d, refcnt %d, fl_head %d\n", sp, sp->refcnt, sp->fl_head);

	return object;
}

void 
kmem_cache_free(struct kmem_cache *cp, void *buf)
{
	int offset  = cp -> buf_sz * ( cp -> buf_amt - ( (int)buf % PAGE_SZ) / cp -> buf_sz);
	
	struct kmem_slab *sp = buf + offset;
	
	if( sp -> refcnt == cp -> buf_amt){
		sp -> fl_head = sp -> fl_tail = buf;
		__rem_slab(&(cp -> sp_full), sp);
		__add_slab(&(cp -> sp_part), sp);
	
	}else{
		void **temp = sp -> fl_tail;
		*temp = buf;
		sp -> fl_tail = buf;
	}	

	sp -> refcnt--;

	if(sp -> refcnt == 0){
		__rem_slab(&(cp -> sp_part), sp);
		__add_slab(&(cp -> sp_empt), sp);
	}

	//printf("kmem_cache_free: sp %d, refcnt %d, fl_tail %d\n", sp, sp->refcnt, sp->fl_tail);
}

void
__free_slab(struct kmem_slab *head)
{
	struct kmem_slab *curr;

	while(head){
		curr = head;
		head = head -> next;
		free(curr -> slab);
	}	
}

void 
kmem_cache_destroy(struct kmem_cache *cp)
{
	__free_slab(cp -> sp_full);
	__free_slab(cp -> sp_part);
	__free_slab(cp -> sp_empt);
	
	free(cp);
}

void 
__print_slab(struct kmem_cache *cp)
{
	struct kmem_slab *sp;
	
	printf("sp_full: ");	
	for(sp = cp -> sp_full; sp; sp = sp -> next)
		printf("%d, %d, ", sp, sp -> refcnt);
	printf("\n");
	
		
	printf("sp_part: ");	
	for(sp = cp -> sp_part; sp; sp = sp -> next)
		printf("%d, %d, ", sp, sp -> refcnt);
	printf("\n");
	
	printf("sp_empt: ");	
	for(sp = cp -> sp_empt; sp; sp = sp -> next)
		printf("%d, %d, ", sp, sp -> refcnt);
	printf("\n");
	
}
