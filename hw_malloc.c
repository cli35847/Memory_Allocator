#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "hw_malloc.h"

#define MEMORY_ALLOC_UNIT	(64*1024)
#define MINIMUM_CHUNK_SIZE	(48)
#define CHUNK_HDR_SIZE	sizeof(struct chunk_header)
#define ROUND8(s)	(((s)+7) & ~0x7)

pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER;

#define MEM_LOCK	pthread_mutex_lock(&mlock)
#define MEM_UNLOCK	pthread_mutex_unlock(&mlock)

void *start_brk;

// bin[] is used to store the free memory.
// chunk header is 40 byes long.
// bin[0] => 40+8, ... bin[5] => 40+6*8
// bin[6] => > 40+7*8
// chunk must be 8 bytes alignment.
//
struct chunk_header bin[7];

void *get_end_sbrk(void)
{
	return sbrk(0);
}

//get adjacent chunk after ptr.
struct chunk_header *chunk_after(struct chunk_header *ptr)
{

	return  (struct chunk_header*) (ptr->chunk_size + (chunk_ptr_t) ptr);
}

//get adjacent chunk before the ptr.
struct chunk_header *chunk_before(struct chunk_header *ptr)
{
	return (struct chunk_header*) (ptr->pre_chunk_size*-1 + (chunk_ptr_t) ptr);
}

int get_bin_num(struct chunk_header *p)
{
	return (p->chunk_size-CHUNK_HDR_SIZE)/8 - 1;
}

void initialize_malloc(void)
{
struct chunk_header *chunk;
int i;

	start_brk = sbrk(MEMORY_ALLOC_UNIT);

	chunk = (struct chunk_header*) start_brk;
	chunk->chunk_size = MEMORY_ALLOC_UNIT;
	chunk->prev = chunk->next = (chunk_ptr_t) &bin[6];
	bin[6].prev = bin[6].next = (chunk_ptr_t) chunk;

	for(i=0; i < 6; i++)
	{
		bin[i].prev = bin[i].next = (chunk_ptr_t) &bin[i];
	}

}

//add entry to tail of double link list.
void add_entry(struct chunk_header *head, struct chunk_header *entry)
{
struct chunk_header *prev;

	prev = (struct chunk_header*) head->prev;
	head->prev =(chunk_ptr_t) entry;
	entry->prev = (chunk_ptr_t) prev;
	entry->next = (chunk_ptr_t) head;
	prev->next  = (chunk_ptr_t) entry;
}

//add entry with chunk size in descending order for bin[6]
void add_entry_descend(struct chunk_header *head, struct chunk_header *entry)
{
struct chunk_header *ptr=(struct chunk_header *) head->next;

	while (ptr != head)
	{
		if (ptr->chunk_size < entry->chunk_size)
		{
			//insert entry before ptr,
			//equal to insert at the tail of ptr (treat ptr as head).
			add_entry(ptr, entry);
			return;
		}
		ptr = (struct chunk_header*) ptr->next;
	}
	//run to here mean this entry is the smallest one or the link list is empty
	//add to the tail of this bin.
	add_entry(head, entry);
}

//remove entry from double link list.
void remove_entry(struct chunk_header *entry)
{
struct chunk_header *prev, *next;

	prev = (struct chunk_header*) entry->prev;
	next = (struct chunk_header*) entry->next;
	prev->next = entry->next;
	next->prev = (chunk_ptr_t) prev;

	//clear entry link list
	entry->prev = entry->next = 0;
}

//best fit algorithm, go over the list and find the smallest size meet &
//the address is lowest.
//and remove it from list.
struct chunk_header *best_fit(struct chunk_header *head, size_t chsize)
{
struct chunk_header *ptr = (struct chunk_header *) head->next;
struct chunk_header *smallest=(struct chunk_header *) NULL;


	while (ptr != head)
	{
		if (ptr->chunk_size < chsize)
			break; 
		if (smallest)
		{
			if (ptr->chunk_size < smallest->chunk_size)
				smallest = ptr;
			else if (ptr->chunk_size == smallest->chunk_size)
			{ //have same size, compare who has lowest address.
				if ( ptr < smallest)
					smallest = ptr;
			}
		}
		else
			smallest = ptr;

		ptr = (struct chunk_header *)ptr->next;
	}
	
	if (smallest)
	{
	//remove smallest from link list
		remove_entry(smallest);
	}

	return smallest;
}

//increase another 64K
int grow_memory(void)
{
struct chunk_header *chunk;

	chunk = (struct chunk_header*) sbrk(MEMORY_ALLOC_UNIT);
        chunk->chunk_size = MEMORY_ALLOC_UNIT;
	add_entry_descend(&bin[6], chunk);

	return 1;
}

//shrink memory only from the end of brk.
struct chunk_header *shrink_memory(struct chunk_header *chunk)
{
char *ptr = (char*) chunk;

	if ( (chunk->chunk_size < MEMORY_ALLOC_UNIT) ||
		(ptr + chunk->chunk_size) < (char*)get_end_sbrk())
		return chunk;
	
	//split chunk 64k to shrink memory.
	chunk->chunk_size -= MEMORY_ALLOC_UNIT;
	if (chunk->chunk_size == 0)
		chunk = NULL;
	//return 64K to system.
	sbrk(0-MEMORY_ALLOC_UNIT);
	if ( hw_get_start_brk() == sbrk(0) )
		start_brk = NULL;

	printf("shrink memory to %p\n", get_end_sbrk());
	if (chunk)
		return shrink_memory(chunk);
	else
		return NULL;
}

//alloc memory from bin[6] which have memory to split or to meet the requested size
struct chunk_header *bin_alloc(size_t chsize)
{
struct chunk_header *ptr, *new_chunk;

	ptr = best_fit(&bin[6], chsize);
	if (ptr == NULL)
	{
		printf("grow memory\n");
		if ( grow_memory() )
			return bin_alloc(chsize);
		else
			return NULL;
	}

	//check if need split or not.
	if (ptr->chunk_size >= (chsize + MINIMUM_CHUNK_SIZE))
	{
		int bin_num;

		//lets split it.
		//and return lower address to user
		new_chunk = (struct chunk_header*) (chsize + (chunk_ptr_t) ptr);
		new_chunk->prev = new_chunk->next = 0;
		new_chunk->chunk_size = ptr->chunk_size - chsize;
		new_chunk->pre_chunk_size = chsize;
		new_chunk->prev_free_flag = 0;
		//reduce original chunk size
		ptr->chunk_size = chsize;
		//add new chunk to free list.
		bin_num = get_bin_num(new_chunk);
		if (bin_num < 6)
			add_entry(&bin[bin_num], new_chunk);
		else
			add_entry_descend(&bin[6], new_chunk);
	}
	else
	{
		struct chunk_header *adj;
		// ptr will be used, update next adjacent chunk that prev is not free.
		adj = chunk_after(ptr);
		adj->prev_free_flag = 0;
	}

	return ptr;
}

void *alloc(size_t bytes)
{
int chsize = ROUND8(bytes + CHUNK_HDR_SIZE);
int bin_num;
struct chunk_header *ptr;

	//printf("chunk size = %d\n", chsize);

	bin_num = (ROUND8(bytes))/8 - 1;
	if (bin_num < 6)
	{
		ptr = best_fit(&bin[bin_num], chsize);
		if (ptr != NULL)
		{
			// <need set free flag.>
			return ptr;
		}
		//ptr == NULL, go through to alloc from bin[6].
	}

	ptr = bin_alloc(chsize);

	return (void*) (ptr+1);
}

void *hw_malloc(size_t bytes)
{
void *ptr;

	if (bytes == 0)
		return NULL;
	if ((bytes + sizeof(struct chunk_header)) > MEMORY_ALLOC_UNIT)
		return NULL;

	MEM_LOCK;
	if (start_brk==NULL)
		initialize_malloc();

	ptr = alloc(bytes);
	MEM_UNLOCK;

	return ptr;
}

int hw_free(void *mem)
{
int bin_num;
struct chunk_header *ptr = (struct chunk_header *) ((char*)(mem) - CHUNK_HDR_SIZE);
struct chunk_header *adj;

	if (mem == NULL)
		return 0;
//printf("free chunk size=%lld\n", ptr->chunk_size);

	MEM_LOCK;
	//check if we can merge chunk before ptr.
	while (((void*)(adj=chunk_before(ptr))) >= hw_get_start_brk())
	{
		if (!ptr->prev_free_flag)
			break;

		remove_entry(adj);
		adj->chunk_size += ptr->chunk_size;
		memset(ptr,0,CHUNK_HDR_SIZE);
		ptr = adj;
	}
	//check if we can merge chunk after ptr.
	while (((void*)(adj=chunk_after(ptr))) < get_end_sbrk())
	{
		if (adj->next == 0) //adj chunk is in used.
		{
			//tell adj chunk, we are free now.
			adj->prev_free_flag = 1;
			adj->pre_chunk_size = ptr->chunk_size;
			break;
		}
		//remove adj from link list.
		remove_entry(adj);
		ptr->chunk_size += adj->chunk_size;
		memset(adj,0,CHUNK_HDR_SIZE);
	}
	
	ptr = shrink_memory(ptr);
	if ( ptr )
	{
		bin_num = get_bin_num(ptr);

		if (bin_num < 6)
			add_entry(&bin[bin_num], ptr);
		else
			add_entry_descend(&bin[6], ptr);
	}
	MEM_UNLOCK;
	return 1;
}

void *hw_get_start_brk(void)
{
	return start_brk;
}

#if 1
void print_chunk(struct chunk_header *ptr)
{
printf("chunk ptr=%p, size=%lld\n", (void*)ptr,(long long) ptr->chunk_size);
printf("pre chunk size=%lld, prev_free_flag=%lld\n", ptr->pre_chunk_size, ptr->prev_free_flag);
printf("chunk prev=%lld, next=%lld \n", (long long)ptr->prev,(long long) ptr->next);
}

void print_bin(int num)
{
struct chunk_header *ptr;

	if (num > 6)
		return;

	ptr = (struct chunk_header*) bin[num].next;

	while(ptr != &bin[num])
	{
		printf("0x%08x--------%lld\n",(unsigned int)((void*)ptr - hw_get_start_brk()),(long long) ptr->chunk_size);
		ptr = (struct chunk_header*) ptr->next;
	}
}
#endif

