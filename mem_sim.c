/***************************************************************************
 * *    Inf2C-CS Coursework 2: Cache Simulation
 * *
 * *    Instructor: Boris Grot
 * *
 * *    TA: Siavash Katebzadeh
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <time.h>
/* Do not add any more header files */

/*
 * Various structures
 */
typedef enum {FIFO, LRU, Random} replacement_p;

const char* get_replacement_policy(uint32_t p) {
    switch(p) {
        case FIFO: return "FIFO";
        case LRU: return "LRU";
        case Random: return "Random";
        default: assert(0); return "";
    };
    return "";
}

typedef struct {
    uint32_t address;
} mem_access_t;

// These are statistics for the cache and should be maintained by you.
typedef struct {
    uint32_t cache_hits;
    uint32_t cache_misses;
} result_t;


/*
 * Parameters for the cache that will be populated by the provided code skeleton.
 */

replacement_p replacement_policy = FIFO;
uint32_t associativity = 0;
uint32_t number_of_cache_blocks = 0;
uint32_t cache_block_size = 0;


/*
 * Each of the variables below must be populated by you.
 */
uint32_t g_num_cache_tag_bits = 0;
uint32_t g_cache_offset_bits= 0;
uint32_t number_of_sets= 0;
uint32_t number_of_index_bits= 0;
result_t g_result;


/* Reads a memory access from the trace file and returns
 * 32-bit physical memory address
 */
mem_access_t read_transaction(FILE *ptr_file) {
    char buf[1002];
    char* token = NULL;
    char* string = buf;
    mem_access_t access;
    
    if (fgets(buf, 1000, ptr_file)!= NULL) {
        /* Get the address */
        token = strsep(&string, " \n");
        access.address = (uint32_t)strtol(token, NULL, 16);
        return access;
    }
    
    /* If there are no more entries in the file return an address 0 */
    access.address = 0;
    return access;
}

void print_statistics(uint32_t num_cache_tag_bits, uint32_t cache_offset_bits, result_t* r) {
    /* Do Not Modify This Function */
    
    uint32_t cache_total_hits = r->cache_hits;
    uint32_t cache_total_misses = r->cache_misses;
    printf("CacheTagBits:%u\n", num_cache_tag_bits);
    printf("CacheOffsetBits:%u\n", cache_offset_bits);
    printf("Cache:hits:%u\n", r->cache_hits);
    printf("Cache:misses:%u\n", r->cache_misses);
    printf("Cache:hit-rate:%2.1f%%\n", cache_total_hits / (float)(cache_total_hits + cache_total_misses) * 100.0);
}

// Created a new block structure to hold valid bits, the access number and the memory address
typedef struct {
    uint32_t address;
    int valid_bit;
    int access_num;
} cache_block_t;


cache_block_t **cache;          // initialize a cache which is a 2D array of blocks

//Setup initializes every cache block attribute with 0s for all cache blocks.
void setup(cache_block_t **cache, int associativity){
    for(int i = 0; i < number_of_sets; i++){
        for(int j = 0; j < associativity; j ++){
            cache[i][j].access_num = 0;
            cache[i][j].valid_bit = 0;
            cache[i][j].address = 0;
        }
    }
}


int return_offset(uint32_t address){
    int remove_offset = cache_block_size - g_cache_offset_bits;
//    +------------------------------------------------------------+
//    | tag (tag bits) | index (index bits) | offset (offset bits) |
//    +------------------------------------------------------------+
//    to get offset, shift left (index + tag) bits and then shift right the same amount to put it back at lsb position.
    
    return (address << remove_offset) >> remove_offset;
}

int return_index(uint32_t address){
    uint32_t index = (address << g_num_cache_tag_bits) >> (g_num_cache_tag_bits + g_cache_offset_bits);
    // to get index, shift left (number of cache bits), then shift right (number of cache bits + offset bits) to place it back at lsb position
    return index % number_of_cache_blocks;
}

int return_tag(uint32_t address){
    uint32_t tag = address >> (g_cache_offset_bits + number_of_index_bits);
    // to get tag, shift right (offset bits + index bits)
    return tag;
}

int execute_random(mem_access_t access, cache_block_t **cache, int associativity, int random){  //random replacement policy
    for(int i = 0; i < associativity; i++){
        cache_block_t *block = &cache[return_index(access.address)][i];     //get index of set and store it into a variable
        if (return_tag(access.address) == return_tag(block->address) && block->valid_bit == 1){ // if there is a hit
            g_result.cache_hits++;
            block->access_num = 0;      //reset access number
            return 1;
        }
    }
    for(int i = 0; i < associativity; i++){     // if it is a miss and the set is not full
        cache_block_t *block = &cache[return_index(access.address)][i];
        if (block->valid_bit == 0){
            block->address = access.address;        // fill the block with the address
            block->valid_bit = 1;
            block->access_num = 0;
            g_result.cache_misses++;            // increase miss counter
            return 1;
        }
    }
    cache[return_index(access.address)][random].address = access.address;       // if there is a miss and cache is full
    cache[return_index(access.address)][random].access_num = 0;                 // replace a random block in that set
    g_result.cache_misses++;
    return 1;
}

int execute_fifo(mem_access_t access, cache_block_t **cache, int access_number, int associativity){     //FIFO replacement policy
    for(int i = 0; i < associativity; i++){
        cache_block_t *block = &cache[return_index(access.address)][i];         // point to the appropriate set
        if (return_tag(access.address) == return_tag(block->address) && block->valid_bit == 1){     // if there is a hit in the set
            g_result.cache_hits++;          // with a hit, the access number is not updated, unlike LRU
            return 1;
        }
    }
    for(int i = 0; i < associativity; i++){
        cache_block_t *block = &cache[return_index(access.address)][i];
        if (block->valid_bit == 0){                 // if it's a miss but the set isn't full
            block->address = access.address;
            block->valid_bit = 1;                   // set valid bit to 1
            block->access_num = access_number;      // access number is only updated when the block is first placed
            g_result.cache_misses++;                // increase miss counter
            return 1;
        }
    }
    int min_access = access_number;
    int index = 0;
    for(int i = 0; i < associativity; i++){     // finding the block with the lowest access number.
        cache_block_t *block = &cache[return_index(access.address)][i];
        if (min_access > block->access_num){
            min_access = block->access_num;
            index = i;
        }
    }
    cache[return_index(access.address)][index].address = access.address;        // replaces the block with the lowest access number (eg. first inserted)
    cache[return_index(access.address)][index].access_num = access_number;
    g_result.cache_misses++;
    return 1;
}

int execute_lru(mem_access_t access, cache_block_t **cache, int access_number, int associativity){      //execute LRU
    for(int i = 0; i < associativity; i++){
        cache_block_t *block = &cache[return_index(access.address)][i];
        if (return_tag(access.address) == return_tag(block->address) && block->valid_bit == 1){
            g_result.cache_hits++;
            block->access_num = access_number;      // if there is a hit, update the access number to the current one
            /*FORMATTING START
            printf("Tag: %i",return_tag(access.address));
            printf("  Set Index: %i",return_index(access.address));
            printf("  Block Index: %i",i);
            printf(" Offset: %i",return_offset(access.address));
            printf("   #HIT\n");
            */
            return 1;
        }
    }
    for(int i = 0; i < associativity; i++){
        cache_block_t *block = &cache[return_index(access.address)][i];
        if (block->valid_bit == 0){             // if there is a mit but the set isn't full, add the block to the set and update access number
            block->address = access.address;
            block->valid_bit = 1;
            block->access_num = access_number;
            /*FORMATTING START
            printf("Tag: %i",return_tag(block->address));
            printf("  Set Index: %i",return_index(block->address));
            printf("  Block Index: %i",i);
            printf(" Offset: %i",return_offset(access.address));
            printf("   #MISS\n");
            */
            g_result.cache_misses++;
            return 1;
        }
    }
    // if there is a miss and the set is full, find the block with the lowest access number to replace
    int min_access = access_number;
    int index = 0;
    for(int i = 0; i < associativity; i++){
        cache_block_t *block = &cache[return_index(access.address)][i];
        if (min_access > block->access_num){
            min_access = block->access_num;         // update the access number, unlike FIFO
            index = i;
        }
    }
    cache[return_index(access.address)][index].address = access.address;
    cache[return_index(access.address)][index].access_num = access_number;
    g_result.cache_misses++;                //replace block and update the miss counter
    /*FORMATTING START
    printf("Tag: %i",return_tag(access.address));
    printf("  Set Index: %i",return_index(access.address));
    printf("  Block Index: %i",index);
    printf(" Offset: %i",return_offset(access.address));
    printf("   #MISS\n");
    */
    return 1;
}

int main(int argc, char** argv) {
    time_t t;
    /* Intializes random number generator */
    /* Important: *DO NOT* call this function anywhere else. */
    srand((unsigned) time(&t));
    /* ----------------------------------------------------- */
    /* ----------------------------------------------------- */
    
    /*
     *
     * Read command-line parameters and initialize configuration variables.
     *
     */
    int improper_args = 0;
    char file[10000];
    if (argc < 6) {
        improper_args = 1;
        printf("Usage: ./mem_sim [replacement_policy: FIFO LRU Random] [associativity: 1 2 4 8 ...] [number_of_cache_blocks: 16 64 256 1024] [cache_block_size: 32 64] mem_trace.txt\n");
    } else  {
        /* argv[0] is program name, parameters start with argv[1] */
        if (strcmp(argv[1], "FIFO") == 0) {
            replacement_policy = FIFO;
        } else if (strcmp(argv[1], "LRU") == 0) {
            replacement_policy = LRU;
        } else if (strcmp(argv[1], "Random") == 0) {
            replacement_policy = Random;
        } else {
            improper_args = 1;
            printf("Usage: ./mem_sim [replacement_policy: FIFO LRU Random] [associativity: 1 2 4 8 ...] [number_of_cache_blocks: 16 64 256 1024] [cache_block_size: 32 64] mem_trace.txt\n");
        }
        associativity = atoi(argv[2]);
        number_of_cache_blocks = atoi(argv[3]);
        cache_block_size = atoi(argv[4]);
        strcpy(file, argv[5]);
    }
    if (improper_args) {
        exit(-1);
    }
    assert(number_of_cache_blocks == 16 || number_of_cache_blocks == 64 || number_of_cache_blocks == 256 || number_of_cache_blocks == 1024);
    assert(cache_block_size == 32 || cache_block_size == 64);
    assert(number_of_cache_blocks >= associativity);
    assert(associativity >= 1);
    
    printf("\n");
    printf("input:trace_file: %s\n", file);
    printf("input:replacement_policy: %s\n", get_replacement_policy(replacement_policy));
    printf("input:associativity: %u\n", associativity);
    printf("input:number_of_cache_blocks: %u\n", number_of_cache_blocks);
    printf("input:cache_block_size: %u\n", cache_block_size);
    printf("\n");

    /* Open the file mem_trace.txt to read memory accesses */
    FILE *ptr_file;
    ptr_file = fopen(file,"r");
    if (!ptr_file) {
        printf("Unable to open the trace file: %s\n", file);
        exit(-1);
    }
        
    
    /* result structure is initialized for you. */
    memset(&g_result, 0, sizeof(result_t));
    
    /* Do not delete any of the lines below.
     * Use the following snippet and add your code to finish the task. */

    number_of_sets = number_of_cache_blocks / associativity;            // finding the number of sets
    
    cache_block_t **cache = malloc(number_of_sets  * sizeof(cache_block_t));        // dynamically assign memory to cache
    
    for (int i = 0 ; i < number_of_sets; i++){
        cache[i] = (cache_block_t *)malloc(associativity * sizeof(cache_block_t));  // for every set, allocate appropriate memory
    }
    g_cache_offset_bits = (int)log2(cache_block_size);          // find offset bits
    
    number_of_index_bits = (int)log2(number_of_sets);           // find index bits
    
    g_num_cache_tag_bits = 32 - g_cache_offset_bits - number_of_index_bits;         // find tag bits by subtracting from size of memory address

    setup(cache, associativity);            // start setup of cache
    
    /* You may want to setup your Cache structure here. */
    
    mem_access_t access;
    /* Loop until the whxole trace file has been read. */
    int access_number = 0;
    int random;
    while(1) {
        access = read_transaction(ptr_file);
        // If no transactions left, break out of loop.
        if (access.address == 0)
            break;
        
        access_number++;
        if(replacement_policy == LRU){
            execute_lru(access, cache, access_number, associativity);
        }
        else if(replacement_policy == FIFO){
            execute_fifo(access, cache, access_number, associativity);
        }
        else{
            random = rand() % associativity;
            execute_random(access, cache, associativity, random);
        }
        
    }
    
    /* Do not modify code below. */
    /* Make sure that all the parameters are appropriately populated. */
    print_statistics(g_num_cache_tag_bits, g_cache_offset_bits, &g_result);
    
    /* Close the trace file. */
    fclose(ptr_file);
    return 0;
}
