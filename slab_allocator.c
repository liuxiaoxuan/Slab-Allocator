#include <stdio.h>

#include<slab_allocator.h>


struct kmem_cache *
kmem_cache_create(char *name, size_t size, int align, kc_fn_t constructor, kc_fn_t destructor)
{
}

void *
kmem_cache_alloc(struct kmem_cache *cp, int flags)
{
}

void 
kmem_cache_free(struct kmem_cache *cp, void *buf)
{
}

void 
kmem_cache_destroy(struct kmem_cache *cp)
{
}
