#include <windows.h>
#include <assert.h>
#include <time.h>
#include "producer.h"
#include "event.h"

/*
        A producer thread which produces a random event every x milliseconds.
*/

thread_t
create_producer_thread(producer_params_t* params)
{
  thread_t thread = {};

  thread.handle = CreateThread(NULL,             // default security attributes
                               0,                // use default stack size
                               producer_do_work, // thread function name
                               params,           // argument to thread function
                               0,                // use default creation flags
                               &thread.id); // returns the thread identifier

  return thread;
}

unsigned long __stdcall producer_do_work(void* void_params)
{
  producer_params_t* params = (producer_params_t*)void_params;

  clock_t start = clock();

  while (params->keep_alive) {
    if (params->do_work) {
      event_t e = {};
      e.type = EVENT_RANDOM;
      push_event(params->queue, e);
      params->produced_count++;
    }
    Sleep(params->period_ms);
  }

  params->elapsed_sec = ((double)(clock() - start)) / CLOCKS_PER_SEC;

  return 0;
}
