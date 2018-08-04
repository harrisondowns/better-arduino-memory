//
//  Memory.hpp
//  Backbone-2018.Tests
//
//  Created by Harrison Downs on 7/8/18.
//  Copyright © 2018 Harrison Downs. All rights reserved.
//

#ifndef Memory_hpp
#define Memory_hpp

//#include <stdio.h>

void memInit();

void *mallocNew(int size);
void freeVar(void *p, int size);


// since these functions use C++ template features, the implementation has to be
// exposed in the header file. They are the easy to call wrappe functions for the
// above malloc and frees.
template <typename T>
T *mallocNew(){
    return (T*)mallocNew(sizeof(T));
}

template <typename T>
void freeVar(T *t){
    freeVar((void*)t, sizeof(T));
}

void reclaimHeap();


#endif /* Memory_hpp */
