#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
// #include <cstdlib>



/* essentialy we would start by allocating the minimum required memory based on a request, considering a page is 4096 bytes so 5000 bytes would actually result in 8192 bytes allocated 
   If our page is empty after a release a block from, 
   we release it back with munmap

   to keep track of the different pages allocated, each heap page's last 8 bytes will be a pointer to the next heap page 
   
   and just like with blocks assigned, we'll have a start header and end header with the size of the page, 
   as well as padding that is equal to 0, 
   so when the first block checks what's the size of the block before it
        or the last block checks what's the size of the block after it
   it gets 0 signaling it's the end of the page, (followed by the size of the page, and a pointer to the next page )
   

   block structure = 
   {
    size of block: word_size bytes;
    data: size_of_block bytes;
    size of block: word_size bytes;
   }

   word_size = sizeof(unsigned int)
   heap page = 
   {
    pointer to previous page: pointer_size bytes;                    // only used for relinking pages when a page is freed
    size of page: word_size bytes;                                   // used to get the size of the page 
    padding: word_size bytes (all 0);                                // used to detect the end of the heap block segment
    actual heap: (size of page) - (6*(word_size) + 2*(pointer_size)  // the actual area allocated when requested
    padding: word_size bytes (all 0);                                // used to detect the end of the heap block segment
    size of page: word_size bytes;                                   // kept for symmetrical purposes
    pointer to next page: pointer_size bytes;                        // used to get to the next (older) page
    }


    the size of the page at the end of the page isn't actually used, it costs 4 bytes (when word_size=4), which is at most 0.1% of the page size, so I decided to keep it for symmetrical purposes purposes. 
*/

// the pointer to the start of our heap
void *gHeap = (void *) -1;


unsigned int calc_needed_page_size(unsigned int size){
    // given a size in bytes, calculate how many bytes are needed to be asked from the OS to allocate a block
    unsigned int page_header_size = 6 * sizeof(unsigned int) + 2 * sizeof (void *);
    unsigned int base_page_size = 4096;  // 4kb
    unsigned interest = ((size + page_header_size) % base_page_size) ? 1 : 0;
    return (((size + page_header_size) / base_page_size) + interest ) * base_page_size;
}


unsigned int get_page_size(void *ptr){
    // returns the size of the page in bytes
    return ((unsigned int *)ptr)[2];
}


unsigned int get_block_size(void *ptr){
    // returns the size of the block in bytes, ignoring the allocation bit marker
    unsigned int size = ((unsigned int *)ptr)[0];
    size = size & ~1;
    return size;
}


unsigned int get_prev_block_size(void *ptr){
    // returns the size of the previous block in bytes, ignoring the allocation bit marker
    unsigned int *prev_block_end = ((unsigned int *)ptr) -1;
    return get_block_size(prev_block_end);
}


void *get_next_block(void *ptr){
    // returns a pointer to the next block
    unsigned int block_size = get_block_size(ptr);
    char *cp = (char *)ptr;
    return cp + (2 * sizeof(unsigned int)) + block_size;
}


void *get_prev_block(void *ptr){
    // returns a pointer to the previous block
    unsigned int *prev_block_end = ((unsigned int *)ptr) -1;
    unsigned int block_size = get_block_size(prev_block_end);
    char *cp = (char *)prev_block_end;
    return cp - (block_size  + sizeof(unsigned int));
}


void *get_first_block_in_page(void *ptr){
    // get the first block in allocation segment of a page
    return (((char *)ptr )+(2 * sizeof(unsigned int)) + (sizeof(unsigned int *)));
}


void *get_next_page(void *ptr){
    // returns a pointer to the next page
    unsigned int block_size = get_page_size(ptr);
    char *cp = (char *)ptr;
    return (void *)(((int **)(cp + block_size - 8))[0]);
}


void *get_prev_page(void *ptr){
    // returns a pointer to the previous page
    return (void *)(((int **)ptr)[0]);
}


void *get_page_ptr(void *block){
    // given a pointer to a block in a page, return the pointer to start of the page
    unsigned int block_size;
    while(true){
        if (get_prev_block_size(block) == 0){
            return (unsigned int *)block - 4;
        }
        block = get_prev_block(block);
    }
}


void set_next_page_ptr(void *page_ptr, void *next_page_ptr){
    // set the pointer to the next page in the page
    unsigned int page_size = get_page_size(page_ptr);
    int **page_end = (int **) (((char *)page_ptr) + page_size - sizeof(int *));
    page_end[0] =  (int *) next_page_ptr;
}


void set_prev_page_ptr(void *page_ptr, void *prev_page_ptr){
    // set the pointer to the prev page in the page
    int **page_start = (int **) page_ptr;
    page_start[0] = (int *)prev_page_ptr;
}


void print_page_as_uint(void *ptr){
    // prints the page as if it was an array of unsigned int
    unsigned int *block_start = (unsigned int *)ptr ;
    unsigned int block_size = get_page_size(block_start);
    for (int i = 0; i < block_size/sizeof(unsigned int); ++i) {
        std::cout << block_start[i] << " ";
    }
    std::cout << std::endl;
}


void print_block_as_uint(void *ptr){
    // prints the block as if it was an array of unsigned int
    unsigned int *block_start = (unsigned int *)ptr ;
    unsigned int block_size = get_block_size(block_start);
    for (int i = 0; i < block_size/sizeof(unsigned int); ++i) {
        std::cout << block_start[i] << " ";
    }
    std::cout << std::endl;
}


void print_all_pages_as_uint(){
    // iterate over all pages and print them as if they were an array of unsigned int
    std::cout << std::endl;
    std::cout << "printing all pages:" << std::endl;
    void *page = gHeap;
    while (page!=(void *)-1){
        std::cout << "page: "<< page << std::endl;
        print_page_as_uint(page);
        std::cout << std::endl;
        page = get_next_page(page);
    }
}


void mark_block_allocated(void *ptr){
    // mark a block as allocated by flipping the rightmost bit of the size marker to 1
    unsigned int *block_start = (unsigned int *)ptr;
    unsigned int block_size = get_block_size(ptr);
    unsigned int *block_end = (unsigned int *)((char *)(block_start + 1) + block_size);
    block_start[0] = block_size + 1;
    block_end[0] = block_size + 1;
}


void mark_block_free(void *ptr){
    // mark a block as free by flipping the rightmost bit of the size marker to 0
    unsigned int *block_start = (unsigned int *)ptr;
    unsigned int block_size = get_block_size(ptr);
    unsigned int *block_end = (unsigned int *)((char *)(block_start + 1) + block_size);
    block_start[0] = block_size;
    block_end[0] = block_size;
}


bool check_block_allocated(void *ptr){
    // check if a block is allocated by checking the rightmost bit of the size marker 0->free, 1->allocated 
    unsigned int block_marker = ((unsigned int *)ptr)[0];
    bool allocated = block_marker & 1;
    return allocated ;
}


bool check_page_allocated(void *page_ptr){
    // checks weather a page has any allocated blocks
    void *block = get_first_block_in_page(page_ptr);
    unsigned int block_size;
    bool reached_end_of_page = false;
    while (!reached_end_of_page){
        if (check_block_allocated(block)){
            return true;
        }

        //advance the block
        block = get_next_block(block);
        block_size = get_block_size(block);
        if (block_size == 0){
            reached_end_of_page = true;
        }
    }
    return false;
}


void setup_block(void *ptr, unsigned int size){
    // set the size markers of a block
    unsigned int *block_start = (unsigned int *)ptr;
    unsigned int *block_end = (unsigned int *)((char *)(block_start + 1) + size);
    block_start[0] = size;
    block_end[0] = size;
}


void coalesce_block_leftward(void *ptr){
    // merge block with the block previous to it, if there is one
    unsigned int prev_block_size = get_prev_block_size(ptr);
    unsigned int block_size = get_block_size(ptr);
    if (prev_block_size!=0){
        setup_block(get_prev_block(ptr), prev_block_size+block_size);
    }
}


void coalesce_block_rightward(void *ptr){
    // merge block with the block next to it, if there is one
    unsigned int block_size = get_block_size(ptr);
    unsigned int next_block_size = get_block_size(get_next_block(ptr));
    if (next_block_size!=0){
        setup_block(ptr, block_size+next_block_size);
    }
}


void coalesce_block(void *ptr){
    // checks if the blocks on either sides of the block are free, if they are merge them; 
    void *prv_block = get_prev_block(ptr);
    void *next_block = get_next_block(ptr);

    if (!check_block_allocated(get_next_block(ptr))){
        coalesce_block_rightward(ptr);
    }
    if (!check_block_allocated(get_prev_block(ptr))){
        coalesce_block_leftward(ptr);
    }
}


void init_heap_page(void *ptr, unsigned int page_size){
    // sets the header values needed for the page bookkeeping, as well as inserting it to the top of the heap's linked list
    unsigned int page_header_size = (6 * sizeof(unsigned int)) + (2 * sizeof (void *));

    // Create two void pointers    
    unsigned int block_size = page_size-page_header_size;
    int **glob_heap = (int **) &gHeap;
    int **heap_page_start = (int **) ptr;
    unsigned int *page_start = (unsigned int *)&heap_page_start[1] ;
    unsigned int *block_end = page_start + 3 + (block_size/sizeof(unsigned int));

    set_prev_page_ptr(ptr, (void *)-1);
    page_start[0] = page_size;
    page_start[1] = 0;
    setup_block(&page_start[2], block_size);
    block_end[1] = 0;
    block_end[2] = page_size;
    set_next_page_ptr(ptr, gHeap);

    if (gHeap!= (void *)-1){
        ((int **)gHeap)[0] =(int *) ptr;
    }
    //reassign the first heap page to be the new heap page
    // gHeap = ptr;
    gHeap = ptr;
    
}



void * partition_heap_block(void *ptr, unsigned int partition_size){
    // partitions a block into 2 when possible
    /* 
    a block has to have at least sizeof(unsigned int) bytes, and must be a multiple of sizeof(unsigned int);

    if there isn't enought room to partition the block in a way that the other partition is at least 2*sizeof(unsigned int) bytes,
    we will give all of it -
        it's better to leave a few bytes unused than failing
        this will happen if the block size is 12 and the user asked for 10, for example
    */

    unsigned int block_header_size = 2*(sizeof(unsigned int));

    char *cp = (char *)ptr;
    unsigned int *left_block_starter = (unsigned int *)ptr;
    unsigned int block_size = get_block_size(left_block_starter);


    if (block_size < partition_size){
        return (void *)-1;
    }
    if ((block_size == partition_size) || (block_size < partition_size+(2*block_header_size))){
        return &left_block_starter[1];
    }
    
    unsigned int interest = (partition_size % sizeof(unsigned int)) ? 1 : 0;
    unsigned int left_block_size = (partition_size/sizeof(unsigned int) + interest ) * sizeof(unsigned int);

    setup_block(ptr, left_block_size);

    unsigned int * right_block_start = (unsigned int *) (cp + sizeof(unsigned int) + left_block_size) + 1;
    unsigned int right_block_size = block_size - (left_block_size + block_header_size);
    setup_block(right_block_start, right_block_size);


    return &left_block_starter[1];
}


void *page_allocate(unsigned int size){
    // asks the OS for memory, used to get a new page
    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        std::cout << "mmap failed" << std::endl;
        return (void *)-1;
    }

    return memory;
}


void page_free(void* ptr){
    // mark the page as free for the OS. 
    unsigned int page_size = get_page_size(ptr);
    if (munmap(ptr, page_size) == -1) {
        std::cout << "munmap failed" << std::endl;
    }
}


void *find_best_fit_block_in_page(void *page_ptr, unsigned int size){
    // iterate over a page looking for the best block to allocate, would return a pointer to the best fitting block if one is found, otherwise a -1 pointer signaling an error
    void *best_fit_block = (void *)-1;
    unsigned int best_fit_block_size_difference = -1; // highest int because it's unsigned
    bool reached_end_of_page = false;

    void *block = get_first_block_in_page(page_ptr);
    unsigned int block_size = get_block_size(block);

    while (!reached_end_of_page){
        if ((block_size == size) 
                 && (!check_block_allocated(block))) {
                return (void *)block;
        }
        if ((!check_block_allocated(block))
                 && (block_size > size) 
                 && (best_fit_block_size_difference > block_size - size)){
            best_fit_block = block;
            best_fit_block_size_difference = block_size - size;
        }

        //advance the block
        block = get_next_block(block);
        block_size = get_block_size(block);
        if (block_size == 0){
            reached_end_of_page = true;
        }
    }
    
    if (best_fit_block==(void *)-1){
        return (void *) -1;
    }
    
    return best_fit_block;
}


void *find_best_fit_block(unsigned int size){
    // iterate over all pages looking for the best block to allocate
    void *block;
    void *best_fit_block = (void *)-1;
    unsigned int block_size;
    unsigned int best_fit_block_size_difference = -1; // highest int because it's unsigned
    void *page = gHeap;

    while (page!=(void *)-1){
        block = find_best_fit_block_in_page(page, size);
        if (block!=(void *)-1){
            block_size = get_block_size(block);
            if ((best_fit_block_size_difference > block_size - size)){
                best_fit_block = block;
                best_fit_block_size_difference = block_size - size;
            }
            if (best_fit_block_size_difference==0){
                return best_fit_block;
            }
        }
        page = get_next_page(page);
    }

    return best_fit_block;
}


void *find_first_fit_block_in_page(void *page_ptr, unsigned int size){
    // iterate over a page looking for the first block that is big enough, returns -1 pointer if not found
    void *fit_block = (void *)-1;
    bool reached_end_of_page = false;

    void *block = get_first_block_in_page(page_ptr);
    unsigned int block_size = get_block_size(block);

    while (!reached_end_of_page){
        if ((block_size >= size) 
         && (!check_block_allocated(block))) {
            return (void *)block;
        }
        //advance the block
        block = get_next_block(block);
        block_size = get_block_size(block);
        if (block_size == 0){
            reached_end_of_page = true;
        }
    }
    
    return (void *) -1;
}


void *find_first_fit_block(unsigned int size){
    // iterate over all pages looking for the first block suitable for allocation
    void *best_fit_block = (void *)-1;
    void *page = gHeap;

    while (page!=(void *)-1){
        best_fit_block = find_first_fit_block_in_page(page, size);
        if (best_fit_block!=(void *)-1){
            return best_fit_block;
        }
        page = get_next_page(page);
    }

    return best_fit_block;
}


void *halloc(unsigned int size){
    // search for a (best) fitting block in existing pages, if one isn't found, request a new page for it 
    void *block = find_best_fit_block(size);
    if (block == (void *)-1) {
        //allocate new page
        unsigned int page_size = calc_needed_page_size(size) ;
        std::cout << "requesting a new heap page of the size: " << page_size << " bytes"  << std::endl;
        
        void* new_heap = page_allocate(page_size);
        std::cout << "got address: " << new_heap  << std::endl;
        init_heap_page(new_heap, page_size);
        
        block = find_first_fit_block_in_page(new_heap, size);
    }
    partition_heap_block(block, size);
    mark_block_allocated(block);
    return block;
}


void hfree(void *ptr){
    // free a block, if it's the only allocated block in a page, free the page and relink the previous and next pages
    mark_block_free(ptr);
    void *page_ptr = get_page_ptr(ptr);
    
    if (!check_page_allocated(page_ptr)){
        // need to make sure linked pages are handled, maybe need to change gHeap...
        std::cout << "freeing page: " << page_ptr << std::endl;
        void *next_page_ptr = get_next_page(page_ptr);
        void *prev_page_ptr = get_prev_page(page_ptr);
        if (next_page_ptr!=(void *)-1){
            set_prev_page_ptr(next_page_ptr, prev_page_ptr);
        }
        if (prev_page_ptr!=(void *)-1){
            set_next_page_ptr(prev_page_ptr, next_page_ptr);
        }
        if (gHeap==page_ptr){
            std::cout << "replacing first page: " << gHeap << " with: " << next_page_ptr << std::endl;
            gHeap = next_page_ptr; 
        }
        page_free(page_ptr);
    }
    else{
        coalesce_block(ptr);
    }
}



int main() {
    
    // Size of the memory region to map (in bytes)
    unsigned int size = 4048 ; 
    

    print_all_pages_as_uint();
    void* pt = halloc(40);
    std::cout << "address of memblock: " << pt << std::endl;
    std::cout << "address of gHeap: " << gHeap << std::endl;
    // print_page_as_uint(gHeap);
    print_all_pages_as_uint();

    void* pt1 = halloc(900);
    std::cout << "address of memblock: " << pt1 << std::endl;
    std::cout << "address of gHeap: " << gHeap << std::endl;
    // print_page_as_uint(gHeap);
    print_all_pages_as_uint();


    void* pt2 = halloc(3888);
    std::cout << "address of memblock: " << pt2 << std::endl;
    std::cout << "address of gHeap: " << gHeap << std::endl;
    // print_page_as_uint(gHeap);
    print_all_pages_as_uint();

    hfree(pt2);
    std::cout << "address of glob_heap: " << gHeap << std::endl;
    print_all_pages_as_uint();

    unsigned int t = 12+ 1;
    t = t & 1;
    std::cout << t << std::endl;
    std::cout << std::endl;
    return 0;
}
