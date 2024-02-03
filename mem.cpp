#include <iostream>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
// #include <cstdlib>



/* essentialy we would start by allocating the minimum required memory based on a request, considering a page is 4096 bytes so 5000 bytes would actually result in 8192 bytes allocated 
   If our page is empty after a release a block from, indicated by the page header being equal to the next block header - sizeof(page header)
   we release it back with munmap

   to keep track of the different pages allocated, each heap page's last 8 bytes will be a pointer to the next heap page 
   
   and just like with blocks assigned, we'll have a start header and end header with the size of the page, 
   as well as padding that is equal to 0, 
   so when the first block checks what's the size of the block before it
        or the last block checks what's the size of the block after it
   it gets 0 signaling it's the end of the page, (followed by the size of the page, and a pointer to the next page )

   heap page = 
   {
    /////////pointer to last page: 8 bytes;
    size of page: heap_word_size bytes;
    padding: heap_word_size bytes (all 0);
    actual heap: (size of page) - (4 * (heap_word_size))
    padding: heap_word_size bytes (all 0);
    size of page: heap_word_size bytes;
    pointer to next page: 8 bytes;
    }
        
    so when a request for memory is entered, we actually need to add to it ((6 * heap_word_size) + 16) bytes
    4 heap_word_size: for the page header and padding
    2 heap_word_size: for the first heap block headers (after which the heap could be full...)
    16 bytes: for 2 pointers to to the next and last pages
*/


/* structure of my heap
    
    {size of block: heap_word_size bytes; 
     actual data: requested size;
     size of block: heap_word_size bytes; 
     }
     so when a request for memory is entered, 





1) allocate (mmap) the needed amount of pages (4096 bytes each) for the requested memory (if none available yet)
    1.1) intialize the memory, the very first X (4 for 4gb, 5 for 1tb, 6 for 281tb... this is just a coding practice, 4 is enough.) bytes represent the size of the block  (this means the implementation is limited to max block size of 2**(8*x))
            - because the last bit is always 0 (becuase it's divsible by 2) the last bit of the "header" represents weather it's allocated or not
            also change the value for the last X bytes to be the same, this is a not perfect way to make sure that the ptr we get for `free` is valid
2) assign the requested memory, so {,  }

if a block is completley empty, we set it free. 
if 

*/
/*

padding: heap_word_size bytes (all 0);
size of page: heap_word_size bytes;
pointer to next page: 8 bytes;
*/




// struct heap_block{
//     unsigned int block_size = 0;

//     char data[] ;
// };

// struct heap_page{
//     heap_page * last_page = nullptr; // pointer to last page: 8 bytes;
//     unsigned int page_size = 0;// size of page: heap_word_size bytes;
//     unsigned int heap_start_padding = 0; // padding: heap_word_size bytes (all 0);
//     heap_block heap; // the actual heap 
//     unsigned int heap_end_padding = 0; // padding: heap_word_size bytes (all 0);
//     unsigned int page_end_size_end = 0;// size of page: heap_word_size bytes;
//     heap_page * next_page = nullptr;// pointer to next page
// };



void *gHeap = (void *) nullptr;


unsigned int calc_needed_page_size(unsigned int size){
    unsigned int page_header_size = 6 * sizeof(unsigned int) + sizeof (void *);
    unsigned int base_page_size = 4096;  // 4kb
    unsigned interest = ((size + page_header_size) % base_page_size) ? 1 : 0;
    return (((size + page_header_size) / base_page_size) + interest ) * base_page_size;
}


unsigned int get_block_size(void *ptr){
    unsigned int size = ((unsigned int *)ptr)[0];
    size = size & ~1;
    return size;
}


void *get_next_block(void *ptr){
    unsigned int block_size = get_block_size(ptr);
    char *cp = (char *)ptr;
    return cp + (2 * sizeof(unsigned int)) + block_size;
}

void *get_next_page(void *ptr){
    unsigned int block_size = get_block_size(ptr);
    char *cp = (char *)ptr;
    return (void *)(((int **)(cp + block_size - 8))[0]);
}

void print_block_as_uint(void *ptr){
    unsigned int *block_start = (unsigned int *)ptr ;
    unsigned int block_size = get_block_size(block_start);
    std::cout << std::endl;
    for (int i = 0; i < block_size/sizeof(unsigned int); ++i) {
        std::cout << block_start[i] << " ";
    }
    std::cout << std::endl;
}


void mark_block_allocated(void *ptr){
    unsigned int *block_start = (unsigned int *)ptr;
    unsigned int block_size = get_block_size(ptr);
    unsigned int *block_end = (unsigned int *)((char *)(block_start + 1) + block_size);
    block_start[0] = block_size + 1;
    block_end[0] = block_size + 1;
}


void mark_block_free(void *ptr){
    unsigned int *block_start = (unsigned int *)ptr;
    unsigned int block_size = get_block_size(ptr);
    unsigned int *block_end = (unsigned int *)((char *)(block_start + 1) + block_size);
    block_start[0] = block_size;
    block_end[0] = block_size;
}


bool check_block_allocated(void *ptr){
    unsigned int block_marker = ((unsigned int *)ptr)[0];
    bool allocated = block_marker & 1;
    return allocated ;
}


void setup_block(void *ptr, unsigned int size){
    unsigned int *block_start = (unsigned int *)ptr;
    unsigned int *block_end = (unsigned int *)((char *)(block_start + 1) + size);
    block_start[0] = size;
    block_end[0] = size;
}

void init_heap_page(void *ptr, unsigned int page_size){
    unsigned int page_header_size = 6 * sizeof(unsigned int) + sizeof (void *);

    // Create two void pointers
    int **glob_heap = (int **) &gHeap;
    
    unsigned int block_size = page_size-page_header_size;

    unsigned int *page_start = (unsigned int *)ptr ;
    unsigned int *block_end = page_start + 3 + (block_size/sizeof(unsigned int));
    int **heap_page_end = (int **) &block_end[3];

    page_start[0] = page_size;
    page_start[1] = 0;
    setup_block(&page_start[2], block_size);
    block_end[1] = 0;
    block_end[2] = page_size - sizeof(void *); // actually 8 bytes smaller because of the pointer at the end
    heap_page_end[0] =  (int *) gHeap;

    //reassign the first heap page to be the new heap page
    glob_heap[0] = (int *) ptr;
    
}



void * partition_heap_block(void *ptr, unsigned int partition_size){
    /* 
    a block has to have at least sizeof(unsigned int) bytes, and must be a multiple of sizeof(unsigned int);

    if there isn't enought room to partition the block in a way that the other partition is at least 8 bytes, 
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
    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        std::cout << "mmap failed" << std::endl;
        return (void *)-1;
    }

    return memory;
}


void page_free(void* ptr){
    unsigned int page_size;
    if (munmap(ptr, 4096) == -1) {
        std::cout << "munmap failed" << std::endl;
    }
}

void *find_best_fit_block_in_page(void *page_ptr, unsigned int size){
    // iterate over a page looking for the best block to allocate, would return a pointer - 
    void *best_fit_block = nullptr;
    unsigned int best_fit_block_size_difference = -1; // highest int because it's unsigned
    bool reached_end_of_page = false;

    void *block = (void *)(&((unsigned int *)page_ptr)[2]);
    unsigned int block_size;

    while (!reached_end_of_page){
        block_size = get_block_size(block);
        if (block_size == 0){
            reached_end_of_page = true;
        }
        else if ((block_size == size) 
                 && (!check_block_allocated(block))) {
                return (void *)block;
        }
        else if ((!check_block_allocated(block))
                 && (block_size > size) 
                 && (best_fit_block_size_difference > block_size - size)){
            best_fit_block = block;
            best_fit_block_size_difference = block_size - size;
        }

        //advance the block
        block = get_next_block(block);
    }
    
    if (best_fit_block==nullptr){
        return (void *) -1;
    }
    
    return best_fit_block;
}

void *find_best_fit_block(unsigned int size){
    // iterate over all pages looking for the best block to allocate
    void *best_fit_block = (void *)-1;
    void *page = gHeap;

    while (page!=nullptr){
        best_fit_block = find_best_fit_block_in_page(page, size);
        if (best_fit_block!=(void *)-1){
            return best_fit_block;
        }
        page = get_next_page(page);
    }

    return best_fit_block;
}


void *halloc(unsigned int size){
    void *block = find_best_fit_block(size);
    if (block == (void *)-1) {
        //allocate new page
        unsigned int page_size = calc_needed_page_size(size) ;
        std::cout << "requesting a new heap page of the size: " << page_size << " bytes"  << std::endl;
        
        void* new_heap = page_allocate(page_size);
        std::cout << "got address: " << new_heap  << std::endl;
        init_heap_page(new_heap, page_size);
        
        block = find_best_fit_block_in_page(new_heap, size);
        partition_heap_block(block, size);
        mark_block_allocated(block);

        return block;
    }
    partition_heap_block(block, size);
    mark_block_allocated(block);
    return block;
}


int main() {
    
    // Size of the memory region to map (in bytes)
    unsigned int size = 4048 ; 
    


    void* pt = halloc(40);
    std::cout << "address of memblock: " << pt << std::endl;
    print_block_as_uint(gHeap);

    void* pt1 = halloc(900);
    std::cout << "address of memblock: " << pt1 << std::endl;
    print_block_as_uint(gHeap);



    void* pt2 = halloc(3888);
    std::cout << "address of memblock: " << pt2 << std::endl;
    print_block_as_uint(gHeap);


    std::cout << "address of glob_heap: " << gHeap << std::endl;



    // // Create two void pointers
    // int **glob_heap = (int **) &gHeap;
    // std::cout << "address of glob_heap: " << &gHeap << std::endl;
    // glob_heap[0] = (int *) pt;
    // std::cout << "address of glob_heap: " << gHeap << std::endl;

    // char *page_pointer_char = (char *) pt; 
    // int **int_pointer = (int **) pt;
    // unsigned int *uip = (unsigned int *)pt + 2;
    // unsigned int *uip_end = (unsigned int *)(page_pointer_char +  size);
    // int **heap_page_end = (int **) (uip_end + 3);

    // int_pointer[0] = (int *)pt;
    // uip[0] = calc_needed_page_size(size);
    // uip[1] = 0;
    // uip[2] = size; 
    // uip_end[0] = size;
    // uip_end[1] = 0;
    // uip_end[2] = calc_needed_page_size(size);
    // heap_page_end[0] =  (int *) gHeap;


    // // cp[6] = 42;
    // // Print the contents of the memory
    // for (int i = 0; i < 64; ++i) {
    //     std::cout << int_pointer[i] << " ";
    // }
    // std::cout << std::endl;
    // std::cout << std::endl;
    // for (int i = 0; i < 64; ++i) {
    //     std::cout << uip[i] << " ";
    // }
    // std::cout << std::endl;
    // std::cout << std::endl;
    // for (int i = 0; i < 5; ++i) {
    //     std::cout << uip_end[i] << " ";
    // }

    // std::cout << std::endl;
    // std::cout << heap_page_end[0];
    unsigned int t = 12+ 1;
    t = t & 1;
    std::cout << t << std::endl;
    std::cout << std::endl;
    return 0;
}
