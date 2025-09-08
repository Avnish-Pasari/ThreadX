#define _GNU_SOURCE
#include "csc369_thread.h"
#include <malloc.h>
#include <stdlib.h>

#include <ucontext.h>

#include <assert.h>
#include <sys/time.h>

// TODO: You may find this useful, otherwise remove it
//#define DEBUG_USE_VALGRIND // uncomment to debug with valgrind
#ifdef DEBUG_USE_VALGRIND
#include <valgrind/valgrind.h>
#endif

#include "csc369_interrupts.h"

//****************************************************************************
// Private Definitions
//****************************************************************************
// TODO: You may find this useful, otherwise remove it
typedef enum
{
  CSC369_THREAD_FREE = 0,
  CSC369_THREAD_READY = 1,
  CSC369_THREAD_RUNNING = 2,
  CSC369_THREAD_ZOMBIE = 3,
  CSC369_THREAD_BLOCKED = 4
} CSC369_ThreadState;

/**
 * The Thread Control Block.
 */

// typedef struct
// {
//   // TODO: Populate this struct with other things you need

//   /**
//    * The thread context.
//    */
//   ucontext_t context; // TODO: You may find this useful, otherwise remove it

//   /**
//    * What code the thread exited with.
//    */
//   int exit_code; // TODO: You may find this useful, otherwise remove it

//   /**
//    * The queue of threads that are waiting on this thread to finish.
//    */
//   CSC369_WaitQueue* join_threads; // TODO: You may find this useful, otherwise remove it
// } TCB;




typedef struct
{
  Tid tid;             // The thread id
  int thread_exists;   // 0: does not exist; 1: exists;
  int is_main;         // 0: is not main; 1: is main;
  ucontext_t my_context;
  CSC369_WaitQueue *join_threads;
  void *my_stack_pointer;  
  int setcontext_called; // 0: not called; 1: called;
  int my_state;          // 0: ready; 1: running; -1: dead;
  int exit_code;
} TCB;





/**
 * A wait queue.
 */
typedef struct csc369_wait_queue_t
{
  TCB thread_queue[CSC369_MAX_THREADS];
} CSC369_WaitQueue;

//**************************************************************************************************
// Private Global Variables (Library State)
//**************************************************************************************************


// /**
//  * All possible threads have their control blocks stored contiguously in memory.
//  */
// TCB threads[CSC369_MAX_THREADS]; // TODO: you may find this useful, otherwise remove it

// /**
//  * Threads that are ready to run in FIFO order.
//  */
// CSC369_WaitQueue ready_threads; // TODO: you may find this useful, otherwise remove it

// /**
//  * Threads that need to be cleaned up.
//  */
// CSC369_WaitQueue zombie_threads; // TODO: you may find this useful, otherwise remove it



TCB storage_tracker[CSC369_MAX_THREADS];
TCB ready_queue[CSC369_MAX_THREADS];

TCB running;

CSC369_ThreadError csc369_ERROR_TID_INVALID = CSC369_ERROR_TID_INVALID;
CSC369_ThreadError csc369_ERROR_THREAD_BAD = CSC369_ERROR_THREAD_BAD;
CSC369_ThreadError csc369_ERROR_SYS_THREAD = CSC369_ERROR_SYS_THREAD;
CSC369_ThreadError csc369_ERROR_SYS_MEM = CSC369_ERROR_SYS_MEM;
CSC369_ThreadError csc369_ERROR_OTHER = CSC369_ERROR_OTHER;


//**************************************************************************************************
// Helper Functions
//**************************************************************************************************
// void // TODO: You may find this useful, otherwise remove it
// Queue_Init(CSC369_WaitQueue* queue)
// {
//   // FIXME
// }

// int // TODO: You may find this useful, otherwise remove it
// Queue_IsEmpty(CSC369_WaitQueue* queue)
// {
//   return 0; // FIXME
// }

// void // TODO: You may find this useful, otherwise remove it
// Queue_Enqueue(CSC369_WaitQueue* queue, TCB* tcb)
// {
//   // FIXME
// }

// TCB* // TODO: You may find this useful, otherwise remove it
// Queue_Dequeue(CSC369_WaitQueue* queue)
// {
//   return NULL; // FIXME
// }

// void // TODO: You may find this useful, otherwise remove it
// Queue_Remove(CSC369_WaitQueue* queue, TCB* tcb)
// {
//   // FIXME
// }

// void // TODO: You may find this useful, otherwise remove it
// TCB_Init(TCB* tcb, Tid thread_id)
// {
//   // FIXME
// }

// void // TODO: You may find this useful, otherwise remove it
// TCB_Free(TCB* tcb)
// {
//   // FIXME
// #ifdef DEBUG_USE_VALGRIND
//   // VALGRIND_STACK_DEREGISTER(...);
// #endif
// }

// // TODO: You may find it useful to create a helper function to create a context
// // TODO: You may find it useful to create a helper function to switch contexts


void MyThreadStub(void (*f)(void *), void *arg)
{
  CSC369_InterruptsEnable();
  f(arg);
  CSC369_ThreadExit(CSC369_EXIT_CODE_NORMAL);
}



// Assumption that the queue is not NULL
void Thread_Recursive_Kill(CSC369_WaitQueue* queue, int tid)
{
  // Assumption that the queue is not NULL
  if(queue->thread_queue[0].thread_exists == 0)
  {
    return;
  }

  // Else

  for(int w = 0; w < CSC369_MAX_THREADS; w++)
  {
    if(queue->thread_queue[w].thread_exists == 1 && queue->thread_queue[w].tid == tid)
    {
      queue->thread_queue[w].my_state = -1;
      queue->thread_queue[w].exit_code = CSC369_EXIT_CODE_KILL;

      TCB tempp;

      tempp.tid = queue->thread_queue[w].tid;
      tempp.thread_exists = queue->thread_queue[w].thread_exists;
      tempp.is_main = queue->thread_queue[w].is_main;
      tempp.my_context = queue->thread_queue[w].my_context;
      tempp.my_stack_pointer = queue->thread_queue[w].my_stack_pointer;
      tempp.setcontext_called = queue->thread_queue[w].setcontext_called;
      tempp.my_state = queue->thread_queue[w].my_state;
      tempp.join_threads = queue->thread_queue[w].join_threads;
      tempp.exit_code = queue->thread_queue[w].exit_code;

      // Shifting the wait queue
      for(int h = w; h < CSC369_MAX_THREADS - 1; h++)
      {
        queue->thread_queue[h].tid = queue->thread_queue[h+1].tid;
        queue->thread_queue[h].thread_exists = queue->thread_queue[h+1].thread_exists;
        queue->thread_queue[h].is_main = queue->thread_queue[h+1].is_main;
        queue->thread_queue[h].my_context = queue->thread_queue[h+1].my_context;
        queue->thread_queue[h].my_stack_pointer = queue->thread_queue[h+1].my_stack_pointer;
        queue->thread_queue[h].setcontext_called = queue->thread_queue[h+1].setcontext_called;
        queue->thread_queue[h].my_state = queue->thread_queue[h+1].my_state;
        queue->thread_queue[h].exit_code = queue->thread_queue[h+1].exit_code;
        queue->thread_queue[h].join_threads = queue->thread_queue[h+1].join_threads;
      }
      queue->thread_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

      // Finding an empty position in the ready queue

      for(int t = 0; t < CSC369_MAX_THREADS; t++)  
      {
        if(ready_queue[t].thread_exists == 0)
        {
          ready_queue[t].tid = tempp.tid;
          ready_queue[t].thread_exists = tempp.thread_exists;
          ready_queue[t].is_main = tempp.is_main;
          ready_queue[t].my_context = tempp.my_context;
          ready_queue[t].my_stack_pointer = tempp.my_stack_pointer;
          ready_queue[t].setcontext_called = tempp.setcontext_called;
          ready_queue[t].my_state = tempp.my_state;
          ready_queue[t].exit_code = tempp.exit_code;     
          ready_queue[t].join_threads = tempp.join_threads;
          break;
        }
      }

      break;

    }
    if(queue->thread_queue[w].thread_exists == 1 && queue->thread_queue[w].tid != tid)
    {
      Thread_Recursive_Kill(queue->thread_queue[w].join_threads, tid);
    }


  }

  return;

}




void my_clean()  
{ 
  // Disabling Interrupts
  int const prev_state = CSC369_InterruptsDisable();

  // Cleans up dead threads from both the storage and the queue
  // Assumption that there are never any dead threads in 'running' variable
  // Also frees malloc if applicable

  // Checking the ready array for any dead threads
  int i;
  int j;

  for(i = 0; i < CSC369_MAX_THREADS; i++)  
  {
    if(ready_queue[i].thread_exists == 1 && ready_queue[i].my_state == -1)
    {

      // Found a dead thread

      // Free the stack
      if(ready_queue[i].is_main == 0)
      {
        #ifdef DEBUG_USE_VALGRIND
        VALGRIND_STACK_DEREGISTER(storage_tracker[ready_queue[i].tid].my_stack_pointer);
        #endif
        free(storage_tracker[ready_queue[i].tid].my_stack_pointer);
      }

      // Schedule threads from the waiting queue to the ready queue
      while(ready_queue[i].join_threads->thread_queue[0].thread_exists != 0)
      {

        TCB temp;

        temp.tid = ready_queue[i].join_threads->thread_queue[0].tid;
        temp.thread_exists = ready_queue[i].join_threads->thread_queue[0].thread_exists;
        temp.is_main = ready_queue[i].join_threads->thread_queue[0].is_main;
        temp.my_context = ready_queue[i].join_threads->thread_queue[0].my_context;
        temp.my_stack_pointer = ready_queue[i].join_threads->thread_queue[0].my_stack_pointer;
        temp.setcontext_called = ready_queue[i].join_threads->thread_queue[0].setcontext_called;
        temp.my_state = ready_queue[i].join_threads->thread_queue[0].my_state;
        temp.exit_code = ready_queue[i].join_threads->thread_queue[0].exit_code;
        temp.join_threads = ready_queue[i].join_threads->thread_queue[0].join_threads;

        // Shifting the queue
        int k;
        for(k = 0; k < CSC369_MAX_THREADS - 1; k++) 
        {
          ready_queue[i].join_threads->thread_queue[k].tid = ready_queue[i].join_threads->thread_queue[k+1].tid;
          ready_queue[i].join_threads->thread_queue[k].thread_exists = ready_queue[i].join_threads->thread_queue[k+1].thread_exists;
          ready_queue[i].join_threads->thread_queue[k].is_main = ready_queue[i].join_threads->thread_queue[k+1].is_main;
          ready_queue[i].join_threads->thread_queue[k].my_context = ready_queue[i].join_threads->thread_queue[k+1].my_context;
          ready_queue[i].join_threads->thread_queue[k].my_stack_pointer = ready_queue[i].join_threads->thread_queue[k+1].my_stack_pointer;
          ready_queue[i].join_threads->thread_queue[k].setcontext_called = ready_queue[i].join_threads->thread_queue[k+1].setcontext_called;
          ready_queue[i].join_threads->thread_queue[k].my_state = ready_queue[i].join_threads->thread_queue[k+1].my_state;
          ready_queue[i].join_threads->thread_queue[k].exit_code = ready_queue[i].join_threads->thread_queue[k+1].exit_code;
          ready_queue[i].join_threads->thread_queue[k].join_threads = ready_queue[i].join_threads->thread_queue[k+1].join_threads;
        }

        ready_queue[i].join_threads->thread_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

        // Checking for empty position in the ready queue

        for(k = 0; k < CSC369_MAX_THREADS; k++)  
        {
          if(ready_queue[k].thread_exists == 0)
          {
            ready_queue[k].tid = temp.tid;
            ready_queue[k].thread_exists = temp.thread_exists;
            ready_queue[k].is_main = temp.is_main;
            ready_queue[k].my_context = temp.my_context;
            ready_queue[k].my_stack_pointer = temp.my_stack_pointer;
            ready_queue[k].setcontext_called = temp.setcontext_called;
            ready_queue[k].my_state = temp.my_state;
            ready_queue[k].exit_code = ready_queue[i].exit_code;     // Setting the exit code of the threads to the exit code of the "exited" thread they were waiting on
            ready_queue[k].join_threads = temp.join_threads;
            break;
          }
        }

      } 

      // Freeing the waiting queue
      CSC369_WaitQueueDestroy(ready_queue[i].join_threads);

      // Delete this from the storage_tracker
      storage_tracker[ready_queue[i].tid].thread_exists = 0;
      storage_tracker[ready_queue[i].tid].is_main = 0;
      storage_tracker[ready_queue[i].tid].my_stack_pointer = NULL;
      storage_tracker[ready_queue[i].tid].setcontext_called = 0;
      storage_tracker[ready_queue[i].tid].my_state = 0;
      storage_tracker[ready_queue[i].tid].exit_code = 0;
      storage_tracker[ready_queue[i].tid].join_threads = NULL;

      // Delete from the queue
      for(j = i; j < CSC369_MAX_THREADS - 1; j++) 
      {
        ready_queue[j].tid = ready_queue[j+1].tid;
        ready_queue[j].thread_exists = ready_queue[j+1].thread_exists;
        ready_queue[j].is_main = ready_queue[j+1].is_main;
        ready_queue[j].my_context = ready_queue[j+1].my_context;
        ready_queue[j].my_stack_pointer = ready_queue[j+1].my_stack_pointer;
        ready_queue[j].setcontext_called = ready_queue[j+1].setcontext_called;
        ready_queue[j].my_state = ready_queue[j+1].my_state;
        ready_queue[j].exit_code = ready_queue[j+1].exit_code;
        ready_queue[j].join_threads = ready_queue[j+1].join_threads;
      }

      ready_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

      i = 0;

    }
  }

  // Setting interrupts to the previous state
  CSC369_InterruptsSet(prev_state);  
}



//**************************************************************************************************
// thread.h Functions
//**************************************************************************************************
int
CSC369_ThreadInit(void)
{
      // Disabling interrupts
      int const prev_state = CSC369_InterruptsDisable();

      int i;

      // Default Initializing the storage_tracker
      for(i = 0; i < CSC369_MAX_THREADS; i++)
      {
        storage_tracker[i].tid = i;               // Setting the thread id - remains fixed
        storage_tracker[i].thread_exists = 0;     // Setting that the thread does not exist
        storage_tracker[i].is_main = 0;           // Setting it to NOT main
        storage_tracker[i].my_stack_pointer = NULL;
        storage_tracker[i].setcontext_called = 0;
        storage_tracker[i].my_state = 0;
        storage_tracker[i].exit_code = 0;
        storage_tracker[i].join_threads = NULL;
      }

      // Default Initializing the ready_queue
      for(i = 0; i < CSC369_MAX_THREADS; i++)
      {
        ready_queue[i].tid = i;                  // Setting the thread id
        ready_queue[i].thread_exists = 0;        // Setting that the thread does not exist
        ready_queue[i].is_main = 0;              // Setting it to NOT main
        ready_queue[i].my_stack_pointer = NULL;
        ready_queue[i].setcontext_called = 0;
        ready_queue[i].my_state = 0;
        ready_queue[i].exit_code = 0;
        ready_queue[i].join_threads = NULL;
      }

      // Default Initializing for running
      running.tid = 0;
      running.thread_exists = 0;
      running.is_main = 0;
      running.my_stack_pointer = NULL;
      running.setcontext_called = 0;
      running.my_state = 0;
      running.exit_code = 0;
      running.join_threads = NULL;
      



      // Initialization of the main thread

      running.tid = 0;
      running.thread_exists = 1;
      running.is_main = 1;
      running.my_stack_pointer = NULL;
      running.setcontext_called = 0;
      running.my_state = 1;
      running.exit_code = 0;
      running.join_threads = CSC369_WaitQueueCreate();
      int err = getcontext(&running.my_context);
      if(err != 0)
      {
        return csc369_ERROR_OTHER;
      }

      storage_tracker[0].thread_exists = 1;
      storage_tracker[0].is_main = 1;
      storage_tracker[0].my_stack_pointer = NULL;
      storage_tracker[0].setcontext_called = 0;
      storage_tracker[0].my_state = 1;
      storage_tracker[0].exit_code = 0;
      storage_tracker[0].join_threads = running.join_threads;
      int err2 = getcontext(&storage_tracker[0].my_context);
      if(err2 != 0)
      {
        return csc369_ERROR_OTHER;
      }

      // Setting interrupts to the previous state
      CSC369_InterruptsSet(prev_state);

      return 0;
}


Tid
CSC369_ThreadId(void)
{
  // Disabling interrupts
  int const prev_state = CSC369_InterruptsDisable(); 

  int temporary = running.tid;

  // Setting interrupts to the previous state
  CSC369_InterruptsSet(prev_state);  

  return temporary;
}

Tid
CSC369_ThreadCreate(void (*f)(void*), void* arg)
{

  // Disabling interrupts
  int const prev_state = CSC369_InterruptsDisable();


  // Cleaning up the dead threads from the system  

  my_clean();

  // Checking if no more threads can be created
  int i;
  int counter = 0;
  int index = -1;

  for(i = 0; i < CSC369_MAX_THREADS; i++)
  {
    if(storage_tracker[i].thread_exists == 1)
    {
      counter = counter + 1;
    }
  }

  if(counter == CSC369_MAX_THREADS)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state); 

    return csc369_ERROR_SYS_THREAD;
  }

  // Getting the index where the thread can be added
  for(i = 0; i < CSC369_MAX_THREADS; i++)
  {
    if(storage_tracker[i].thread_exists == 0)
    {
      index = i;
      break;
    }
  }



  // Creating the new thread

  storage_tracker[index].is_main = 0;
  storage_tracker[index].thread_exists = 1;
  storage_tracker[index].setcontext_called = 0;
  storage_tracker[index].my_state = 0;
  storage_tracker[index].exit_code = 0;
  storage_tracker[index].join_threads = CSC369_WaitQueueCreate();


  int err = getcontext(&storage_tracker[index].my_context);
  if(err != 0)
  {
    return csc369_ERROR_OTHER;
  }

  storage_tracker[index].my_context.uc_mcontext.gregs[REG_RIP] = (long long int) &MyThreadStub;

  storage_tracker[index].my_context.uc_mcontext.gregs[REG_RDI] = (long long int) f;

  storage_tracker[index].my_context.uc_mcontext.gregs[REG_RSI] = (long long int) arg;



  // mallocing the stack size

  // Allocating extra space for malloc

  void *checker = malloc(CSC369_THREAD_STACK_SIZE); 
  if(checker == NULL)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return csc369_ERROR_SYS_MEM;
  }

  storage_tracker[index].my_stack_pointer = checker;

  #ifdef DEBUG_USE_VALGRIND
  VALGRIND_STACK_REGISTER(checker, checker + CSC369_THREAD_STACK_SIZE);
  #endif
  

  storage_tracker[index].my_context.uc_mcontext.gregs[REG_RSP] = (long long int) checker;

  // Inverting the stack

  // Only adding 110 extra and not 200 to make sure that you are safely on the stack space allocated by malloc - Inverting the stack
  storage_tracker[index].my_context.uc_mcontext.gregs[REG_RSP] = storage_tracker[index].my_context.uc_mcontext.gregs[REG_RSP] + CSC369_THREAD_STACK_SIZE;


  // Aligning the stack pointer to be aligned to 16 bytes

  while(storage_tracker[index].my_context.uc_mcontext.gregs[REG_RSP] % 16 != 0)
  {
    storage_tracker[index].my_context.uc_mcontext.gregs[REG_RSP] = storage_tracker[index].my_context.uc_mcontext.gregs[REG_RSP] - 1;
  }

  // Making a provision for an 8 byte push

  storage_tracker[index].my_context.uc_mcontext.gregs[REG_RSP] = storage_tracker[index].my_context.uc_mcontext.gregs[REG_RSP] - 8;

  // After the thread is created, it needs to be added to the queue

  for(i = 0; i < CSC369_MAX_THREADS; i++)
  {
    if(ready_queue[i].thread_exists == 0)
    {
      ready_queue[i].tid = index;
      ready_queue[i].thread_exists = 1;
      ready_queue[i].is_main = 0;
      ready_queue[i].my_context = storage_tracker[index].my_context;
      ready_queue[i].my_stack_pointer = storage_tracker[index].my_stack_pointer;
      ready_queue[i].setcontext_called = storage_tracker[index].setcontext_called;
      ready_queue[i].my_state = 0;
      ready_queue[i].exit_code = 0;
      ready_queue[i].join_threads = storage_tracker[index].join_threads;
      break;
    }
  }

  // Setting interrupts to the previous state
  CSC369_InterruptsSet(prev_state);  

  return index;
}

void
CSC369_ThreadExit(int exit_code)
{
  // Disabling the interrupts
  int const prev_state = CSC369_InterruptsDisable();

  // Cleaning up any dead threads
  my_clean();

  // Checking if this is the last thread in the system
  if(ready_queue[0].thread_exists == 0 && running.join_threads->thread_queue[0].thread_exists == 0) 
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    exit(exit_code);
  }




  // Modified added code

  for(int m = 0; m < CSC369_MAX_THREADS; m++)
  {
    running.join_threads->thread_queue[m].exit_code = exit_code;
  }

  CSC369_ThreadWakeAll(running.join_threads);

  assert(running.join_threads->thread_queue[0].thread_exists == 0);

  // End of Modified added code


  // If this is not the last thread in the system, then
  TCB temp;

  temp.tid = running.tid;
  temp.thread_exists = running.thread_exists;
  temp.is_main = running.is_main;
  temp.my_context = running.my_context;
  temp.my_stack_pointer = running.my_stack_pointer;
  temp.setcontext_called = running.setcontext_called;
  temp.my_state = running.my_state;
  temp.join_threads = running.join_threads;
  temp.exit_code = running.exit_code;

  running.tid = ready_queue[0].tid;
  running.thread_exists = ready_queue[0].thread_exists;
  running.is_main = ready_queue[0].is_main;
  running.my_context = ready_queue[0].my_context;
  running.my_stack_pointer = ready_queue[0].my_stack_pointer;
  running.setcontext_called = ready_queue[0].setcontext_called;
  running.join_threads = ready_queue[0].join_threads;
  running.exit_code = ready_queue[0].exit_code;
  running.my_state = 1;

  // Shifting the queue
  int i;
  for(i = 0; i < CSC369_MAX_THREADS - 1; i++)
  {
    ready_queue[i].tid = ready_queue[i+1].tid;
    ready_queue[i].thread_exists = ready_queue[i+1].thread_exists;
    ready_queue[i].is_main = ready_queue[i+1].is_main;
    ready_queue[i].my_context = ready_queue[i+1].my_context;
    ready_queue[i].my_stack_pointer = ready_queue[i+1].my_stack_pointer;
    ready_queue[i].setcontext_called = ready_queue[i+1].setcontext_called;
    ready_queue[i].my_state = ready_queue[i+1].my_state;
    ready_queue[i].join_threads = ready_queue[i+1].join_threads;
    ready_queue[i].exit_code = ready_queue[i+1].exit_code;
  }

  ready_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

  // Checking for empty position in the queue
  int index;

  for(i = 0; i < CSC369_MAX_THREADS; i++)
  {
    if(ready_queue[i].thread_exists == 0)
    {
      index = i;
      break;
    }
  }

  ready_queue[index].tid = temp.tid;
  ready_queue[index].thread_exists = 1;
  ready_queue[index].is_main = temp.is_main;
  ready_queue[index].my_stack_pointer = temp.my_stack_pointer;
  ready_queue[index].setcontext_called = temp.setcontext_called;
  ready_queue[index].join_threads = temp.join_threads;
  ready_queue[index].exit_code = exit_code;        // Setting the exit code of a thread that exited normally
  ready_queue[index].my_state = -1;                              // Turning the exited thread into a zombie


  storage_tracker[running.tid].setcontext_called = 1;
  int err4 = setcontext(&storage_tracker[running.tid].my_context);
  if(err4 == -1)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    exit(-1);
  }

  // Note - the code will never reach here!!!!
  // Setting interrupts to the previous state
  CSC369_InterruptsSet(prev_state);  
}






Tid
CSC369_ThreadKill(Tid tid)
{
  // Disabling Interrupts
  int const prev_state = CSC369_InterruptsDisable();

  // Cleaning up dead threads
  my_clean();

  int checker = tid;

  // Checking if the identifier is valid or not
  if(checker < 0 || checker >= CSC369_MAX_THREADS)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return csc369_ERROR_TID_INVALID;
  }

  // Checking if the identifier is of the calling thread
  if(running.tid == tid)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return csc369_ERROR_THREAD_BAD;
  }

  // Checking if the identifier is valid but the thread does not exist
  if(storage_tracker[checker].thread_exists == 0)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return csc369_ERROR_SYS_THREAD;
  }

  // Checking for the thread on the ready queue
  int i;
  int flagg = 0;

  for(i = 0; i < CSC369_MAX_THREADS; i++)
  {
    if(ready_queue[i].thread_exists == 1 && ready_queue[i].tid == tid)
    {
      flagg = 1;
      ready_queue[i].my_state = -1;                      // Turning the thread into a zombie.
      ready_queue[i].exit_code = CSC369_EXIT_CODE_KILL;  // Setting the error code for the zombie thread.
      break;
    }
  }

  // Modified Code
  // If flagg is 0, this means that the thread which needs to be killed exists but is
  // not on the ready queue
  if(flagg == 0)
  {

    // Checking the wait queue of running
    Thread_Recursive_Kill(running.join_threads, tid);

    // for(int w = 0; w < CSC369_MAX_THREADS; w++)
    // {
    //   if(running.join_threads->thread_queue[w].thread_exists == 1 && running.join_threads->thread_queue[w].tid == tid)
    //   {
    //     running.join_threads->thread_queue[w].my_state = -1;
    //     running.join_threads->thread_queue[w].exit_code = CSC369_EXIT_CODE_KILL;

    //     TCB tempp;

    //     tempp.tid = running.join_threads->thread_queue[w].tid;
    //     tempp.thread_exists = running.join_threads->thread_queue[w].thread_exists;
    //     tempp.is_main = running.join_threads->thread_queue[w].is_main;
    //     tempp.my_context = running.join_threads->thread_queue[w].my_context;
    //     tempp.my_stack_pointer = running.join_threads->thread_queue[w].my_stack_pointer;
    //     tempp.setcontext_called = running.join_threads->thread_queue[w].setcontext_called;
    //     tempp.my_state = running.join_threads->thread_queue[w].my_state;
    //     tempp.join_threads = running.join_threads->thread_queue[w].join_threads;
    //     tempp.exit_code = running.join_threads->thread_queue[w].exit_code;

    //     // Shifting the wait queue
    //     for(int h = w; h < CSC369_MAX_THREADS - 1; h++)
    //     {
    //       running.join_threads->thread_queue[h].tid = running.join_threads->thread_queue[h+1].tid;
    //       running.join_threads->thread_queue[h].thread_exists = running.join_threads->thread_queue[h+1].thread_exists;
    //       running.join_threads->thread_queue[h].is_main = running.join_threads->thread_queue[h+1].is_main;
    //       running.join_threads->thread_queue[h].my_context = running.join_threads->thread_queue[h+1].my_context;
    //       running.join_threads->thread_queue[h].my_stack_pointer = running.join_threads->thread_queue[h+1].my_stack_pointer;
    //       running.join_threads->thread_queue[h].setcontext_called = running.join_threads->thread_queue[h+1].setcontext_called;
    //       running.join_threads->thread_queue[h].my_state = running.join_threads->thread_queue[h+1].my_state;
    //       running.join_threads->thread_queue[h].exit_code = running.join_threads->thread_queue[h+1].exit_code;
    //       running.join_threads->thread_queue[h].join_threads = running.join_threads->thread_queue[h+1].join_threads;
    //     }
    //     running.join_threads->thread_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

    //     // Finding an empty position in the ready queue

    //     for(int t = 0; t < CSC369_MAX_THREADS; t++)  
    //     {
    //       if(ready_queue[t].thread_exists == 0)
    //       {
    //         ready_queue[t].tid = tempp.tid;
    //         ready_queue[t].thread_exists = tempp.thread_exists;
    //         ready_queue[t].is_main = tempp.is_main;
    //         ready_queue[t].my_context = tempp.my_context;
    //         ready_queue[t].my_stack_pointer = tempp.my_stack_pointer;
    //         ready_queue[t].setcontext_called = tempp.setcontext_called;
    //         ready_queue[t].my_state = tempp.my_state;
    //         ready_queue[t].exit_code = tempp.exit_code;     
    //         ready_queue[t].join_threads = tempp.join_threads;
    //         break;
    //       }
    //     }

    //     break;

    //   }

    // }


    // Checking the wait queues of each thread in the ready_queue
    for(int m = 0; m < CSC369_MAX_THREADS; m++)
    {
      if(ready_queue[m].thread_exists == 1)
      {

        Thread_Recursive_Kill(ready_queue[m].join_threads, tid);

        // Checking the wait queue of each thread
        // for(int w = 0; w < CSC369_MAX_THREADS; w++)
        // {
        //   if(ready_queue[m].join_threads->thread_queue[w].thread_exists == 1 && ready_queue[m].join_threads->thread_queue[w].tid == tid)
        //   {
        //     ready_queue[m].join_threads->thread_queue[w].my_state = -1;
        //     ready_queue[m].join_threads->thread_queue[w].exit_code = CSC369_EXIT_CODE_KILL;

        //     TCB tempp;

        //     tempp.tid = ready_queue[m].join_threads->thread_queue[w].tid;
        //     tempp.thread_exists = ready_queue[m].join_threads->thread_queue[w].thread_exists;
        //     tempp.is_main = ready_queue[m].join_threads->thread_queue[w].is_main;
        //     tempp.my_context = ready_queue[m].join_threads->thread_queue[w].my_context;
        //     tempp.my_stack_pointer = ready_queue[m].join_threads->thread_queue[w].my_stack_pointer;
        //     tempp.setcontext_called = ready_queue[m].join_threads->thread_queue[w].setcontext_called;
        //     tempp.my_state = ready_queue[m].join_threads->thread_queue[w].my_state;
        //     tempp.join_threads = ready_queue[m].join_threads->thread_queue[w].join_threads;
        //     tempp.exit_code = ready_queue[m].join_threads->thread_queue[w].exit_code;

        //     // Shifting the wait queue
        //     for(int h = w; h < CSC369_MAX_THREADS - 1; h++)
        //     {
        //       ready_queue[m].join_threads->thread_queue[h].tid = ready_queue[m].join_threads->thread_queue[h+1].tid;
        //       ready_queue[m].join_threads->thread_queue[h].thread_exists = ready_queue[m].join_threads->thread_queue[h+1].thread_exists;
        //       ready_queue[m].join_threads->thread_queue[h].is_main = ready_queue[m].join_threads->thread_queue[h+1].is_main;
        //       ready_queue[m].join_threads->thread_queue[h].my_context = ready_queue[m].join_threads->thread_queue[h+1].my_context;
        //       ready_queue[m].join_threads->thread_queue[h].my_stack_pointer = ready_queue[m].join_threads->thread_queue[h+1].my_stack_pointer;
        //       ready_queue[m].join_threads->thread_queue[h].setcontext_called = ready_queue[m].join_threads->thread_queue[h+1].setcontext_called;
        //       ready_queue[m].join_threads->thread_queue[h].my_state = ready_queue[m].join_threads->thread_queue[h+1].my_state;
        //       ready_queue[m].join_threads->thread_queue[h].exit_code = ready_queue[m].join_threads->thread_queue[h+1].exit_code;
        //       ready_queue[m].join_threads->thread_queue[h].join_threads = ready_queue[m].join_threads->thread_queue[h+1].join_threads;
        //     }
        //     ready_queue[m].join_threads->thread_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

        //     // Finding an empty position in the ready queue

        //     for(int t = 0; t < CSC369_MAX_THREADS; t++)  
        //     {
        //       if(ready_queue[t].thread_exists == 0)
        //       {
        //         ready_queue[t].tid = tempp.tid;
        //         ready_queue[t].thread_exists = tempp.thread_exists;
        //         ready_queue[t].is_main = tempp.is_main;
        //         ready_queue[t].my_context = tempp.my_context;
        //         ready_queue[t].my_stack_pointer = tempp.my_stack_pointer;
        //         ready_queue[t].setcontext_called = tempp.setcontext_called;
        //         ready_queue[t].my_state = tempp.my_state;
        //         ready_queue[t].exit_code = tempp.exit_code;     
        //         ready_queue[t].join_threads = tempp.join_threads;
        //         break;
        //       }
        //     }

        //     break;

        //   }

        // }
      }
      

    }

  }



  my_clean();

  // Setting interrupts to the previous state
  CSC369_InterruptsSet(prev_state);  

  return tid;

}

int
CSC369_ThreadYield()
{

  // Disabling Interrupts
  int const prev_state = CSC369_InterruptsDisable();
  
  // Cleaning up all the dead threads before yielding

  my_clean();

  // Checking if there are any threads in the ready queue
  // If not, yield to self
  if(ready_queue[0].thread_exists == 0)
  {
    int temporary = running.tid;

    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    // keep on running
    return temporary;
  }

  // Else

  TCB temp;

  temp.tid = running.tid;
  temp.thread_exists = running.thread_exists;
  temp.is_main = running.is_main;
  temp.my_context = running.my_context;
  temp.my_stack_pointer = running.my_stack_pointer;
  temp.setcontext_called = running.setcontext_called;
  temp.my_state = running.my_state;
  temp.exit_code = running.exit_code;
  temp.join_threads = running.join_threads;

  running.tid = ready_queue[0].tid;
  running.thread_exists = ready_queue[0].thread_exists;
  running.is_main = ready_queue[0].is_main;
  running.my_context = ready_queue[0].my_context;
  running.my_stack_pointer = ready_queue[0].my_stack_pointer;
  running.setcontext_called = ready_queue[0].setcontext_called;
  running.exit_code = ready_queue[0].exit_code;
  running.join_threads = ready_queue[0].join_threads;
  running.my_state = 1;

  // Shifting the queue
  int i;
  for(i = 0; i < CSC369_MAX_THREADS - 1; i++) 
  {
    ready_queue[i].tid = ready_queue[i+1].tid;
    ready_queue[i].thread_exists = ready_queue[i+1].thread_exists;
    ready_queue[i].is_main = ready_queue[i+1].is_main;
    ready_queue[i].my_context = ready_queue[i+1].my_context;
    ready_queue[i].my_stack_pointer = ready_queue[i+1].my_stack_pointer;
    ready_queue[i].setcontext_called = ready_queue[i+1].setcontext_called;
    ready_queue[i].my_state = ready_queue[i+1].my_state;
    ready_queue[i].exit_code = ready_queue[i+1].exit_code;
    ready_queue[i].join_threads = ready_queue[i+1].join_threads;
  }

  ready_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

  // Checking for empty position in the queue
  int index;

  for(i = 0; i < CSC369_MAX_THREADS; i++)  
  {
    if(ready_queue[i].thread_exists == 0)
    {
      index = i;
      break;
    }
  }

  ready_queue[index].tid = temp.tid;
  ready_queue[index].thread_exists = temp.thread_exists;
  ready_queue[index].is_main = temp.is_main;
  ready_queue[index].my_stack_pointer = temp.my_stack_pointer;
  ready_queue[index].setcontext_called = temp.setcontext_called;
  ready_queue[index].exit_code = temp.exit_code;
  ready_queue[index].join_threads = temp.join_threads;
  ready_queue[index].my_state = 0;

  int temp_id = ready_queue[index].tid;
  int running_id = running.tid;

  storage_tracker[running.tid].setcontext_called = 0;

  // Getting the current context for the current thread
  getcontext(&storage_tracker[ready_queue[index].tid].my_context);

  if(storage_tracker[running.tid].setcontext_called == 1) 
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    // storage_tracker[running_id].setcontext_called = 0;  
    return running_id;                                   
  }

  // Setting the context to the new thread
  storage_tracker[running.tid].setcontext_called = 1;
  setcontext(&storage_tracker[running.tid].my_context);

}


int
CSC369_ThreadYieldTo(Tid tid)
{
  // Disabling interrupts
  int const prev_state = CSC369_InterruptsDisable();
  
  // Cleaning up the dead threads before yielding

  my_clean();

  // Checking if the identifier is invalid

  int check = tid;
  if(check < 0 || check >= CSC369_MAX_THREADS)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return csc369_ERROR_TID_INVALID;
  }

  // Checking if the identifier is valid, but a thread with that identifier does not
  // exist
  if(storage_tracker[check].thread_exists == 0)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return csc369_ERROR_THREAD_BAD;
  }

  // Checking if the tid is of the current/running thread itself
  // Then yield to self
  if(running.tid == tid)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return tid;     
  }





  // Modified code
  // Checking if the identifier is valid and the thread exists but the thread
  // Is not on the ready queue i.e. it is waiting for some other thread to complete
  int flagg = 0;
  for(int g = 0; g < CSC369_MAX_THREADS; g++)
  {
    if(ready_queue[g].thread_exists == 1 && ready_queue[g].tid == tid)
    {
      flagg = 1;
      break;
    }
  }
  // Detected that the thread to yield to is on a wait queue (waiting for some other thread to 
  // finish) and thus not on the ready_queue
  if(flagg == 0)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return csc369_ERROR_THREAD_BAD;
  }
  // End of modified code







  // Else
  // Find the index of that identifier in the ready queue
  int i;
  int index;

  for(i = 0; i < CSC369_MAX_THREADS; i++)
  {
    if(ready_queue[i].thread_exists == 1 && ready_queue[i].tid == tid)
    {
      index = i;
      break;
    }
  }

  TCB temp;

  temp.tid = running.tid;
  temp.thread_exists = running.thread_exists;
  temp.is_main = running.is_main;
  temp.my_context = running.my_context;
  temp.my_stack_pointer = running.my_stack_pointer;
  temp.setcontext_called = running.setcontext_called;
  temp.my_state = running.my_state;
  temp.exit_code = running.exit_code;
  temp.join_threads = running.join_threads;

  running.tid = ready_queue[index].tid;
  running.thread_exists = ready_queue[index].thread_exists;
  running.is_main = ready_queue[index].is_main;
  running.my_context = ready_queue[index].my_context;
  running.my_stack_pointer = ready_queue[index].my_stack_pointer;
  running.setcontext_called = ready_queue[index].setcontext_called;
  running.exit_code = ready_queue[index].exit_code;
  running.join_threads = ready_queue[index].join_threads;
  running.my_state = 1;

  // Shifting the queue

  for(i = index; i < CSC369_MAX_THREADS - 1; i++)
  {
    ready_queue[i].tid = ready_queue[i+1].tid;
    ready_queue[i].thread_exists = ready_queue[i+1].thread_exists;
    ready_queue[i].is_main = ready_queue[i+1].is_main;
    ready_queue[i].my_context = ready_queue[i+1].my_context;
    ready_queue[i].my_stack_pointer = ready_queue[i+1].my_stack_pointer;
    ready_queue[i].setcontext_called = ready_queue[i+1].setcontext_called;
    ready_queue[i].my_state = ready_queue[i+1].my_state;
    ready_queue[i].exit_code = ready_queue[i+1].exit_code;
    ready_queue[i].join_threads = ready_queue[i+1].join_threads;
  }

  ready_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

  // Checking for empty position in the queue
  int index1;

  for(i = 0; i < CSC369_MAX_THREADS; i++)
  {
    if(ready_queue[i].thread_exists == 0)
    {
      index1 = i;
      break;
    }
  }

  ready_queue[index1].tid = temp.tid;
  ready_queue[index1].thread_exists = temp.thread_exists;
  ready_queue[index1].is_main = temp.is_main;
  ready_queue[index1].my_stack_pointer = temp.my_stack_pointer;
  ready_queue[index1].setcontext_called = temp.setcontext_called;
  ready_queue[index1].exit_code = temp.exit_code;
  ready_queue[index1].join_threads = temp.join_threads;
  ready_queue[index1].my_state = 0;

  int temp_id = ready_queue[index1].tid;
  int running_id = running.tid;

  storage_tracker[running.tid].setcontext_called = 0;

  // Getting the context of the current thread
  getcontext(&storage_tracker[ready_queue[index1].tid].my_context);

  if(storage_tracker[running.tid].setcontext_called == 1)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    //storage_tracker[running_id].setcontext_called = 0;
    return running_id;                                   
  }

  // Setting the context to the new thread
  storage_tracker[running.tid].setcontext_called = 1;
  setcontext(&storage_tracker[running.tid].my_context); 

}




//****************************************************************************
// New Assignment 2 Definitions - Task 2
//****************************************************************************
CSC369_WaitQueue*
CSC369_WaitQueueCreate(void)
{

  // Disabling Interrupts
  int const prev_state = CSC369_InterruptsDisable();

  CSC369_WaitQueue* checker = malloc(sizeof(CSC369_WaitQueue));
  if(checker == NULL)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return NULL;
  }
  #ifdef DEBUG_USE_VALGRIND
  VALGRIND_STACK_REGISTER(checker, checker + sizeof(CSC369_WaitQueue));
  #endif

  // Initializing empty/default queue
  int i;
  for(i = 0; i < CSC369_MAX_THREADS; i++)
  {
    checker->thread_queue[i].tid = -1;
    checker->thread_queue[i].thread_exists = 0;
    checker->thread_queue[i].is_main = 0;
    checker->thread_queue[i].my_stack_pointer = NULL;
    checker->thread_queue[i].setcontext_called = 0;
    checker->thread_queue[i].my_state = 0;
    checker->thread_queue[i].join_threads = NULL;
    checker->thread_queue[i].exit_code = 0;
  }

  // Setting interrupts to the previous state
  CSC369_InterruptsSet(prev_state);  

  return checker;

}

int
CSC369_WaitQueueDestroy(CSC369_WaitQueue* queue)
{
  // Disabling Interrupts
  int const prev_state = CSC369_InterruptsDisable();

  if(queue->thread_queue[0].thread_exists == 1)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return CSC369_ERROR_OTHER;
  }

  #ifdef DEBUG_USE_VALGRIND
  VALGRIND_STACK_DEREGISTER(queue);
  #endif

  free(queue);

  // Setting interrupts to the previous state
  CSC369_InterruptsSet(prev_state);  

  return 0;

}

void
CSC369_ThreadSpin(int duration)
{
  struct timeval start, end, diff;

  int ret = gettimeofday(&start, NULL);
  assert(!ret);

  while (1) {
    ret = gettimeofday(&end, NULL);
    assert(!ret);
    timersub(&end, &start, &diff);

    if ((diff.tv_sec * 1000000 + diff.tv_usec) >= duration) {
      return;
    }
  }
}

int
CSC369_ThreadSleep(CSC369_WaitQueue* queue)
{

  // Disabling Interrupts
  int const prev_state = CSC369_InterruptsDisable();
  
  assert(queue != NULL);

  // Cleaning up all the dead threads before yielding
  // my_clean();

  // Checking if there are no other ready threads to be run
  if(ready_queue[0].thread_exists == 0)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return CSC369_ERROR_SYS_THREAD;
  }

  TCB temp;

  temp.tid = running.tid;
  temp.thread_exists = running.thread_exists;
  temp.is_main = running.is_main;
  temp.my_context = running.my_context;
  temp.my_stack_pointer = running.my_stack_pointer;
  temp.setcontext_called = running.setcontext_called;
  temp.my_state = running.my_state;
  temp.exit_code = running.exit_code;
  temp.join_threads = running.join_threads;

  running.tid = ready_queue[0].tid;
  running.thread_exists = ready_queue[0].thread_exists;
  running.is_main = ready_queue[0].is_main;
  running.my_context = ready_queue[0].my_context;
  running.my_stack_pointer = ready_queue[0].my_stack_pointer;
  running.setcontext_called = ready_queue[0].setcontext_called;
  running.exit_code = ready_queue[0].exit_code;
  running.join_threads = ready_queue[0].join_threads;
  running.my_state = 1;

  // Shifting the queue
  int i;
  for(i = 0; i < CSC369_MAX_THREADS - 1; i++) 
  {
    ready_queue[i].tid = ready_queue[i+1].tid;
    ready_queue[i].thread_exists = ready_queue[i+1].thread_exists;
    ready_queue[i].is_main = ready_queue[i+1].is_main;
    ready_queue[i].my_context = ready_queue[i+1].my_context;
    ready_queue[i].my_stack_pointer = ready_queue[i+1].my_stack_pointer;
    ready_queue[i].setcontext_called = ready_queue[i+1].setcontext_called;
    ready_queue[i].my_state = ready_queue[i+1].my_state;
    ready_queue[i].exit_code = ready_queue[i+1].exit_code;
    ready_queue[i].join_threads = ready_queue[i+1].join_threads;
  }

  ready_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

  // Checking for empty position in the queue
  int index;

  for(i = 0; i < CSC369_MAX_THREADS; i++)  
  {
    if(queue->thread_queue[i].thread_exists == 0)
    {
      index = i;
      break;
    }
  }

  queue->thread_queue[index].tid = temp.tid;
  queue->thread_queue[index].thread_exists = temp.thread_exists;
  queue->thread_queue[index].is_main = temp.is_main;
  queue->thread_queue[index].my_stack_pointer = temp.my_stack_pointer;
  queue->thread_queue[index].setcontext_called = temp.setcontext_called;
  queue->thread_queue[index].exit_code = temp.exit_code;
  queue->thread_queue[index].join_threads = temp.join_threads;
  queue->thread_queue[index].my_state = 0;


  // int temp_id = ready_queue[index].tid;
  int running_id = running.tid;

  storage_tracker[running.tid].setcontext_called = 0;

  // Getting the current context for the current thread
  getcontext(&storage_tracker[queue->thread_queue[index].tid].my_context);

  if(storage_tracker[running.tid].setcontext_called == 1) 
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return running_id;                                   
  }

  // Setting the context to the new thread
  storage_tracker[running.tid].setcontext_called = 1;
  setcontext(&storage_tracker[running.tid].my_context);

}

int
CSC369_ThreadWakeNext(CSC369_WaitQueue* queue)
{

  // Disabling Interrupts
  int const prev_state = CSC369_InterruptsDisable();

  assert(queue != NULL);

  // Cleaning up all the dead threads before yielding
  // my_clean();
  
  // Checking if the queue is empty
  // Then the number of woken up threads is 0
  if(queue->thread_queue[0].thread_exists == 0)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return 0;
  }

  TCB temp;

  temp.tid = queue->thread_queue[0].tid;
  temp.thread_exists = queue->thread_queue[0].thread_exists;
  temp.is_main = queue->thread_queue[0].is_main;
  temp.my_context = queue->thread_queue[0].my_context;
  temp.my_stack_pointer = queue->thread_queue[0].my_stack_pointer;
  temp.setcontext_called = queue->thread_queue[0].setcontext_called;
  temp.my_state = queue->thread_queue[0].my_state;
  temp.exit_code = queue->thread_queue[0].exit_code;
  temp.join_threads = queue->thread_queue[0].join_threads;

  // Shifting the queue
  int i;
  for(i = 0; i < CSC369_MAX_THREADS - 1; i++) 
  {
    queue->thread_queue[i].tid = queue->thread_queue[i+1].tid;
    queue->thread_queue[i].thread_exists = queue->thread_queue[i+1].thread_exists;
    queue->thread_queue[i].is_main = queue->thread_queue[i+1].is_main;
    queue->thread_queue[i].my_context = queue->thread_queue[i+1].my_context;
    queue->thread_queue[i].my_stack_pointer = queue->thread_queue[i+1].my_stack_pointer;
    queue->thread_queue[i].setcontext_called = queue->thread_queue[i+1].setcontext_called;
    queue->thread_queue[i].my_state = queue->thread_queue[i+1].my_state;
    queue->thread_queue[i].exit_code = queue->thread_queue[i+1].exit_code;
    queue->thread_queue[i].join_threads = queue->thread_queue[i+1].join_threads;
  }

  queue->thread_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

  // Checking for empty position in the ready queue

  for(i = 0; i < CSC369_MAX_THREADS; i++)  
  {
    if(ready_queue[i].thread_exists == 0)
    {
      ready_queue[i].tid = temp.tid;
      ready_queue[i].thread_exists = temp.thread_exists;
      ready_queue[i].is_main = temp.is_main;
      ready_queue[i].my_context = temp.my_context;
      ready_queue[i].my_stack_pointer = temp.my_stack_pointer;
      ready_queue[i].setcontext_called = temp.setcontext_called;
      ready_queue[i].my_state = temp.my_state;
      ready_queue[i].exit_code = temp.exit_code;
      ready_queue[i].join_threads = temp.join_threads;
      break;
    }
  }

  // Setting interrupts to the previous state
  CSC369_InterruptsSet(prev_state);  

  return 1;

}

int
CSC369_ThreadWakeAll(CSC369_WaitQueue* queue)
{

  // Disabling Interrupts
  int const prev_state = CSC369_InterruptsDisable();

  assert(queue != NULL);

  // Cleaning up all the dead threads before yielding
  // my_clean();
  
  // Checking if the queue is empty
  // Then the number of woken up threads is 0
  if(queue->thread_queue[0].thread_exists == 0)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return 0;
  }

  // To keep track of the number of threads woken up
  int counter = 0;


  while(queue->thread_queue[0].thread_exists != 0)
  {

    // Incrementing the counter
    counter = counter + 1;

    TCB temp;

    temp.tid = queue->thread_queue[0].tid;
    temp.thread_exists = queue->thread_queue[0].thread_exists;
    temp.is_main = queue->thread_queue[0].is_main;
    temp.my_context = queue->thread_queue[0].my_context;
    temp.my_stack_pointer = queue->thread_queue[0].my_stack_pointer;
    temp.setcontext_called = queue->thread_queue[0].setcontext_called;
    temp.my_state = queue->thread_queue[0].my_state;
    temp.exit_code = queue->thread_queue[0].exit_code;
    temp.join_threads = queue->thread_queue[0].join_threads;

    // Shifting the queue
    int i;
    for(i = 0; i < CSC369_MAX_THREADS - 1; i++) 
    {
      queue->thread_queue[i].tid = queue->thread_queue[i+1].tid;
      queue->thread_queue[i].thread_exists = queue->thread_queue[i+1].thread_exists;
      queue->thread_queue[i].is_main = queue->thread_queue[i+1].is_main;
      queue->thread_queue[i].my_context = queue->thread_queue[i+1].my_context;
      queue->thread_queue[i].my_stack_pointer = queue->thread_queue[i+1].my_stack_pointer;
      queue->thread_queue[i].setcontext_called = queue->thread_queue[i+1].setcontext_called;
      queue->thread_queue[i].my_state = queue->thread_queue[i+1].my_state;
      queue->thread_queue[i].exit_code = queue->thread_queue[i+1].exit_code;
      queue->thread_queue[i].join_threads = queue->thread_queue[i+1].join_threads;
    }

    queue->thread_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

    // Checking for empty position in the ready queue

    for(i = 0; i < CSC369_MAX_THREADS; i++)  
    {
      if(ready_queue[i].thread_exists == 0)
      {
        ready_queue[i].tid = temp.tid;
        ready_queue[i].thread_exists = temp.thread_exists;
        ready_queue[i].is_main = temp.is_main;
        ready_queue[i].my_context = temp.my_context;
        ready_queue[i].my_stack_pointer = temp.my_stack_pointer;
        ready_queue[i].setcontext_called = temp.setcontext_called;
        ready_queue[i].my_state = temp.my_state;
        ready_queue[i].exit_code = temp.exit_code;
        ready_queue[i].join_threads = temp.join_threads;
        break;
      }
    }

  }

  // Setting interrupts to the previous state
  CSC369_InterruptsSet(prev_state);  

  return counter;

}

//****************************************************************************
// New Assignment 2 Definitions - Task 3
//****************************************************************************
int
CSC369_ThreadJoin(Tid tid, int* exit_code)
{

  // Disabling Interrupts
  int const prev_state = CSC369_InterruptsDisable();

  // Cleaning up all the dead threads before yielding
  my_clean();

  // Checking if the identifier is valid or not
  if(tid < 0 || tid >= CSC369_MAX_THREADS)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return CSC369_ERROR_TID_INVALID;
  }

  // Checking if the identifier is of the calling thread
  if(running.tid == tid)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return CSC369_ERROR_THREAD_BAD;
  }

  // Checking if the identifier is valid but the thread does not exist
  if(storage_tracker[tid].thread_exists == 0)
  {
    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    return CSC369_ERROR_SYS_THREAD;
  }

  TCB temp;

  temp.tid = running.tid;
  temp.thread_exists = running.thread_exists;
  temp.is_main = running.is_main;
  temp.my_context = running.my_context;
  temp.my_stack_pointer = running.my_stack_pointer;
  temp.setcontext_called = running.setcontext_called;
  temp.my_state = running.my_state;
  temp.exit_code = running.exit_code;
  temp.join_threads = running.join_threads;

  running.tid = ready_queue[0].tid;
  running.thread_exists = ready_queue[0].thread_exists;
  running.is_main = ready_queue[0].is_main;
  running.my_context = ready_queue[0].my_context;
  running.my_stack_pointer = ready_queue[0].my_stack_pointer;
  running.setcontext_called = ready_queue[0].setcontext_called;
  running.exit_code = ready_queue[0].exit_code;
  running.join_threads = ready_queue[0].join_threads;
  running.my_state = 1;

  // Shifting the queue
  int i;
  for(i = 0; i < CSC369_MAX_THREADS - 1; i++) 
  {
    ready_queue[i].tid = ready_queue[i+1].tid;
    ready_queue[i].thread_exists = ready_queue[i+1].thread_exists;
    ready_queue[i].is_main = ready_queue[i+1].is_main;
    ready_queue[i].my_context = ready_queue[i+1].my_context;
    ready_queue[i].my_stack_pointer = ready_queue[i+1].my_stack_pointer;
    ready_queue[i].setcontext_called = ready_queue[i+1].setcontext_called;
    ready_queue[i].my_state = ready_queue[i+1].my_state;
    ready_queue[i].exit_code = ready_queue[i+1].exit_code;
    ready_queue[i].join_threads = ready_queue[i+1].join_threads;
  }

  ready_queue[CSC369_MAX_THREADS - 1].thread_exists = 0;

  // Checking for empty position in the queue
  int index;

  for(i = 0; i < CSC369_MAX_THREADS; i++)  
  {
    if(storage_tracker[tid].join_threads->thread_queue[i].thread_exists == 0)
    {
      index = i;
      break;
    }
  }

  storage_tracker[tid].join_threads->thread_queue[index].tid = temp.tid;
  storage_tracker[tid].join_threads->thread_queue[index].thread_exists = temp.thread_exists;
  storage_tracker[tid].join_threads->thread_queue[index].is_main = temp.is_main;
  storage_tracker[tid].join_threads->thread_queue[index].my_stack_pointer = temp.my_stack_pointer;
  storage_tracker[tid].join_threads->thread_queue[index].setcontext_called = temp.setcontext_called;
  storage_tracker[tid].join_threads->thread_queue[index].exit_code = temp.exit_code;
  storage_tracker[tid].join_threads->thread_queue[index].join_threads = temp.join_threads;
  storage_tracker[tid].join_threads->thread_queue[index].my_state = 0;


  // int temp_id = ready_queue[index].tid;
  int running_id = running.tid;

  storage_tracker[running.tid].setcontext_called = 0;

  // Getting the current context for the current thread
  getcontext(&storage_tracker[storage_tracker[tid].join_threads->thread_queue[index].tid].my_context);

  if(storage_tracker[running.tid].setcontext_called == 1)  // || storage_tracker[storage_tracker[tid].join_threads->thread_queue[index].tid].setcontext_called == 1) 
  { 
    // Setting the value of the exit code
    *exit_code = running.exit_code;

    // Setting interrupts to the previous state
    CSC369_InterruptsSet(prev_state);  

    // If it comes there, then the thread with identifier tid has just exited
    return tid;                                   
  }

  // Setting the context to the new thread
  storage_tracker[running.tid].setcontext_called = 1;
  setcontext(&storage_tracker[running.tid].my_context);

}












