
#include <inc/lib.h>

// malloc()
//	This function use BEST FIT strategy to allocate space in heap
//  with the given size and return void pointer to the start of the allocated space

//	To do this, we need to switch to the kernel, allocate the required space
//	in Page File then switch back to the user again.
//
//	We can use sys_allocateMem(uint32 virtual_address, uint32 size); which
//		switches to the kernel mode, calls allocateMem(struct Env* e, uint32 virtual_address, uint32 size) in
//		"memory_manager.c", then switch back to the user mode here
//	the allocateMem function is empty, make sure to implement it.


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
#define USER_HEAP_SIZE (USER_HEAP_MAX - USER_HEAP_START)
uint32 allocation_counter = 0; //For indexing: allocated_mem[]
int firstentry = 1;
struct AllocatedMemory
{
	uint32 virtual_address;
	uint32 allocated_pages;
	int UserHeap_ind;
}allocated_mem[USER_HEAP_SIZE/PAGE_SIZE + 1];

struct UserHeap_mem
{
	uint32 virtual_address;
	bool present;
}UserHeapFile[USER_HEAP_SIZE/PAGE_SIZE + 1];

void intialize_heap(){
	for(int i = 0; i < USER_HEAP_SIZE/PAGE_SIZE + 1; i++)
		UserHeapFile[i].present = 0;
	firstentry =0;
}
uint32 required_num_pages;
uint32 consecutive_pages, tmp_start, tmp_ind, allocation_ind;
uint32 get_BESTFIT(uint32 size){
	required_num_pages = size/PAGE_SIZE + (size % PAGE_SIZE != 0);
	uint32 allocation_va = -1, best_allocation = USER_HEAP_SIZE/PAGE_SIZE + 1;
	consecutive_pages = 0, tmp_start = -1, tmp_ind = -1, allocation_ind = -1;

	if(sys_isUHeapPlacementStrategyBESTFIT()){
		for(int i = 0; i < USER_HEAP_SIZE/PAGE_SIZE ; i++){
			if(UserHeapFile[i].present){
				if(consecutive_pages >= required_num_pages && best_allocation > consecutive_pages){
					allocation_va = tmp_start;
					allocation_ind = tmp_ind;
					best_allocation = consecutive_pages;
				}
				tmp_start = -1;
				tmp_ind = -1;
				consecutive_pages = 0;
				continue;
			}
			if(!consecutive_pages){ //Fresh start on a new free range
				tmp_start = ((i*PAGE_SIZE) + USER_HEAP_START);
				tmp_ind = i;
			}
			consecutive_pages++;
		}
	}
	//Last Check
	if(consecutive_pages >= required_num_pages && best_allocation >= consecutive_pages) {
		allocation_va = tmp_start;
		allocation_ind = tmp_ind;
	}
	return allocation_va;
}

void* malloc(uint32 size)
{
	//TODO: [PROJECT 2019 - MS2 - [5] User Heap] malloc() [User Side]
	if (firstentry)
		intialize_heap();
	// Steps:
	//	1) Implement BEST FIT strategy to search the heap for suitable space
	//		to the required allocation size (space should be on 4 KB BOUNDARY)
	uint32 allocation_va = get_BESTFIT(size);

	//	2) if no suitable space found, return NULL
	//	 Else,
	if(allocation_va == -1) return (void*)NULL; //No suitable address was found

	//	3) Call sys_allocateMem to invoke the Kernel for allocation
	sys_allocateMem(allocation_va, size);

	for(uint32 i=0, ind = allocation_ind ; i < required_num_pages ; i++, ind++)
	{
		UserHeapFile[ind].present = 1;
		UserHeapFile[ind].virtual_address = allocation_va;
	}
	allocated_mem[allocation_counter].allocated_pages = required_num_pages;
	allocated_mem[allocation_counter].virtual_address = allocation_va;
	allocated_mem[allocation_counter].UserHeap_ind = allocation_ind;
	allocation_counter++;
	// 	4) Return pointer containing the virtual address of allocated space,
	//
	//This function should find the space of the required range
	// ******** ON 4KB BOUNDARY ******************* //

	//Use sys_isUHeapPlacementStrategyBESTFIT() to check the current strategy

	return (void*)allocation_va;
}

void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//TODO: [PROJECT 2019 - MS2 - [6] Shared Variables: Creation] smalloc() [User Side]
	//This function should find the space of the required range
	// ******** ON 4KB BOUNDARY ******************* //

	//Use sys_isUHeapPlacementStrategyBESTFIT() to check the current strategy
	if(sys_isUHeapPlacementStrategyBESTFIT()){
		// Steps:
		//	1) Implement BEST FIT strategy to search the heap for suitable space
		//		to the required allocation size (space should be on 4 KB BOUNDARY)
		uint32 allocation_va = get_BESTFIT(size);

		//	2) if no suitable space found, return NULL
		//	 Else,
		if(allocation_va == -1) return (void*)NULL; //No suitable address was found

		//	3) Call sys_createSharedObject(...) to invoke the Kernel for allocation of shared variable
		//		sys_createSharedObject(): if succeed, it returns the ID of the created variable. Else, it returns -ve
		int sharedID = sys_createSharedObject(sharedVarName,size, isWritable, (void*)allocation_va);
		//	4) If the Kernel successfully creates the shared variable, return its virtual address
		//	   Else, return NULL
		if(sharedID >= 0){
			for(uint32 i=0, ind = allocation_ind ; i < required_num_pages ; i++, ind++)
			{
				UserHeapFile[ind].present = 1;
				UserHeapFile[ind].virtual_address = allocation_va;
			}
			allocated_mem[allocation_counter].allocated_pages = required_num_pages;
			allocated_mem[allocation_counter].virtual_address = allocation_va;
			allocated_mem[allocation_counter].UserHeap_ind = allocation_ind;
			allocation_counter++;
			return (void*) allocation_va;
		}
	}
	return NULL;
}

void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//TODO: [PROJECT 2019 - MS2 - [6] Shared Variables: Get] sget() [User Side]
	//This function should find the space for sharing the variable
	// ******** ON 4KB BOUNDARY ******************* //

	//Use sys_isUHeapPlacementStrategyBESTFIT() to check the current strategy
	if(sys_isUHeapPlacementStrategyBESTFIT()){
		// Steps:
		//	1) Get the size of the shared variable (use sys_getSizeOfSharedObject())
		uint32 sharedSize = sys_getSizeOfSharedObject(ownerEnvID, sharedVarName);
		//	2) If not exists, return NULL
		if(sharedSize == E_SHARED_MEM_NOT_EXISTS)
			return NULL;
		//	3) Implement BEST FIT strategy to search the heap for suitable space
		//		to share the variable (should be on 4 KB BOUNDARY)
		uint32 allocation_va = get_BESTFIT(sharedSize);
		//	4) if no suitable space found, return NULL
		//	 Else,
		if(allocation_va == -1)
			return NULL;
		//	5) Call sys_getSharedObject(...) to invoke the Kernel for sharing this variable
		//		sys_getSharedObject(): if succeed, it returns the ID of the shared variable. Else, it returns -ve
		int nInd = sys_getSharedObject(ownerEnvID, sharedVarName, (void*)allocation_va);
		//	6) If the Kernel successfully share the variable, return its virtual address
		//	   Else, return NULL
		//
		if(nInd >= 0){
			for(uint32 i=0, ind = allocation_ind ; i < required_num_pages ; i++, ind++)
			{
				UserHeapFile[ind].present = 1;
				UserHeapFile[ind].virtual_address = allocation_va;
			}
			allocated_mem[allocation_counter].allocated_pages = required_num_pages;
			allocated_mem[allocation_counter].virtual_address = allocation_va;
			allocated_mem[allocation_counter].UserHeap_ind = allocation_ind;
			allocation_counter++;
			return (void*) allocation_va;
		}
	}
	return NULL;
}

// free():
//	This function frees the allocation of the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from page file and main memory then switch back to the user again.
//
//	We can use sys_freeMem(uint32 virtual_address, uint32 size); which
//		switches to the kernel mode, calls freeMem(struct Env* e, uint32 virtual_address, uint32 size) in
//		"memory_manager.c", then switch back to the user mode here
//	the freeMem function is empty, make sure to implement it.

void free(void* virtual_address)
{
	//TODO: [PROJECT 2019 - MS2 - [5] User Heap] free() [User Side]
	//you should get the size of the given allocation using its address
	uint32 num_pages_to_free = 0;
	int id;
	for(int i = 0; i < allocation_counter; i++)
		if(allocated_mem[i].virtual_address == (uint32)virtual_address){
			num_pages_to_free = allocated_mem[i].allocated_pages;
			id = i;
			break;
		}

	//Free in the user heap
	for(int i = 0, ind = allocated_mem[id].UserHeap_ind; i < num_pages_to_free; i++, ind++){
		UserHeapFile[ind].present = 0;
		UserHeapFile[ind].virtual_address = 0;
	}
	//Shift to delete the chosen VA
	for(int j = id + 1; j < allocation_counter; j++)
		allocated_mem[j - 1] = allocated_mem[j];
	allocation_counter--;
	//you need to call sys_freeMem()
	sys_freeMem((uint32)virtual_address, num_pages_to_free);
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//=============
// [1] sfree():
//=============
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_freeSharedObject(...); which switches to the kernel mode,
//	calls freeSharedObject(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the freeSharedObject() function is empty, make sure to implement it.

void sfree(void* virtual_address)
{
	//TODO: [PROJECT 2019 - BONUS4] Free Shared Variable [User Side]
	// Write your code here, remove the panic and write your code
	panic("sfree() is not implemented yet...!!");

	//	1) you should find the ID of the shared variable at the given address
	//	2) you need to call sys_freeSharedObject()

}


//===============
// [2] realloc():
//===============

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_moveMem(uint32 src_virtual_address, uint32 dst_virtual_address, uint32 size)
//		which switches to the kernel mode, calls moveMem(struct Env* e, uint32 src_virtual_address, uint32 dst_virtual_address, uint32 size)
//		in "memory_manager.c", then switch back to the user mode here
//	the moveMem function is empty, make sure to implement it.

void *realloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT 2019 - BONUS3] User Heap Realloc [User Side]
	// Write your code here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");

}
