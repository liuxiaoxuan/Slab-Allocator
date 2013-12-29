#include <stdio.h>


#include <kmem_cache.h>

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

unsigned long long start, end;


struct foo{
	int i[63];
	void * ptr;
};

void 
alloc_and_free(){

	struct kmem_cache *foo_cache =kmem_cache_create("foo",sizeof(struct foo), 0, NULL, NULL);

	int i;

	struct foo *fp[30];



	for(i = 0; i < 30; i++){
		fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
		//printf("fp[%d] %d\n",i,fp[i]);
	}
	
	for(i = 29; i >= 0; i--)
		kmem_cache_free(foo_cache,fp[i]);

	for(i = 0; i < 30; i++){
		fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
		//printf("fp[%d] %d\n",i,fp[i]);
	}

	kmem_cache_destroy(foo_cache);

	printf("[alloc and free]: pass\n");

}

void 
perf_test()
{
	struct kmem_cache *foo_cache =kmem_cache_create("foo",sizeof(struct foo), 0, NULL, NULL);

	int i,j;

	struct foo *fp[30];

	rdtscll(start);

	for(j = 0; j < 100; j++){

		for(i = 0; i < 30; i++){
			fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
			fp[i] -> i[ i% 7 ] = i;
		}
	
		for(i = 0; i < 30; i++)
			kmem_cache_free(foo_cache,fp[i]);
	}

	rdtscll(end);

	kmem_cache_destroy(foo_cache);

	printf("[PERF] kmem_slab %lld \n", (end-start)/100);


	rdtscll(start);

	for(j = 0; j < 100; j++){

		for(i = 0; i < 30; i++){
			fp[i] = malloc(sizeof(struct foo));
			fp[i] -> i[ i% 7 ] = i;
		}
	
		for(i = 0; i < 30; i++)
			free(fp[i]);
	}

	rdtscll(end);
	
	printf("[PERF] malloc %lld \n", (end-start)/100);

}


int main()
{

 	//alloc_and_free();
	perf_test();
	printf("main end \n");

	return 0;
}
