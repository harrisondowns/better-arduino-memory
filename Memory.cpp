//
//  Memory.cpp
//  Backbone-2018.Tests
//
//  Created by Harrison Downs on 7/8/18.
//  Copyright © 2018 Harrison Downs. All rights reserved.
//

#include "Memory.h"
#include <cstdint>
#include "Arduino.h"
//#include <iostream>

//#define HEAP_SIZE 32768
#define HEAP_SIZE 16384

#define NUMBER_OF_PAGE_SIZES 12 // log2(HEAP_SIZE) - 2
#define ENDPTR -1
#define POINTER_SIZE 4
#define NUMBER_OF_PAGE_BYTES (HEAP_SIZE / POINTER_SIZE / 8)

using namespace std;

int32_t pagePointers[NUMBER_OF_PAGE_SIZES];
int pageSizes[NUMBER_OF_PAGE_SIZES] = {4, 8, 16, 32, 64, 128, 256, 512,
                                        1024, 2048, 4096, 8192};

char heap[HEAP_SIZE];
char usedPages[NUMBER_OF_PAGE_BYTES];


void *breakDownPage(int sizeCounter, int wantedSizeCounter);

// initializes the memory scheme and nulls out the heap
void memInit(){
    for (int i = 0; i < HEAP_SIZE; i++){
        heap[i] = 0;
    }
    
    for (int i = 0; i < NUMBER_OF_PAGE_SIZES; i++){
        pagePointers[i] = ENDPTR;
    }
    
    for (int i = 0; i < NUMBER_OF_PAGE_BYTES; i++){
        usedPages[i] = 0;
    }
    
    int maxPageSize = pageSizes[NUMBER_OF_PAGE_SIZES - 1];
    int32_t lastPage = 0;
    for (int i = HEAP_SIZE - maxPageSize; i >=0; i -= maxPageSize){
        void *currentPage = &(heap[i]);
        int32_t *pageInt = (int32_t*)currentPage;
        *pageInt = lastPage;
        lastPage = i;
    }
    pagePointers[NUMBER_OF_PAGE_SIZES - 1] = lastPage;
}

// marks a given page byte as used
void markUsed(int32_t byteNum){
    int32_t pageInd = byteNum / POINTER_SIZE;
    usedPages[pageInd / 8] = usedPages[pageInd / 8] | (1 << (pageInd % 8));
}

// marks a given page byte as free
void markFree(int32_t byteNum){
    int32_t pageInd = byteNum / POINTER_SIZE;
    usedPages[pageInd / 8] = usedPages[pageInd / 8] & ~(1 << (pageInd % 8));
}

// checks if a given page bit is used or not
bool isUsed(int32_t pageNum){
    int32_t pageInd = pageNum / POINTER_SIZE;
    return (usedPages[pageInd / 8] >> (pageInd % 8)) & 1;
}

// mallocs an object of a given size
void *mallocNew(int size){
    int sizeCounter = 0;
    int smallestPageCounter = 0;
    while (pageSizes[sizeCounter] < size || pagePointers[sizeCounter] == ENDPTR){
        if (pageSizes[sizeCounter] >= size && (sizeCounter == 0 ||
                                               pageSizes[sizeCounter - 1] < size)){
            smallestPageCounter = sizeCounter;
        }
        sizeCounter++;
    }
    if (sizeCounter < NUMBER_OF_PAGE_SIZES){
        return breakDownPage(sizeCounter, smallestPageCounter);
    }
    Serial.println("TRYING TO RECLAIM HEAP");
    reclaimHeap();

    return mallocNew(size);
}

// breaks down a given page size into two components until it reaches
// the requested page size. Then takes that page off the list and returns it.
void *breakDownPage(int sizeCounter, int wantedSizeCounter){
    if (sizeCounter > wantedSizeCounter){
        // sizeCounter page size / 2
        int32_t smallPageSize = pageSizes[sizeCounter] / 2;
        
        // break large page into two smaller pages
        int32_t firstPage = pagePointers[sizeCounter];
        int32_t secondPage = firstPage + smallPageSize;
        
        // pull out pointers that these pages will point to for reassign
        int32_t *firstPageP = (int32_t*)&(heap[firstPage]);
        int32_t *secondPageP = (int32_t*)&(heap[secondPage]);
        
        // remove firstPage from the pointers array for larger size
        pagePointers[sizeCounter] = *firstPageP;
        
        *firstPageP = secondPage;
        *secondPageP = ENDPTR;
        pagePointers[sizeCounter - 1] = firstPage;
        
        return breakDownPage(sizeCounter - 1, wantedSizeCounter);
    }
    else{
        int32_t pageToReturn = pagePointers[sizeCounter];
        
        for (int i = 0; i < pageSizes[sizeCounter]; i += POINTER_SIZE){
            markUsed(pageToReturn + i);
        }
        
        // advance pointers to next page
        int32_t *pageP = (int32_t*)&(heap[pageToReturn]);
        pagePointers[sizeCounter] = *pageP;
        
        return &(heap[pageToReturn]);
    }
}

// frees a page of a given size
void freeVar(void *p, int size){
    // looks for the minimum page size larger than the given size
    int pageCounter = 0;
    while (pageCounter < NUMBER_OF_PAGE_SIZES - 1 && pageSizes[pageCounter] < size){
        pageCounter++;
    }
    
    // get the index in the heap of the given pointer
    long heapInd = (long)p - (long)(&(heap[0]));

    // make the value of that page = the head of the list
    int32_t *freePage = (int32_t*)&(heap[heapInd]);
    *freePage = pagePointers[pageCounter];
    
    // marks each page bit as free
    for (int i = 0; i < pageSizes[pageCounter]; i += POINTER_SIZE){
        markFree((int32_t)heapInd + i);
    }
    
    // add the page to the front of its list
    pagePointers[pageCounter] = (int32_t)heapInd;
}

// takes a given index and the number of pages in a row that are free
// and readds them to the heap recursively in as few pages as possible
void addToHeap(int32_t ind, int size){
    if (size == 0){
        return;
    }
    for (int i = NUMBER_OF_PAGE_SIZES - 1; i >= 0; i--){
        int pageSize = pageSizes[i] / POINTER_SIZE;
        if ((ind == 0 || ind >= pageSize) && ind % pageSize == 0){
            if (size >= pageSize){
                int32_t *freePage = (int32_t*)&(heap[ind * 4]);
                *freePage = pagePointers[i];
                pagePointers[i] = ind * 4;
                addToHeap(ind + pageSize, size - pageSize);
                break;
            }
        }
    }
}

// restructures the heap. merges neighboring free pages back together.
void reclaimHeap(){
    // reset the page pointers
    for (int i = 0; i < NUMBER_OF_PAGE_SIZES; i++){
        pagePointers[i] = ENDPTR;
    }
    
    // count number of pages in a row that are free
    int numPagesInRow = 0;
    int startPage = 0;
    for (int i = 0; i < HEAP_SIZE / POINTER_SIZE; i++){

        // if the index is the last byte OR the byte is used OR the page size is at a maximum
        if (i == (HEAP_SIZE / POINTER_SIZE) - 1 ||
            isUsed(i * POINTER_SIZE) ||
            numPagesInRow == pageSizes[NUMBER_OF_PAGE_SIZES - 1] / POINTER_SIZE){

            // add one to avoid off by one errors if the last page is free
            if (i == (HEAP_SIZE / POINTER_SIZE) - 1 && !isUsed(i * POINTER_SIZE)){
                numPagesInRow++;
            }

            // add the relevant free pages to the heap
            addToHeap(startPage, numPagesInRow);

            // start over
            numPagesInRow = 0;
            if (isUsed(i * POINTER_SIZE)){
                startPage = i + 1;
            } else{
                startPage = i;
            }
            
        }

        // actually count number of pages in a row that are free
        if (!isUsed(i * POINTER_SIZE)){
            numPagesInRow++;
        }
    }
    
}




