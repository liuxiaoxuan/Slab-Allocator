#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include <math.h>

#include <kmem_cache.h>

#define INTER_FG 0.125

#define INC 1
#define DEC 0

size_t void_sz = sizeof(void *);
size_t kmem_bufctl_sz = sizeof(struct kmem_bufctl);
size_t kmem_slab_sz = sizeof(struct kmem_slab);
size_t kmem_cache_sz = sizeof(struct kmem_cache);

int
__align(int align)
{
	if(!align)
		return align;
	if(align == 1)
		return 2;

	return pow(2, (int)(log2(align) + 0.5)); 
}

int 
__buf_sz(int obj_sz, int align)
{
	if(!align || !(obj_sz % align))
		return obj_sz;

	//printf("%d %d %d \n", obj_sz, align, obj_sz / align);

	return align * ( (int) (obj_sz / align) + 1);	
}

int 
__slab_sz(int buf_sz, int align)
{
	int i, buf_amt;
	double inter_fg;

	for(i = 1; i < 10; i++){
		buf_amt = (i * PAGE_SZ - align) / buf_sz;

		inter_fg = 1 - ((double)buf_amt * buf_sz) / (i * PAGE_SZ);

		if( inter_fg < INTER_FG)
			return i * PAGE_SZ;
	}

	return i * PAGE_SZ;
}

void 
__init_buf(struct kmem_slab *sp, struct kmem_cache *cp)
{
	int i;

	void **curr;

	for(i = 1; i <= cp -> buf_amt; i++){
		curr    = sp -> bufstart + cp -> buf_sz * i - void_sz;
		*curr   = sp -> bufstart + cp -> buf_sz * i;

		//printf("__init_buf: curr_addr %d, curr_val %d\n", curr, *curr);

		if(cp -> slab_sz != PAGE_SZ){
			curr  = sp -> bufstart + cp -> buf_sz * i - void_sz * 2;		
			*curr = sp;
			//printf("__init_buf: curr_addr %d, curr_val %d\n", curr, *curr);

		}	
	}

	curr    = sp -> bufstart + cp -> buf_sz * (i - 1) - void_sz;
	*curr   = sp -> bufstart;
}

void *
__create_slab(struct kmem_cache *cp)
{
	void *slab;
	struct kmem_slab *sp;
	int offset = 0;

	if(cp -> slab_sz == PAGE_SZ){
		slab = memalign(cp -> slab_sz, cp -> slab_sz);
	}else{
		slab = malloc(cp -> slab_sz);
		if(cp -> align)
			offset = cp -> align - (int)slab % cp -> align;
	}
	
	sp = slab + offset + cp -> buf_amt * cp -> buf_sz;

	sp -> bufstart = slab + offset;
	sp -> slab          = slab;
	sp -> refcnt        = 0;
	sp -> next          = NULL;
	sp -> prev          = NULL;

	sp -> fl_head       = sp -> bufstart;
	sp -> fl_tail       = sp -> fl_head + (cp -> buf_sz - 1) * cp -> buf_amt;

	//printf("__create_slab: slab %d, sp %d, bufstart %d\n",slab, sp, sp -> bufstart);

	__init_buf(sp, cp);

	return sp;
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

	align = __align(align);
	
	if(size > BUF_SZ)
		size += sizeof(void *) * 2;
	
	int buf_sz = __buf_sz(size, align);

	cp -> name          = name;
	cp -> align         = align;
	cp -> buf_sz        = buf_sz;
	cp -> slab_sz       = __slab_sz(buf_sz, align);
	cp -> buf_amt       = (cp -> slab_sz - kmem_slab_sz - align ) / buf_sz;
	cp -> constructor   = constructor;
	cp -> destructor    = destructor;	
	cp -> sp_full       = NULL;
	cp -> sp_part       = NULL;
	cp -> sp_empt       = NULL;
	
	//printf("kmem_cache_create: cp %d, slab_sz %d, buf_sz %d, buf_amt %d\n",cp,cp->slab_sz, cp->buf_sz,cp->buf_amt);

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
	
	if( sp -> refcnt == cp -> buf_amt){
		sp -> fl_head = sp -> fl_tail = NULL;	
		__rem_slab(&(cp -> sp_part), sp);
		__add_slab(&(cp -> sp_full), sp);
	}else{
		void **next   = object + cp -> buf_sz - void_sz;
		sp -> fl_head = *next;
	}

	//printf("kmem_cache_alloc: sp %d, refcnt %d, fl_head %d obj %d\n", sp, sp->refcnt, sp->fl_head, object);

	return object;
}

void 
kmem_cache_free(struct kmem_cache *cp, void *buf)
{
	struct kmem_slab *sp;

	if(cp -> slab_sz == PAGE_SZ){
		int offset = cp -> buf_sz * ( cp -> buf_amt - ( (int)buf % PAGE_SZ) / cp -> buf_sz);
		sp = buf + offset;
	}else{
		void **temp = buf + cp -> buf_sz - void_sz * 2;
		sp = *temp;
	}

	if( sp -> refcnt == cp -> buf_amt){
		sp -> fl_head = sp -> fl_tail = buf;
		__rem_slab(&(cp -> sp_full), sp);
		__add_slab(&(cp -> sp_part), sp);
	
	}else{
		void **next = sp -> fl_tail + cp -> buf_sz - void_sz;
		*next = buf;
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
