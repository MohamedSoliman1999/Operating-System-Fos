#include <inc/memlayout.h>
#include <kern/kheap.h>
#include <kern/memory_manager.h>

//NOTE: All kernel heap allocations are multiples of PAGE_SIZE (4KB)

#define KERNEL_HEAP_SIZE (KERNEL_HEAP_MAX - KERNEL_HEAP_START)
uint32 allocation_counter = 0; //For indexing: allocated_mem[]
struct AllocatedMemory
{
	uint32 virtual_address;
	uint32 allocated_pages;
}allocated_mem[KERNEL_HEAP_SIZE/PAGE_SIZE + 1];

//using frame number
uint32 va_retrieval[(1<<20) + 1];

void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT 2019 - MS1 - [1] Kernel Heap] kmalloc()
	uint32 required_num_pages = size/PAGE_SIZE + (size % PAGE_SIZE != 0);

	uint32 allocation_va = -1, best_allocation = KERNEL_HEAP_SIZE/PAGE_SIZE + 1;

	uint32 *ptr_table;
	struct Frame_Info* ptr_frame_info;

	uint32 consecutive_pages = 0, tmp_start = -1;
	for(uint32 va = KERNEL_HEAP_START ; va < KERNEL_HEAP_MAX ; va += PAGE_SIZE) {
		ptr_frame_info = get_frame_info(ptr_page_directory, (void *)va, &ptr_table);
		//The VA is mapped
		if(ptr_frame_info != NULL) {
			if(consecutive_pages >= required_num_pages && best_allocation > consecutive_pages) {
				allocation_va = tmp_start;
				best_allocation = consecutive_pages;
			}
			tmp_start = -1;
			consecutive_pages = 0;
			continue;
		}
		if(!consecutive_pages) //Fresh start on a new free range
			tmp_start = va;
		consecutive_pages++;
	}
	//Last Check
	if(consecutive_pages >= required_num_pages && best_allocation > consecutive_pages) {
		allocation_va = tmp_start;
		best_allocation = consecutive_pages;
	}
	if(allocation_va == -1) return (void*)NULL; //No suitable address was found

	for(uint32 i=0, va = allocation_va ; i < required_num_pages ; i++, va += PAGE_SIZE) {
		ptr_frame_info = get_frame_info(ptr_page_directory, (void *)va, &ptr_table);
		int result;
		if(ptr_frame_info == NULL)
			result = allocate_frame(&ptr_frame_info);

		if(result == E_NO_MEM){
			cprintf("FAILED, no physical memory available.\n");
			return (void*)NULL;
		}

		result = map_frame(ptr_page_directory, ptr_frame_info, (void*)va, PERM_WRITEABLE|PERM_PRESENT);
		if(result != 0){
			cprintf("FAILED, no page table found and there’s no free frame for creating it.\n");
			free_frame(ptr_frame_info);
			return (void*)NULL;
		}
		uint32 PA = to_physical_address(ptr_frame_info);
		uint32 frame_number = PA/PAGE_SIZE;
		va_retrieval[frame_number] = va;
	}
	allocated_mem[allocation_counter].allocated_pages = required_num_pages;
	allocated_mem[allocation_counter].virtual_address = allocation_va;
	allocation_counter++;

	//TODO: [PROJECT 2019 - BONUS1] Implement the FIRST FIT strategy for Kernel allocation
	// Beside the BEST FIT
	// use "isKHeapPlacementStrategyFIRSTFIT() ..." functions to check the current strategy

	return (void*)allocation_va;
}


void kfree(void* virtual_address)
{
	//TODO: [PROJECT 2019 - MS1 - [1] Kernel Heap] kfree()
	//you need to get the size of the given allocation using its address
	//found in allocated_mem[]
	uint32 num_pages_to_free = 0;
	int id;
	for(int i = 0; i < allocation_counter; i++){
		if(allocated_mem[i].virtual_address == (uint32)virtual_address){
			num_pages_to_free = allocated_mem[i].allocated_pages;
			id = i;
			break;
		}
	}
	//Shift to delete the chosen VA
	for(int j = id + 1; j < allocation_counter; j++)
		allocated_mem[j - 1] = allocated_mem[j];
	allocation_counter--;
	uint32 *ptr_table;
	for(int i = 0, va = (uint32)virtual_address; i < num_pages_to_free; i++, va += PAGE_SIZE){
		struct Frame_Info *ptr_frame_info = get_frame_info(ptr_page_directory, (void *)va, &ptr_table);
		uint32 PA = to_physical_address(ptr_frame_info);
		uint32 frame_number = PA/PAGE_SIZE;
		va_retrieval[frame_number] = 0;
		unmap_frame(ptr_page_directory, (void*)va);
	}
}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT 2019 - MS1 - [1] Kernel Heap] kheap_virtual_address()
	//return the virtual address corresponding to given physical_address

	struct Frame_Info* ptr_frame_info = to_frame_info(physical_address), *ptr_frame_info_tst;
	if(ptr_frame_info == NULL)
		return 0;

	uint32 frame_number = (physical_address/PAGE_SIZE);
	if(va_retrieval[frame_number] != 0)
		return va_retrieval[frame_number];
	return 0;

	//old method
	/*
	uint32* ptr_page_table;
	for(int va = KERNEL_HEAP_START; va < KERNEL_HEAP_MAX; va += PAGE_SIZE){
		ptr_frame_info_tst = get_frame_info(ptr_page_directory, (void*) va, &ptr_page_table);
		if(ptr_frame_info_tst != NULL && ptr_frame_info_tst == ptr_frame_info)
			return va;
	}
	return 0;
	*/
}

unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT 2019 - MS1 - [1] Kernel Heap] kheap_physical_address()
	//return the physical address corresponding to given virtual_address
    if (virtual_address < KERNEL_HEAP_START || virtual_address > KERNEL_HEAP_MAX)
    	return 0;

	struct Frame_Info* ptr_frame_info;
	uint32 *pageT;
	ptr_frame_info = get_frame_info(ptr_page_directory, (void *)virtual_address, &pageT);
	if(ptr_frame_info == NULL)
		return 0;

	uint32 phyAdd = to_physical_address(ptr_frame_info);
	return phyAdd;
}


//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT 2019 - BONUS2] Kernel Heap Realloc
	// Write your code here, remove the panic and write your code

	return NULL;
	panic("krealloc() is not implemented yet...!!");

}
