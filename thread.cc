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
//ucontext_t temp;
ucontext_t *current;
ucontext_t *next;
bool lib_init_called = false;

/*void memoryRelease() {
	ucontext_t currentContext;
	getcontext(&currentContext);
	cout << "De-allocate memory: " << &currentContext << endl;
	delete [] (char*)currentContext.uc_stack.ss_sp;
	setcontext(&schedulerContext);
}*/

bool isLockOwner (ucontext_t * thread) {
	for (int j = 0; j < lockedQueueContext.size(); j++) {
		if (thread == lockedQueueContext[j]) {
			return true;
		}
	}
	return false;
}

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

void deleteWaitQueue (int index) {
	//cout << "Index in waitQueue " << index << endl;
	readyQueue.push_back(waitQueue[index]);
	//cout << "Added to readyQueue: " << waitQueue[index] << endl;
	waitQueue.erase(waitQueue.begin()+index);
	waitQueueLock.erase(waitQueueLock.begin()+index);
	waitQueueCond.erase(waitQueueCond.begin()+index);
}

bool isLockUsed (unsigned int lock) {
	for (int j = 0; j < currentLocks.size(); j++) {
		if (currentLocks[j] == lock) {
			return true;
		}
	}
	return false;
}

void threadScheduler() {
	while (readyQueue.size() != 0) {
		current = readyQueue.front();
		//cout << "Front of queue: " << readyQueue.front() << endl;
		readyQueue.erase(readyQueue.begin());
		swapcontext(&schedulerContext, current);
		//cout << "scheduler" << endl;
		//cout << "size of ready queue1: " << readyQueue.size() << endl;
	}
	cout << "Thread library exiting." << endl;
}

int thread_libinit(thread_startfunc_t func, void *arg){
	//cout << "thread_libinit" << endl;
	if (lib_init_called == true) {
		return -1;
	}
	else {
		lib_init_called = true;
		ucontext_t ucontext_ptr;
		getcontext(&ucontext_ptr);           // ucontext_ptr has type (ucontext_t *)
		getcontext(&initial);
		//getcontext(&releaseMem);
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
	
		/*releaseMem.uc_stack.ss_sp = stack;
		releaseMem.uc_stack.ss_size = STACK_SIZE;
		releaseMem.uc_stack.ss_flags = 0;
		releaseMem.uc_link = NULL;

		makecontext(&releaseMem, memoryRelease, 1, arg);*/
	
		makecontext(&schedulerContext, threadScheduler, 1, arg);
	
		makecontext(&ucontext_ptr, (void (*)()) func, 1, arg);
		readyQueue.push_back(&ucontext_ptr);
		//cout << "Parent thread: " << &ucontext_ptr << endl;
		threadScheduler();
	}
	return 0;
}

void makeContext(thread_startfunc_t func, void *arg) {
	//cout << "makeContext" << endl;
	ucontext_t tempContext;
	getcontext(&tempContext);
	tempContext.uc_stack.ss_sp = malloc(STACK_SIZE);
	tempContext.uc_stack.ss_size = STACK_SIZE;
	tempContext.uc_stack.ss_flags = 0;
	tempContext.uc_link = &schedulerContext;
	makecontext(&tempContext, (void (*)(void)) func, 1, arg);
	readyQueue.push_back(&tempContext);
	//cout << "Child context: " << &tempContext << endl;
}

int thread_create(thread_startfunc_t func, void *arg){
	if (lib_init_called == true) {
		char *stack = new char [STACK_SIZE];
		initial.uc_stack.ss_sp = stack;
		initial.uc_stack.ss_size = STACK_SIZE;
		initial.uc_stack.ss_flags = 0;
		initial.uc_link = current;
		//initial.uc_link = &schedulerContext;
		makecontext(&initial, (void (*)(void)) makeContext, 2, func, arg);
		//makecontext(&initial, (void (*)(void)) func, 1, arg);
		swapcontext(current, &initial);
		//delete [] (char*)stack;
		//cout << "Child context: " << &initial << endl;
		return 0;
	}
	else {
		return -1;
	}
}

extern int thread_yield(void){
	if (lib_init_called == true) {
		//cout << "yield" << endl;
		readyQueue.push_back(current);
		//cout << "Added to readyQueue: " << current << endl;
		//cout << "Current thread: " << current << endl;
		//cout << "Next thread: " << readyQueue[0] << endl;
		next = &schedulerContext;
		//cout << "size of ready queue: " << readyQueue.size() << endl;
		swapcontext(current, next);
		return 0;
	}
	else {
		return -1;
	}
}
extern int thread_lock(unsigned int lock) {
	if (lib_init_called == true) {
		//cout << "lock " << lock << endl;
		//interrupt_disable();
		ucontext_t temp;
		while (isLockUsed(lock)) {
			//cout << "Lock in use" << endl;
			//getcontext(&tempLock);
			getcontext(current);
			//cout << "temp: " << &temp << endl;
			for (int k = 0; k < lockedQueueContext.size(); k++) {
				if (current == lockedQueueContext[k]) {
					//cout << "tempLock: " << &tempLock << " lockedQueueContext[k]: " << lockedQueueContext[k] << endl;
					//cout << "Thread already called this lock" << endl;
					return -1;
				}
			}
			//cout << "Hello" << endl;
			//cout << "temp: " << &temp << endl;
			readyQueue.push_back(current);
			//lockedQueue.push_back(current); //add context to blocked queue
			//lockedQueueId.push_back(lock);
			next = &schedulerContext;
			//cout << "size of lock queue: " << lockedQueue.size() << endl;
			swapcontext(current, next); //switch to different context
		}
			//cout << "size of ready queue: " << readyQueue.size() << endl;
		interrupt_disable();
		currentLocks.push_back(lock);
		//getcontext(&tempLock);
		lockedQueueContext.push_back(current);
		//cout << "Added this address to lockedQueueContext: " << &tempLock << endl;
		interrupt_enable();
		return 0;
	}
	else {
		return -1;
	}
		
}
extern int thread_unlock(unsigned int lock) {
	if (lib_init_called == true) {
		//cout << "unlock " << lock << endl;
		//ucontext_t checkLockOwner;
		interrupt_disable();
		if (isLockUsed(lock)) {
			//cout << "Lock being used" << endl;
			getcontext(current);
			//cout << "Address of current context: " << &tempLock << endl;
			for (int k = 0; k < lockedQueueContext.size(); k++) {
				if (current == lockedQueueContext[k]) {
					//cout << "true" << endl;
					for (int j = 0; j < currentLocks.size(); j++) {
						if (currentLocks[j] == lock) {
							currentLocks.erase(currentLocks.begin()+j);
							lockedQueueContext.erase(lockedQueueContext.begin()+j);
						}
					}
					//cout << "Locked Queue size: " << lockedQueueId.size() << endl;
					for (int i = 0; i < lockedQueueId.size(); i++) {
						//cout << "checking locked queue id" << endl;
						if (lockedQueueId[i] == lock) {
							//cout << "Size of lockedQueue: " << lockedQueueId.size() << endl;
							//cout << "Locked queue address: " << lockedQueue[i] << endl;
							//cout << "size of ready queue: " << readyQueue.size() << endl;
							next = &schedulerContext;
							readyQueue.push_back(lockedQueue[i]);
							//cout << "Added to readyQueue: " << lockedQueue[i] << endl;
							lockedQueue.erase(lockedQueue.begin()+i);
							lockedQueueId.erase(lockedQueueId.begin()+i);
							break;
						}
					}	
				}
			}
		}

		else {
			//cout << "Trying to unlock an unused lock" << endl;
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
extern int thread_wait(unsigned int lock, unsigned int cond) {
	if (lib_init_called == true) {
		//cout << "Wait" << lock << " " << cond << endl;
		thread_unlock(lock);
		ucontext_t waitTemp;
		getcontext(&waitTemp);
		next = &schedulerContext;
		waitQueue.push_back(&waitTemp);
		waitQueueLock.push_back(lock);
		waitQueueCond.push_back(cond);
		//cout << "waitQueue added: " << &waitTemp << endl;
		//cout << "waitQueue size: " << waitQueue.size() << endl;
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
extern int thread_signal(unsigned int lock, unsigned int cond) {
	if (lib_init_called == true) {
		//cout << "Signal" << lock << " " << cond << endl;
		if (searchWaitQueue(lock, cond) != -1) {
			//cout << "Signal found lock in waitQueue" << endl;
			deleteWaitQueue(searchWaitQueue(lock, cond));
		}
		return 0;
	}
	else {
		return -1;
	}
}
extern int thread_broadcast(unsigned int lock, unsigned int cond) {
	if (lib_init_called == true) {
		//cout << "Broadcast" << lock << " " << cond << endl;
		while (searchWaitQueue(lock, cond) != -1) {
			//cout << "Broadcast found lock in waitQueue" << endl;
			deleteWaitQueue(searchWaitQueue(lock, cond));
		}
		return 0;
	}
	else {
		return -1;
	}
}

