// mem_mgr.cpp


//---------------------------------------- NOTES --------------------------------------------
// main idea is that we have vm adress that refers to page and offset
// page table that has the frame of memory that holds that page
// when you look at a memory address and it's empty, you go to file and read in a page and mark the index of frame (first empty frame space)
// frame counter set to 0, store at 0 and then incriment the frame counter 
// if no more space, fifo- take away first one, lru - least recently used 
// replacement/placement policy, demand paging
// when replace page thats when we replace it and increase the counter for the next one tp be replaced and then when its at the end, wrap it around back to 0

// the value is 57 it says 39 bc its in hex
// phy: number is getting that number from frm x frame size + offset
// the number could be wrong because the page is giving us the wrong frame. is the page already exists in the page table you should use that 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <cassert>

#include <list>

#pragma warning(disable : 4996)

#define ARGC_ERROR 1
#define FILE_ERROR 2

#define FRAME_SIZE  256
#define FIFO 0
#define LRU 1
#define REPLACE_POLICY FIFO 

#define NFRAMES 128 //switch from 256 to 128
#define PTABLE_SIZE 256
#define TLB_SIZE 16

struct page_node {    
    size_t npage;
    size_t frame_num;
    bool is_present;
    bool is_used;
};

char* ram = (char*)malloc(NFRAMES * FRAME_SIZE);
page_node pg_table[PTABLE_SIZE];  // page table and (single) TLB
//change to standard list if use below std line
page_node tlb[TLB_SIZE]; //small copy of page tabla

//std::list<page_node> lru_list;

const char* passed_or_failed(bool condition) { 
    return condition ? " + " : "fail"; 
}

size_t failed_asserts = 0;

size_t get_page(size_t x)   { 
    return 0xff & (x >> 8); 
}

size_t get_offset(size_t x) {
    return 0xff & x;
}

void get_page_offset(size_t x, size_t& page, size_t& offset) {
    page = get_page(x);
    offset = get_offset(x);
}

void update_frame_ptable(size_t npage, size_t frame_num) {
    pg_table[npage].frame_num = frame_num;
    pg_table[npage].is_present = true;
    pg_table[npage].is_used = true;
}

int find_frame_ptable(size_t frame) {  // FIFO, if there a frame like that in the page table, otherwise -1
    for (int i = 0; i < PTABLE_SIZE; i++) {
        if (pg_table[i].frame_num == frame && 
            pg_table[i].is_present == true) { 
                return i; 
            }
    }
    return -1;
}

size_t get_used_ptable() {  // LRU
    size_t unused = -1;
    for (size_t i = 0; i < PTABLE_SIZE; i++) {
        if (pg_table[i].is_used == false && 
            pg_table[i].is_present == true) { 
                return (size_t)i; 
            }
    }
    // All present pages have been used recently, set all page entry used flags to false
    for (size_t i = 0; i < PTABLE_SIZE; i++) { 
        pg_table[i].is_used = false; 
    }

    for (size_t i = 0; i < PTABLE_SIZE; i++) {
        page_node& r = pg_table[i];
        if (!r.is_used && r.is_present) { 
            return i; 
        }
    }
    return (size_t)-1;
}

int check_tlb(size_t page) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (tlb[i].npage == page) { 
            return i; 
        }
    }
    return -1;
}

void open_files(FILE*& fadd, FILE*& fcorr, FILE*& fback) { 
    fadd = fopen("addresses.txt", "r");
    if (fadd == NULL) { 
        fprintf(stderr, "Could not open file: 'addresses.txt'\n");  
        exit(FILE_ERROR); 
    }

    fcorr = fopen("correct.txt", "r");
    if (fcorr == NULL) { 
        fprintf(stderr, "Could not open file: 'correct.txt'\n");  
        exit(FILE_ERROR); 
    }

    fback = fopen("BACKING_STORE.bin", "rb");
    if (fback == NULL) { 
        fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  
        exit(FILE_ERROR); 
    }
}

void close_files(FILE* fadd, FILE* fcorr, FILE* fback) { 
    fclose(fadd);
    fclose(fcorr);
    fclose(fback);
}

void initialize_pg_table_tlb() { 
    for (int i = 0; i < PTABLE_SIZE; ++i) {
        pg_table[i].npage = (size_t)i;
        pg_table[i].is_present = false;
        pg_table[i].is_used = false;
    }
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb[i].npage = (size_t)-1;
        tlb[i].is_present = false;
        pg_table[i].is_used = false;
    }
}

void summarize(size_t pg_faults, size_t tlb_hits) {  //shpw the tlb and page fault percentage 
    printf("\nPage Fault Percentage: %1.3f%%", (double)pg_faults / 1000);
    printf("\nTLB Hit Percentage: %1.3f%%\n\n", (double)tlb_hits / 1000);
    printf("ALL logical ---> physical assertions PASSED!\n");
    printf("\n\t\t...done.\n");
}

void tlb_add(int index, page_node entry) { 
    tlb[index] = entry;
}  

void tlb_remove(int index) { 
    tlb[index].is_present = false;
    tlb[index].npage = (size_t)-1;
} 

void tlb_hit(size_t& frame, size_t& page, size_t& tlb_hits, int result) { 
    frame = tlb[result].frame_num;
    tlb_hits++;
 }

void tlb_miss(size_t& frame, size_t& page, size_t& tlb_track) {   //updates the frame from page table
    frame = pg_table[page].frame_num;
    //need to update tlb
    tlb_add(tlb_track++, pg_table[page]);
}

void fifo_replace_page(size_t& frame ) { //FIFO: when a page must be replaced, the oldest page is chosen. 
    tlb_remove(frame);
}  

void lru_replace_page(size_t& frame) { //LRU: chooses the page that has not been used for the longest period of time. 
    for(int i = 0; i < PTABLE_SIZE - 1; i++){
        pg_table[i] = pg_table[i+1];
    } 
    frame = pg_table[0].frame_num; 
    tlb[frame].is_present = false;
    pg_table[frame].is_used = false;
}

void page_fault(size_t& frame, size_t& page, size_t& frames_used, size_t& pg_faults, size_t& tlb_track, FILE* fbacking) {  
    unsigned char buf[BUFSIZ];
    memset(buf, 0, sizeof(buf));
    bool is_memfull = false;

    ++pg_faults;
    if (frames_used >= NFRAMES) { 
        is_memfull = true; 
    }
    frame = frames_used % NFRAMES;    // FIFO only

    if (is_memfull) { 

    }
         // load page into RAM, update pg_table, TLB
    fseek(fbacking, page * FRAME_SIZE, SEEK_SET);
    fread(buf, FRAME_SIZE, 1, fbacking);

    for (int i = 0; i < FRAME_SIZE; i++) {
        *(ram + (frame * FRAME_SIZE) + i) = buf[i];
    }
    update_frame_ptable(page, frame);
    tlb_add(tlb_track++, pg_table[page]);
    if (tlb_track > 15) { 
        tlb_track = 0; 
    }
    
    ++frames_used;
} 

void check_address_value(size_t logic_add, size_t page, size_t offset, size_t physical_add,
                         size_t& prev_frame, size_t frame, int val, int value, size_t o) { 
    printf("log: %5lu 0x%04x (pg:%3lu, off:%3lu)-->phy: %5lu (frm: %3lu) (prv: %3lu)--> val: %4d == value: %4d -- %s", 
          logic_add, logic_add, page, offset, physical_add, frame, prev_frame, 
          val, value, passed_or_failed(val == value));

    if (frame < prev_frame) {  
        printf("   HIT!\n");
    } else {
        prev_frame = frame;
        printf("----> pg_fault\n");
    }
    if (o % 5 == 4) { printf("\n"); }

    if (val != value) { ++failed_asserts; }
    if (failed_asserts > 5) { exit(-1); }
}

void run_simulation() { 
        // addresses, pages, frames, values, hits and faults
    size_t logic_add, virt_add, phys_add, physical_add;
    size_t page, frame, offset, value, prev_frame = 0, tlb_track = 0;
    size_t frames_used = 0, pg_faults = 0, tlb_hits = 0;
    int val = 0;
    char buf[BUFSIZ];

    bool is_memfull = false;     // physical memory to store the frames

    initialize_pg_table_tlb();

        // addresses to test, correct values, and pages to load
    FILE *faddress, *fcorrect, *fbacking;
    open_files(faddress, fcorrect, fbacking);

    for (int o = 0; o < 1000; o++) {     // read from file correct.txt
        fscanf(fcorrect, "%s %s %lu %s %s %lu %s %ld", buf, buf, &virt_add, buf, buf, &phys_add, buf, &value);  

        fscanf(faddress, "%ld", &logic_add);  
        get_page_offset(logic_add, page, offset);

        int result = check_tlb(page);
        if (result >= 0) {  
            tlb_hit(frame, page, tlb_hits, result); 
        } else if (pg_table[page].is_present) {
            tlb_miss(frame, page, tlb_track);
        } else {         // page fault
            page_fault(frame, page, frames_used, pg_faults, tlb_track, fbacking);
        }

        physical_add = (frame * FRAME_SIZE) + offset;
        val = (int)*(ram + physical_add);

        check_address_value(logic_add, page, offset, physical_add, prev_frame, frame, val, value, o);
    }
    close_files(faddress, fcorrect, fbacking);  // and time to wrap things up
    free(ram);
    summarize(pg_faults, tlb_hits);
}


int main(int argc, const char * argv[]) {
    run_simulation();
    return 0;
}
