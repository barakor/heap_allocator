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

    return (((size + page_header_size) / base_page_size) + 1 ) * base_page_size;
}




void init_heap_page(void *ptr, unsigned int page_size){
    unsigned int page_header_size = 6 * sizeof(unsigned int) + sizeof (void *);
    // Create two void pointers
    int **glob_heap = (int **) &gHeap;
    
    unsigned int block_size = page_size-page_header_size;
    unsigned int block_size_bytes = block_size/sizeof(unsigned int);

    std::cout << "address of heap page: " << ptr << std::endl;
    std::cout << "size of heap page header: " << page_header_size << std::endl;
    char *page_pointer_char = (char *) ptr; 
    unsigned int *uip = (unsigned int *)ptr ;
    unsigned int *uip_end = (uip + 3 + block_size_bytes);
    int **heap_page_end = (int **) (uip_end + 3);

    uip[0] = page_size;
    uip[1] = 0;
    uip[2] = block_size; 
    uip_end[0] = block_size;
    uip_end[1] = 0;
    uip_end[2] = page_size;
    heap_page_end[0] =  (int *) gHeap;
    //reassign the first heap page to be the new heap page
    std::cout << "address of glob_heap: " << gHeap << std::endl;
    glob_heap[0] = (int *) ptr;
    std::cout << "address of glob_heap: " << gHeap << std::endl;

    // Print the contents of the memory

    std::cout << std::endl;
    for (int i = 0; i < page_size/sizeof(unsigned int); ++i) {
        std::cout << uip[i] << " ";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::cout << uip_end[i] << " ";
    }

    std::cout << std::endl;
    std::cout << heap_page_end[0];
    std::cout << std::endl;
    
}


void * partition_heap_block(void *ptr, unsigned int partition_size){
    char *cp = (char *)ptr;
    unsigned int *uip = (unsigned int *)ptr;
    unsigned int block_size = uip[0];

    // char* p = cp+ 5;

    /* 
    if there isn't enought room to partition the block, we will give all of it - 
        it's better to leave a few bytes (up to 7) unused than failing
        this will happend if the block size is 12 and the user asked for 10, for example
    */


    // if (block_size == partition_size){
    //     return &uip[1];
    // }
    // if (block_size < partition_size+(2*(sizeof(unsigned int)))){
    //     return (void *)-1;
    // }






    return (void *)&uip[1];
}


void *heap_allocate(unsigned int size){
    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        std::cout << "mmap failed" << std::endl;
        return (void *)-1;
    }

    return memory;
}


void heap_free(void* ptr){
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

    char *block = (char *)(&((unsigned int *)page_ptr)[2]);
    std::cout << "started looking for a block from address: "<<(void *)block << std::endl;
    unsigned int block_size;

    while (!reached_end_of_page){
        block_size = ((unsigned int *)block)[0];
        std::cout << "Current block address: " << (void*)block << std::endl ; 
        std::cout << "Current block size: " << block_size << std::endl ; 
        if (block_size == 0){
            reached_end_of_page = true;
        }
        else if (block_size == size) {
                return (void *)block;
        }
        else if ((block_size > size) && (block_size - size <  best_fit_block_size_difference)){
            best_fit_block = block;
            best_fit_block_size_difference = block_size - size;
        }

        //advance the block
        block = block + block_size;
    }  
    std::cout << "best fit block address: " << (void*)best_fit_block << std::endl ; 
    std::cout << "best fit block size diff: " << best_fit_block_size_difference << std::endl ; 
    partition_heap_block(best_fit_block, size);
    return best_fit_block;
}

void *find_best_fit_block(unsigned int size){
    // iterate over all pages looking for the best block to allocate
    void *best_fit_block = (void *)-1;




    return best_fit_block;
}


void *halloc(unsigned int size){
    void *memory = find_best_fit_block(size);
    if (memory == (void *)-1) {
        //allocate new page
        unsigned int page_size = calc_needed_page_size(size) ;
        std::cout << "requesting a new heap page of the size: " << page_size << " bytes"  << std::endl;
        
        void* new_heap = heap_allocate(page_size);
        std::cout << "got address: " << new_heap  << std::endl;
        init_heap_page(new_heap, page_size);

        void* block = find_best_fit_block_in_page(new_heap, size);
        
        

        return new_heap; // delete me

        // return block;
    }
    return memory;
}


int main() {
    
    // Size of the memory region to map (in bytes)
    unsigned int size = 2878 ; 
    

    std::cout << "requested page size: " << size << std::endl;
    std::cout << "needed page size: " << calc_needed_page_size(size) << std::endl;

    std::cout << "sizeof int: " << sizeof(int) << std::endl;
    std::cout << "sizeof unsigned int: " << sizeof(unsigned int) << std::endl;
    std::cout << "sizeof size_t: " << sizeof(size_t) << std::endl;


    void* pt = halloc(size);
    std::cout << "address of memblock: " << pt << std::endl;
    
    




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
    
    std::cout << std::endl;
    return 0;
}
