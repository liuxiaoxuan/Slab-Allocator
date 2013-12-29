#include <stdio.h>


#include <kmem_cache.h>


struct foo{
	int i[20];
	void * ptr;
};

void 
alloc_and_free(){

	struct kmem_cache *foo_cache =kmem_cache_create("foo",sizeof(struct foo), 0, NULL, NULL);

	int i;

	struct foo *fp[15];

	for(i = 0; i < 15; i++){
		fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
		//printf("fp[%d] %d\n",i,fp[i]);
	}
	
	for(i = 14; i >= 0; i--)
		kmem_cache_free(foo_cache,fp[i]);

	for(i = 0; i < 15; i++){
		fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
		//printf("fp[%d] %d\n",i,fp[i]);
	}

	kmem_cache_destroy(foo_cache);

	printf("[alloc_and free]:done\n");

}

void alloc_more(){

	struct kmem_cache *foo_cache =kmem_cache_create("foo",sizeof(struct foo), 0, NULL, NULL);

	int i;

	struct foo *fp[15];

	for(i = 0; i < 15; i++){
		fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
		//printf("fp[%d] %d\n",i,fp[i]);
	}

}



int main()
{

 	alloc_and_free();

	printf("main end \n");

	return 0;
}
