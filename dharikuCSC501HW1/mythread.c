#include "mythread.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#define MEM_SIZE (8*1024)

typedef int bool;
#define true 1;
#define false 0;

typedef struct container {
	unsigned long long int id;
	ucontext_t * context;
	struct container * next; //for ready queue
	struct container * prev; //for ready queue
	struct container * childhead; //head of children list
	struct container * childtail; //tail of children list
	struct container * parent; //parent of the children list, held by all children
	struct container * childnext; //if this struct is one of the children in the list, then its next
	struct container * childprev; //if this struct is one of the children in the list, then its prev
	bool thischildjoinsparent; //true (for child) to call parent(single join) - set by the thread calling join(on this child) if false(for child), it could be join all or a normal thread execution based on thisparentjoinsall(for parent)
	bool thisparentjoinsall; //if this is true (for parent) and thischildjoinsparent is false (for child), then the child can call its sibling
	bool executed;
	int childCount;
	unsigned long long int parentId;
} container;

typedef struct semaphore {
	unsigned int ID;
	int count;
	container* start;
	container* end;
} semaphore;

container* executing_container;

unsigned long long int THREAD_ID = 0;
unsigned int semaphore_id = 0;
container* head;
container* tail;
unsigned long long int threadCount = 0;

container* blocktail;
container* blockhead;

ucontext_t comeback;
bool joinoryield = false
bool freed = false
;
;

void MyThreadInit(void (*start_funct)(void *), void *args) {

	ucontext_t* context_func = (ucontext_t*) malloc(sizeof(ucontext_t));
	if (context_func == NULL) {
		printf("\nmalloc failed, exiting...\n");
		exit(0);
	}

	getcontext(context_func);
	context_func->uc_link = 0;
	context_func->uc_stack.ss_sp = malloc(MEM_SIZE);
	if (context_func->uc_stack.ss_sp == NULL) {
		printf("\nmalloc failed, exiting...\n");
		exit(0);
	}
	context_func->uc_stack.ss_flags = 0;
	context_func->uc_stack.ss_size = MEM_SIZE;
	makecontext(context_func, (void *) start_funct, 1, (void *) args);

	container *Main = (container*) malloc(sizeof(container));
	if (Main == NULL) {
		printf("\nmalloc failed, exiting...\n");
		exit(0);
	}
	Main->id = ++THREAD_ID;
	Main->context = context_func;
	Main->prev = NULL;
	Main->next = NULL;
	Main->parent = NULL;
	Main->childhead = NULL;
	Main->childtail = NULL;
	Main->childprev = NULL;
	Main->childnext = NULL;
	Main->childCount = 0;

	Main->thischildjoinsparent = false
	;
	Main->thisparentjoinsall = false
	;

	executing_container = Main;

	swapcontext(&comeback, context_func);
}
MyThread MyThreadCreate(void (*start_funct)(void *), void *args) {
	ucontext_t* context_funct = (ucontext_t*) malloc(sizeof(ucontext_t));
	if (context_funct == NULL) {
		printf("\nmalloc failed, exiting...\n");
		exit(0);
	}

	getcontext(context_funct);
	context_funct->uc_link = 0;
	context_funct->uc_stack.ss_sp = malloc(MEM_SIZE);
	if (context_funct->uc_stack.ss_sp == NULL) {
		printf("\nmalloc failed, exiting...\n");
		exit(0);
	}
	context_funct->uc_stack.ss_flags = 0;
	context_funct->uc_stack.ss_size = MEM_SIZE;
	makecontext(context_funct, (void *) start_funct, 1, args);

	container *newAdd = (container*) malloc(sizeof(container));
	if (newAdd == NULL) {
		printf("\nmalloc failed, exiting...\n");
		exit(0);
	}

	newAdd->id = ++THREAD_ID;
	threadCount++;
	newAdd->context = context_funct;

	if (tail != NULL) {
		newAdd->prev = tail;
		tail->next = newAdd;
		tail = newAdd;
		tail->next = NULL;
	} else {
		head = newAdd;
		head->next = NULL;
		head->prev = NULL;
		tail = head;
	}
	newAdd->parent = executing_container;
	newAdd->parentId = executing_container->id;

	if (executing_container->childtail == NULL) {
		executing_container->childhead = newAdd;
		newAdd->childprev = NULL;
		newAdd->childnext = NULL;
		executing_container->childtail = executing_container->childhead;
	} else {
		newAdd->childprev = executing_container->childtail;
		executing_container->childtail->childnext = newAdd;
		executing_container->childtail = newAdd;
		executing_container->childtail->childnext = NULL;
	}
	executing_container->childCount++;
	newAdd->childhead = NULL;
	newAdd->childtail = NULL;

	newAdd->thischildjoinsparent = false
	;
	newAdd->thisparentjoinsall = false
	;

	return (MyThread) newAdd;
}
void MyThreadYield(void) {
	//patch start
	if(head==NULL && tail==NULL){
		return;
	}
	//patch end
	if (executing_container->id == 1) {
		joinoryield = true
		;
	}
	if (executing_container == head && executing_container != tail) {
		head = head->next;
		executing_container->next = NULL;
		head->prev = NULL;

		executing_container->prev = tail;
		tail->next = executing_container;
		tail = executing_container;
		tail->next = NULL;
		executing_container = head;
		swapcontext(tail->context, head->context);

	} else if (executing_container == head && executing_container == tail) {
		executing_container = head;
		swapcontext(executing_container->context, executing_container->context);
	} else if (executing_container != head && executing_container == tail) {
		executing_container = head;
		swapcontext(tail->context, head->context);
	} else {
		if (executing_container->prev != NULL)
			executing_container->prev->next = executing_container->next;
		if (executing_container->next != NULL)
			executing_container->next->prev = executing_container->prev;
		executing_container->prev = tail;
		tail->next = executing_container;
		tail = executing_container;
		tail->next = NULL;
		executing_container = head;
		swapcontext(tail->context, head->context);
	}
}
void blockContainer(container* executing_container) {
	if (blocktail != NULL) {

		executing_container->prev = blocktail;
		blocktail->next = executing_container;
		blocktail = executing_container;
		blocktail->next = NULL;
	} else {

		blockhead = executing_container;
		blockhead->next = NULL;
		blockhead->prev = NULL;
		blocktail = blockhead;
	}
}
void blockContainerOnSemaphore(semaphore* sem, container* executing_container) {
	if (sem->end != NULL) {

		executing_container->prev = sem->end;
		sem->end->next = executing_container;
		sem->end = executing_container;
		sem->end->next = NULL;
	} else {

		sem->start = executing_container;
		sem->start->next = NULL;
		sem->start->prev = NULL;
		sem->end = sem->start;
	}
}
void detachContainer(container* executing_container) {
	if (executing_container == head && executing_container != tail) {
		head = head->next;
		executing_container->next = NULL;
		head->prev = NULL;
		executing_container->prev = NULL;
	} else if (executing_container == head && executing_container == tail) {
	} else if (executing_container != head && executing_container == tail) {
		tail = executing_container->prev;
		tail->next = NULL;
	} else {
		if (executing_container->prev != NULL)
			executing_container->prev->next = executing_container->next;
		if (executing_container->next != NULL)
			executing_container->next->prev = executing_container->prev;
	}
}
void bringFromBlockToReady(container* cont) {
	if (cont != NULL) {
		if (cont == blockhead && cont != blocktail) {
			blockhead = blockhead->next;
			cont->next = NULL;
			blockhead->prev = NULL;
		} else if (cont == blockhead && cont == blocktail) {
			blockhead = blocktail = NULL;	//newly added
		} else if (cont != blockhead && cont == blocktail) {
			blocktail = cont->prev;
			blocktail->next = NULL;
			cont->prev = NULL;
		} else {
			if (cont->prev != NULL)
				cont->prev->next = cont->next;
			if (cont->next != NULL)
				cont->next->prev = cont->prev;

		}
		if (tail != NULL) {
			cont->prev = tail;
			tail->next = cont;
			tail = cont;
			tail->next = NULL;
		} else {
			head = cont;
			head->next = NULL;
			head->prev = NULL;
			tail = head;
		}
	}
}
void bringFromSemaphoreBlockToReady(semaphore* sem, container* cont) {
	if (cont != NULL) {
		if (cont == sem->start && cont != sem->end) {
			sem->start = sem->start->next;
			cont->next = NULL;
			sem->start->prev = NULL;
		} else if (cont == sem->start && cont == sem->end) {
			sem->start = sem->end = NULL;	//newly added
		} else if (cont != sem->start && cont == sem->end) {
			sem->end = cont->prev;
			sem->end->next = NULL;
			cont->prev = NULL;
		} else {
			if (cont->prev != NULL)
				cont->prev->next = cont->next;
			if (cont->next != NULL)
				cont->next->prev = cont->prev;

		}
		if (tail != NULL) {
			cont->prev = tail;
			tail->next = cont;
			tail = cont;
			tail->next = NULL;
		} else {
			head = cont;
			head->next = NULL;
			head->prev = NULL;
			tail = head;
		}
	}
}
int MyThreadJoin(MyThread thread) {
	container* cont = (container *) thread;
	if (executing_container->id == 1) {
		joinoryield = true
		;
	}
	if (cont->parentId != executing_container->id) {
		return -1;
	}
	if (cont != NULL) {
		cont->thischildjoinsparent = true
		;
		detachContainer(executing_container);
		blockContainer(executing_container);
		if (head != NULL) {
			executing_container = head;
			swapcontext(blocktail->context, head->context);
			return 0;
		} else {
			bringFromBlockToReady(executing_container);
			return 0;
		}

	} else {
		return -1;
	}
}
void MyThreadJoinAll(void) {

	unsigned long long int executingid = executing_container->id;
	if (executingid == 1) {
		joinoryield = true
		;
	}
	executing_container->thisparentjoinsall = true
	;

	container * cont = executing_container->childhead;
	if (cont != NULL) {
		detachContainer(executing_container);
		blockContainer(executing_container);
		if (head != NULL) {
			executing_container = head;
			swapcontext(blocktail->context, head->context);
		}
	}
}
void freeAndRearrange(container* freeThis) {
	if (!freed) {
		if (freeThis == head && freeThis != tail) {
			head = head->next;
			freeThis->next = NULL;
			head->prev = NULL;
			freeThis->prev = NULL;

		} else if (freeThis == head && freeThis == tail) {
			head = NULL;
			tail = NULL;
		} else if (freeThis != head && freeThis == tail) {
			tail = tail->prev;
			freeThis->prev = NULL;
			tail->next = NULL;
			freeThis->next = NULL;
		} else {
			if (freeThis->prev != NULL)
				freeThis->prev->next = freeThis->next;
			if (freeThis->next != NULL)
				freeThis->next->prev = freeThis->prev;

			freeThis->prev = NULL;
			freeThis->next = NULL;
		}
		if (freeThis->parent != NULL) {
			if (freeThis == freeThis->parent->childhead
					&& freeThis != freeThis->parent->childtail) {
				freeThis->parent->childhead =
						freeThis->parent->childhead->childnext;
				freeThis->childnext = NULL;
				freeThis->parent->childhead->childprev = NULL;
				freeThis->childprev = NULL;

			} else if (freeThis == freeThis->parent->childhead
					&& freeThis == freeThis->parent->childtail) {
				freeThis->parent->childhead = NULL;
				freeThis->parent->childtail = NULL;
			} else if (freeThis != freeThis->parent->childhead
					&& freeThis == freeThis->parent->childtail) {
				freeThis->parent->childtail =
						freeThis->parent->childtail->childprev;
				freeThis->childprev = NULL;
				freeThis->parent->childtail->childnext = NULL;
				freeThis->childnext = NULL;
			} else {
				if (freeThis->childprev != NULL)
					freeThis->childprev->childnext = freeThis->childnext;
				if (freeThis->childnext != NULL)
					freeThis->childnext->childprev = freeThis->childprev;

				freeThis->childprev = NULL;
				freeThis->childnext = NULL;
			}
		}
		free(freeThis->context);
		free(freeThis);
	}
}
void MyThreadExit(void) {
	threadCount--;

	executing_container->executed = true
	;
	container* freeThis = executing_container;
	if (joinoryield || 1) {
		container* cont = NULL;
		if (freeThis->thischildjoinsparent) {
			cont = freeThis->parent;
			bringFromBlockToReady(cont);
			freeAndRearrange(freeThis);

		} else if (freeThis->parent != NULL
				&& freeThis->parent->thisparentjoinsall) {
			freeThis->parent->childCount--;
			if (freeThis->parent->childCount == 0) {
				bringFromBlockToReady(freeThis->parent);

			}
			freeAndRearrange(freeThis);
		} else {
			freeAndRearrange(freeThis);
		}
		if (head != NULL) {
			executing_container = head;
			setcontext(head->context);
		} else {
			freed = 1;
			setcontext(&comeback);
		}
	} else {
		freed = 1;
		setcontext(&comeback);
	}

}
MySemaphore MySemaphoreInit(int initialValue) {
	if (initialValue < 0) {
		return NULL;
	}
	semaphore *mySemaphore = (semaphore*) malloc(sizeof(semaphore));
	if (mySemaphore == NULL) {
		printf("\nmalloc failed, exiting...\n");
		exit(0);
	}
	mySemaphore->ID = ++semaphore_id;
	mySemaphore->count = initialValue;
	mySemaphore->start = NULL;
	return (MySemaphore) mySemaphore;
}
void MySemaphoreSignal(MySemaphore sem) {
	semaphore *lock = (semaphore *) sem;
	lock->count++;
	bringFromSemaphoreBlockToReady(lock, lock->start);
}
void MySemaphoreWait(MySemaphore sem) {
	semaphore *lock = (semaphore *) sem;
	if (lock->count > 0) {
		lock->count--;
		return;
	}
	lock->count--;
	detachContainer(executing_container);
	blockContainerOnSemaphore(lock, executing_container);
	if (head != NULL) {
		executing_container = head;
		swapcontext(lock->end->context, head->context);
	} else {
	}
}
int MySemaphoreDestroy(MySemaphore sem) {
	semaphore *lock = (semaphore *) sem;
	if (lock->start != NULL) {
		//patch start
		//printf("semaphore not empty\n");
		//patch end
		return -1;
	} else {
		free(lock);
		return 0;
	}
}
