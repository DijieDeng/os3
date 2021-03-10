/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s1820742
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

#define MAX_ORDER	17


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
//        mm_log.messagef(LogLevel::DEBUG, "Now in the insert_block function pgd=%p, order=0x%lx", pgd, order);
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

//        mm_log.messagef(LogLevel::DEBUG, "INSERT: Finish insert_block(%p, %d)", pgd, order);
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
//        mm_log.messagef(LogLevel::DEBUG, "Now in split_block!");

        // Make sure there is an incoming pointer.
		assert(*block_pointer);
		
		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		// Make sure the source_order valid
        assert(source_order > 0 && source_order <= MAX_ORDER);

        // Initialise the resulted blocks
        auto son_left = *block_pointer;
        //auto son_right = buddy_of(son_left,source_order - 1);
        auto son_right = son_left + (pages_per_block(source_order)/2);
        // Remove the father block
        remove_block(*block_pointer,source_order);
        // Insert children blocks
        insert_block(son_left,source_order - 1);
        insert_block(son_right,source_order - 1);

        return son_left;
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
		
		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));

		// Make sure the source_order is valid
		assert(source_order>=0);

        // Initialise the child block
        auto child = *block_pointer;
        auto parent_left = *block_pointer;
        auto parent_right = buddy_of(*block_pointer,source_order);

        if (parent_left > parent_right) {
            auto temp = parent_left;
            parent_left = parent_right;
            parent_right = temp;
        }
		// delete the parent blocks
		remove_block(parent_left,source_order);
		remove_block(parent_right,source_order);

		// Insert the child block

		//return insert_block(child,source_order+1);
        auto ret = insert_block(child,source_order+1);
        if(parent_right<parent_left){
            //ret = insert_block(child,source_order+1);
            ret = &child;
        }
        //else{
            //ret = insert_block(child,source_order+1);
        //}

        return ret;
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
        mm_log.messagef(LogLevel::DEBUG, "alloc_pages pd=0x%lx", order);

        //Make sure the order is valid
        assert(order >= 0);
        assert(order <= MAX_ORDER);

        int current_order = order;
        // while there is no element in an order, increase the value
        // of ord to search another order
        // If ord's value is more than MAX_ORDER
        // return NULL as there is no enough free pages to be allocated.
        while(_free_areas[current_order]==NULL){
            if(++current_order>=MAX_ORDER){
                return NULL;
            }
            mm_log.messagef(LogLevel::DEBUG, "1.stops here !!!");

        }
        PageDescriptor **slot;
        // spilt the block to order
        while(current_order>order){
            slot = &_free_areas[current_order];
            split_block(slot,current_order);
            current_order--;
            mm_log.messagef(LogLevel::DEBUG, "2.stops here !!!");

        }
        // Remove the block from the free areas
        auto free_block =_free_areas[current_order];
        remove_block(free_block, current_order);
        return free_block;
    }

	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override
	{
	    mm_log.messagef(LogLevel::DEBUG, "Now in free_page function, pgd = %p, order=0x%lx", *pgd,order);

        // Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));

        // Ensure order is valid
        if (order >= MAX_ORDER || !is_correct_alignment_for_order(pgd, order)) {
            return;
        }

        // Search from the top
        int counter_order = 0;
        auto temp_block = insert_block(pgd, order);
        auto current_buddy = buddy_of(pgd, order);
        auto temp_buddy = _free_areas[counter_order];
        // If we have reached the max order, stop
        while (counter_order<MAX_ORDER-1) {
            // No available block in this order.
            if (!temp_buddy) {
                // larger space with higher order
                temp_buddy = _free_areas[++counter_order];
                continue;
            } else if (temp_buddy == current_buddy) {
                // Space reclamation
                temp_block = merge_block(&pgd, counter_order);
                // order increases and update buddy's order
                current_buddy = buddy_of(*temp_block, ++counter_order);
                // let suspect body be the first element from next order
                temp_buddy = _free_areas[counter_order];
            } else {
                temp_buddy = temp_buddy->next_free;
            }
        }
        return;

	}

	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */
    bool reserve_page(PageDescriptor *pgd){
//        mm_log.messagef(LogLevel::DEBUG, "reserve_page pd=%p", *pgd);

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
        int order = MAX_ORDER;
        uint64_t remaining_pages = nr_page_descriptors;

        do {
            if(remaining_pages == 0){
                return true;
            }
            mm_log.messagef(LogLevel::DEBUG, "Trap in the do while loop");
            _free_areas[order] = page_descriptors;
            auto block_size = pages_per_block(order);
            auto block_count = remaining_pages / block_size;
            if(block_count == 0){
                _free_areas[order] = NULL;
                continue;
            }
            while (block_count > 0) {
                // let the next_free field of this page pointer
                // to be the pointer of the next block
                page_descriptors->next_free = page_descriptors + block_size;
                // point to the next block
                page_descriptors = page_descriptors->next_free;
                remaining_pages -= block_size;
                block_count--;
                mm_log.messagef(LogLevel::DEBUG, "Block_Count=0x%lx", block_count);

            }
            order--;
            // if the remainder is zero return 1,
            // as all pages are successfully allocated
        } while (remaining_pages > 0);
        mm_log.messagef(LogLevel::DEBUG, "Done Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);
        return true;
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
