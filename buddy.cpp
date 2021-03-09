/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s1810150
 */
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER 17

/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:
	/**
	 * Returns the number of pages that comprise a 'block', in a given order.
	 * @param order The order to base the calculation off of.
	 * @return Returns the number of pages in a block, in the order.
	 */
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		 * For example, in order-2, there are (1 << 2) == 4 pages in each block.
		 */
		return (1 << order);
	}
	
	/**
	 * Returns TRUE if the supplied page descriptor is correctly aligned for the 
	 * given order.  Returns FALSE otherwise.
	 * @param pgd The page descriptor to test alignment for.
	 * @param order The order to use for calculations.
	 */
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}
	
	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER) {
			return NULL;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return NULL;
		}
				
		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
			sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) : 
			sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);
		
		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}
	
	/**
	 * Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	 * @param pgd The page descriptor of the block to insert.
	 * @param order The order in which to insert the block.
	 * @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
	 * was inserted into.
	 */
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, find the slot in which the page descriptor
		// should be inserted.
		PageDescriptor **slot = &_free_areas[order];
		
		// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
		// greater than what the slot is pointing to.
		while (*slot && pgd > *slot) {
			slot = &(*slot)->next_free;
		}
		
		// Insert the page descriptor into the linked list.
		pgd->next_free = *slot;
		*slot = pgd;
		
		// Return the insert point (i.e. slot)
		return slot;
	}
	
	/**
	 * Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	 * the system will panic.
	 * @param pgd The page descriptor of the block to remove.
	 * @param order The order in which to remove the block from.
	 */
	void remove_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot) {
			slot = &(*slot)->next_free;
		}

		// Make sure the block actually exists.  Panic the system if it does not.
		assert(*slot == pgd);
		
		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = NULL;
	}
	
	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
	{
		// (1) Make sure 'order' is within range
		if (source_order >= MAX_ORDER) {
			return NULL;
		}
		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(*block_pointer, source_order)) {
			return NULL;
		}
		// Make sure there is an incoming pointer.
		assert(*block_pointer);
		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		
		// TODO: Implement this function
		// split the block into 2 half

		// This is variable to store the pointer to the buddy to the pointer.
		PageDescriptor *other_pointer;
		// The pointer to the buddy is the pointer to the block pointer 
		// plus a half size of the block.
		other_pointer = *block_pointer+(pages_per_block(source_order)/2);
		// store the block pointer into another pointer.
		PageDescriptor *ori_pointer = *block_pointer;
		// insert each half into the order below.
		// remove the block pointer from the source_order. 
		remove_block(ori_pointer,source_order);
		// insert the block pointer into the order below source order. 
		insert_block(ori_pointer,source_order-1);
		// insert the buddy pointer into the order below source order. 
		insert_block(other_pointer,source_order-1);		
		return *block_pointer;
	}
	/**
	 * Takes a block in the given source order, and merges it (and it's buddy) into the next order.
	 * This function assumes both the source block and the buddy block are in the free list for the
	 * source order.  If they aren't this function will panic the system.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new slot that points to the merged block.
	 */
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order)
	{
		assert(*block_pointer);
		
		if (source_order >= MAX_ORDER) {
			return NULL;
		}
		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		// TODO: Implement this function
		// calling the "buddy_of" function to obtain the pointer to the buddy pointer.
		PageDescriptor *buddy_pointer = buddy_of(*block_pointer,source_order);
		PageDescriptor *original_pointer = *block_pointer;
		PageDescriptor *first = *block_pointer;
		//PageDescriptor *sec = *buddy_pointer;
		PageDescriptor **return_pointer;
		// remove the pointer to a block from an order. 
		remove_block(original_pointer,(source_order));
		// remove the pointer to a buddy block from an order. 
		remove_block(buddy_pointer,source_order);
		// if the buddy is in front of the block
		if(buddy_pointer<original_pointer){
			return_pointer = insert_block(buddy_pointer,(source_order+1));
			return_pointer = &buddy_pointer;
		// if the buddy is behind the block
		}else{
			return_pointer = insert_block(first,(source_order+1));
		}
		return return_pointer;
	}

public:
	/**
	 * Constructs a new instance of the Buddy Page Allocator.
	 */
	BuddyPageAllocator() {
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			_free_areas[i] = NULL;
		}
	}
	
	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor *alloc_pages(int order) override
	{
		if (order >= MAX_ORDER) {
			return NULL;
		}
		// assign the value of order to another variable ord.
		int ord = order;
		// while there is no element in an order, increase the value
		// of ord to search another order
		// If ord's value is more than MAX_ORDER
		// return NULL as there is no enough free pages to be allocated.
		while(_free_areas[ord] ==NULL){
			ord++;
			if(ord>=MAX_ORDER){
				return NULL;
			}
		}
		// After this while loop finishes, the ord will be the lowest order possible
		// on which there is a free block.
		
		// slot is a pointer to a pointer
		PageDescriptor **slot; 
		// spilt the block until 
		// there is an available block on the order desired. 
		while(ord>order){	
			slot = &_free_areas[ord];
			split_block(slot,ord);
			ord--;	
		}

		// return the first block on the desired order.
		PageDescriptor *return_value = _free_areas[ord]; 
		// removed this block from this order.
		remove_block(_free_areas[ord],ord);
		return return_value;
	}

	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override
	{
		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		// mm_log.messagef(LogLevel::DEBUG, "free page started");
		assert(is_correct_alignment_for_order(pgd, order));

		if (order >= MAX_ORDER) {
			return;
		}
		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return;
		}

		int count = 0;	
		PageDescriptor *pgd_buddy = buddy_of(pgd,order);
		PageDescriptor **intermidate_block = insert_block(pgd,order);
		PageDescriptor *suspected_buddy = _free_areas[count];
		while(count<MAX_ORDER-1){
			// If there is no available block on this order.
			if(suspected_buddy == NULL){
				// increment count
				count++;
				// assign the first element of new order to be "suspected-buddy"
				suspected_buddy = _free_areas[count];
				// start the next iteration
				continue;
			}// If PGD buddy is found within this order.
			else if(suspected_buddy == pgd_buddy){
				// merge the buddy with the pgd given
				intermidate_block = merge_block(intermidate_block,count);
				// increment the variable count
				count++;
				// let suspect body be the first element from next order
				suspected_buddy = _free_areas[count];
				// check if the intermidate block(the new block) has any buddy.
				pgd_buddy = buddy_of( *intermidate_block ,count);	
			// if there are still available block on this order 
			// and the buddy of a buddy has not been found yet
			// let suspected_buddy be the next block on this order.
			}else{
				suspected_buddy = suspected_buddy->next_free;		
			}
	
		}	
	}
	
	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */
	bool reserve_page(PageDescriptor *pgd){
		assert(pgd);
		// initialise two page descriptors
		PageDescriptor *slot,*buddy;
		for(int current_order = 0;current_order<=MAX_ORDER;current_order++){
			// let slot be the first block of an order
			slot = _free_areas[current_order];
			// while slot is not null, if pgd is within this block, then ends the while loop.
			while (slot){				
					if((pgd >= slot) && (pgd <= slot+pages_per_block(current_order)))
						break;				
					slot = (slot)->next_free;
			}
			//if a slot is null, this means the the current order has no available blocks, 
			//start next iteration to search for next order.
			if(slot == NULL)
				continue;
			// If the block contains the slot is found, 
			// split the block until order 0 
			else{
				while(current_order != 0){	
					// store the address to a pointer to a slot into split_block_argument
					PageDescriptor** split_block_argument = &slot;	
					// split the block again		
					split_block(split_block_argument, current_order);	
					//move to the order below.	
					current_order--;
					// find the buddy
					buddy = buddy_of(slot, current_order);
					if(pgd>=buddy)
						slot = buddy;
				}				
				// change the type of pgd to reserved
				pgd->type = PageDescriptorType::RESERVED;
				// remove this pgd from order 0	
				remove_block(pgd,0);
			}
			return true;
		 }
		return false;
	}
	
	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
	{
		mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);
		// TODO: Initialise the free area linked list for the maximum order
		
		// to initialise the allocation algorithm.
		// this is the number of block in a order
		int number_of_block;
		// this is the amount of page descriptors left
		uint64_t remainder = nr_page_descriptors;
	
		// start allocating pages 
		for(int i = MAX_ORDER-1;i>=0;i--){
			// if the remainder is zero return 1, 
			// as all pages are successfully allocated
			if(remainder == 0){
				return 1;
			}
			// let the pointer to page_descriptor be the starting address of an order 
			_free_areas[i] = page_descriptors;
			// calculate the number of blocks in this page
			number_of_block = remainder / pages_per_block(i);
			// find the remainder, which is the page unallocated
			remainder = remainder - number_of_block * pages_per_block(i);

			// if the number of block on this order is zero, let 
			// _free_areas[i] be null and check the order below.
			if(number_of_block == 0){
				_free_areas[i] = NULL;
				continue;
			}
			// for all block on this order
			for(int j = 0; j < number_of_block-1; j++){	
					// let the next_free field of this page pointer 
					// to be the pointer of the next block			
					page_descriptors->next_free = page_descriptors + pages_per_block(i);
					// point to the next block
					page_descriptors = page_descriptors->next_free;		
			}
		}
		return 0;

	}
	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "buddy"; }
	
	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");
		
		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);
						
			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}
			
			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}

	
private:
	PageDescriptor *_free_areas[MAX_ORDER];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);