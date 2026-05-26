
#pragma once

#include "minicoro.h"

/**
Small utility layer on top of minicoro to manage coroutines.

How this works:

- call process_coroutines() in your main update loop, it will resume all active coroutines.
- call destroy_all_coroutines() for cleanup

In your Immediate mode routines: 
- create_managed_coroutine() and grab the handle to the coroutine, keep it around.
- When you want to check on the coroutine, call get_coroutine(handle) to get the pointer to the
coroutine object, then check its status or user data or whatever you need.
- when you check the coroutine and find it's done (ie. mco_status(co) == MCO_DEAD), call destroy_coroutine(handle) to
clean it up. don't forget to set your handle to -1 or something so you don't accidentally check on it again next frame.

*/

int create_managed_coroutine(void(*co_func)(mco_coro*), void* user_data = nullptr);

mco_coro* get_coroutine(int handle);

void destroy_coroutine(int handle);

// call this in an update loop to resume all active coroutines
void process_coroutines();

void destroy_all_coroutines();

// coroutine control functions
void yield();

typedef bool (*CoConditionalNoArg)();
typedef bool (*CoConditional)(void* userdata);
void yield_until_true(CoConditionalNoArg conditionalfunc);
void yield_until_true(CoConditional conditionalfunc, void* userdata);