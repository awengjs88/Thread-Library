#include <ucontext.h>
#include "thread.h"
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>
#include <ctype.h>
#include <stdio.h>
#include <sstream>
#include <algorithm>
#include <map>
#include <utility>
#include <set>
#include <cstdlib>
#include "interrupt.h"

#define STACK_SIZE 262144

using namespace std;

typedef void (*thread_startfunc_t) (void *);

vector<ucontext_t *> readyQueue;
vector<ucontext_t *> lockedQueue, lockedQueueContext;
vector<ucontext_t *> waitQueue;
vector<int> currentLocks, lockedQueueId, waitQueueLock, waitQueueCond;
ucontext_t initial, schedulerContext, releaseMem;
ucontext_t tempLock;
ucontext_t *current;
ucontext_t *next;
bool lib_init_called = false;

//check if a thread owns a specific lock
bool isLockOwner (ucontext_t * thread) {
	for (int j = 0; j < lockedQueueContext.size(); j++) {
		if (thread == lockedQueueContext[j]) {
			return true;
		}
	}
	return false;
}

//check queue
int searchWaitQueue (unsigned int lock, unsigned int cond) {
	for (int a = 0; a < waitQueueLock.size(); a++) {
		if (waitQueueLock[a] == lock) {
			for (int b = 0; b < waitQueueCond.size(); b++) {
				if (waitQueueCond[b] == cond) {
					return b;
				}
			}
		}
	}
	return -1;
}

//delete from queue
void deleteWaitQueue (int index) {
	readyQueue.push_back(waitQueue[index]);
	waitQueue.erase(waitQueue.begin()+index);
	waitQueueLock.erase(waitQueueLock.begin()+index);
	waitQueueCond.erase(waitQueueCond.begin()+index);
}

//check if lock is being used by a thread
bool isLockUsed (unsigned int lock) {
	for (int j = 0; j < currentLocks.size(); j++) {
		if (currentLocks[j] == lock) {
			return true;
		}
	}
	return false;
}

//if all threads are serviced, close thread library
void threadScheduler() {
	while (readyQueue.size() != 0) {
		current = readyQueue.front();
		readyQueue.erase(readyQueue.begin());
		swapcontext(&schedulerContext, current);
	}
	cout << "Thread library exiting." << endl;
}

//initialize thread library
int thread_libinit(thread_startfunc_t func, void *arg){

	if (lib_init_called == true) {
		return -1;
	}
	else {
		lib_init_called = true;
		ucontext_t ucontext_ptr;
		getcontext(&ucontext_ptr);           // ucontext_ptr has type (ucontext_t *)
		getcontext(&initial);
		getcontext(&schedulerContext);

		char *stack = new char [STACK_SIZE];
		ucontext_ptr.uc_stack.ss_sp = stack;
		ucontext_ptr.uc_stack.ss_size = STACK_SIZE;
		ucontext_ptr.uc_stack.ss_flags = 0;
		ucontext_ptr.uc_link = &schedulerContext;

		schedulerContext.uc_stack.ss_sp = stack;
		schedulerContext.uc_stack.ss_size = STACK_SIZE;
		schedulerContext.uc_stack.ss_flags = 0;
		schedulerContext.uc_link = &schedulerContext;
	
	
		makecontext(&schedulerContext, threadScheduler, 1, arg);
	
		makecontext(&ucontext_ptr, (void (*)()) func, 1, arg);
		readyQueue.push_back(&ucontext_ptr);
		threadScheduler();
	}
	return 0;
}

//create context for a thread
void makeContext(thread_startfunc_t func, void *arg) {
	ucontext_t tempContext;
	getcontext(&tempContext);
	tempContext.uc_stack.ss_sp = malloc(STACK_SIZE);
	tempContext.uc_stack.ss_size = STACK_SIZE;
	tempContext.uc_stack.ss_flags = 0;
	tempContext.uc_link = &schedulerContext;
	makecontext(&tempContext, (void (*)(void)) func, 1, arg);
	readyQueue.push_back(&tempContext);
}

//create threads
int thread_create(thread_startfunc_t func, void *arg){
	if (lib_init_called == true) {
		char *stack = new char [STACK_SIZE];
		initial.uc_stack.ss_sp = stack;
		initial.uc_stack.ss_size = STACK_SIZE;
		initial.uc_stack.ss_flags = 0;
		initial.uc_link = current;
		makecontext(&initial, (void (*)(void)) makeContext, 2, func, arg);
		swapcontext(current, &initial);
		return 0;
	}
	else {
		return -1;
	}
}

//causes thread to yield the CPU to the next runnable thread
extern int thread_yield(void){
	if (lib_init_called == true) {
		readyQueue.push_back(current);
		next = &schedulerContext;
		swapcontext(current, next);
		return 0;
	}
	else {
		return -1;
	}
}

//locks a thread with a unique identifier
extern int thread_lock(unsigned int lock) {
	if (lib_init_called == true) {
		//interrupt_disable();
		ucontext_t temp;
		while (isLockUsed(lock)) {
			//getcontext(&tempLock);
			getcontext(current);
			for (int k = 0; k < lockedQueueContext.size(); k++) {
				if (current == lockedQueueContext[k]) {
					return -1;
				}
			}
			readyQueue.push_back(current);
			next = &schedulerContext;
			swapcontext(current, next); //switch to different context
		}
		interrupt_disable();
		currentLocks.push_back(lock);
		lockedQueueContext.push_back(current);
		interrupt_enable();
		return 0;
	}
	else {
		return -1;
	}
		
}

//unlock a thread with a unique identifier
extern int thread_unlock(unsigned int lock) {
	if (lib_init_called == true) {
		//ucontext_t checkLockOwner;
		interrupt_disable();
		if (isLockUsed(lock)) {
			getcontext(current);
			for (int k = 0; k < lockedQueueContext.size(); k++) {
				if (current == lockedQueueContext[k]) {
					for (int j = 0; j < currentLocks.size(); j++) {
						if (currentLocks[j] == lock) {
							currentLocks.erase(currentLocks.begin()+j);
							lockedQueueContext.erase(lockedQueueContext.begin()+j);
						}
					}
					for (int i = 0; i < lockedQueueId.size(); i++) {
						if (lockedQueueId[i] == lock) {
							next = &schedulerContext;
							readyQueue.push_back(lockedQueue[i]);
							lockedQueue.erase(lockedQueue.begin()+i);
							lockedQueueId.erase(lockedQueueId.begin()+i);
							break;
						}
					}	
				}
			}
		}

		else {
			interrupt_enable();
			return -1;
		}
		
		interrupt_enable();
		return 0;
	}
	else {
		return -1;
	}
}

//puts thread on halt until lock is free
extern int thread_wait(unsigned int lock, unsigned int cond) {
	if (lib_init_called == true) {
		thread_unlock(lock);
		ucontext_t waitTemp;
		getcontext(&waitTemp);
		next = &schedulerContext;
		waitQueue.push_back(&waitTemp);
		waitQueueLock.push_back(lock);
		waitQueueCond.push_back(cond);
		swapcontext(&waitTemp, next);
		while (isLockUsed(lock)) {
			thread_yield();
		}
		thread_lock(lock);
		return 0;
	}
	else {
		return -1;
	}
}

//sends signal to unlock a lock with the specified identifier
extern int thread_signal(unsigned int lock, unsigned int cond) {
	if (lib_init_called == true) {
		if (searchWaitQueue(lock, cond) != -1) {
			deleteWaitQueue(searchWaitQueue(lock, cond));
		}
		return 0;
	}
	else {
		return -1;
	}
}

//unlocks all locks with the speicified identifier
extern int thread_broadcast(unsigned int lock, unsigned int cond) {
	if (lib_init_called == true) {
		while (searchWaitQueue(lock, cond) != -1) {
			deleteWaitQueue(searchWaitQueue(lock, cond));
		}
		return 0;
	}
	else {
		return -1;
	}
}

