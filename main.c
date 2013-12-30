#include <stdio.h>
#include <math.h>

#include <kmem_cache.h>
#include <hash_table.h>

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

unsigned long long start, end;

#define AMT 300
#define LOOP 1000

struct foo{
	char i[500];
};

void 
alloc_and_free(){

	struct kmem_cache *foo_cache =kmem_cache_create("foo",sizeof(struct foo), 0, NULL, NULL);

	int i;
	struct foo *fp[AMT];

	for(i = 0; i < AMT; i++){
		fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
		__print_slab(foo_cache);
	}
	
	for(i = 0; i < AMT; i++){	
		kmem_cache_free(foo_cache,fp[i]);
		__print_slab(foo_cache);
	}
	for(i = 0; i < AMT; i++){
		fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
		__print_slab(foo_cache);
	}

	kmem_cache_destroy(foo_cache);

	printf("[alloc and free]: pass\n");

}

void 
perf_test()
{
	int i,j;

	struct foo *fp[AMT];

	struct kmem_cache *foo_cache =kmem_cache_create("foo",sizeof(struct foo), 0, NULL, NULL);

	//warm up loop
	for(i = 0; i < AMT; i++){
		fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
		fp[i] -> i[ 7 ] = 'a';
	}
	
	for(i = 0; i < AMT; i++)
		kmem_cache_free(foo_cache,fp[i]);


	rdtscll(start);

	for(j = 0; j < LOOP; j++){

		for(i = 0; i < AMT; i++){
			fp[i] = kmem_cache_alloc(foo_cache, KM_SLEEP);
			fp[i] -> i[ 6 ] = 'a';
		}

		for(i = 0; i < AMT; i++)
			kmem_cache_free(foo_cache,fp[i]);

	}

	rdtscll(end);

	kmem_cache_destroy(foo_cache);

	printf("[PERF] kmem_slab %lld \n", (end-start)/LOOP/AMT);


	rdtscll(start);

	for(j = 0; j < LOOP; j++){

		for(i = 0; i < AMT; i++){
			fp[i] = malloc(sizeof(struct foo));
			fp[i] -> i[ i% 7 ] = i;
		}
	
		for(i = 0; i < AMT; i++)
			free(fp[i]);
	}

	rdtscll(end);
	
	printf("[PERF] malloc %lld \n", (end-start)/LOOP/AMT);

}


int main()
{	
	//struct kmem_cache *foo_cache =kmem_cache_create("foo",1500, 0, NULL, NULL);

	//struct foo *fp = kmem_cache_alloc(foo_cache,0);

 	//alloc_and_free();
	perf_test();

	//printf("main end %d\n", __slab_sz(2000));

	return 0;
}
