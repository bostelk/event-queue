#include <windows.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>
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

void
calc_metrics(producer_params_t* params,
             uint32_t* thread_ids,
             producer_metrics_t* metrics,
             int producer_count)
{
  for (int n = 0; n < producer_count; n++) {
    metrics[n].thread_id = thread_ids[n];
    metrics[n].events_per_sec =
      params[n].produced_count / params[n].elapsed_sec;
    metrics[n].events_per_sec_max =
      (params[n].elapsed_sec * 1000) / params[n].period_ms;
    metrics[n].events_per_sec_max /= params[n].elapsed_sec;
  }
}

void
print_metrics(producer_metrics_t* metrics, int producer_count)
{
  for (int n = 0; n < producer_count; n++) {
    printf("thread (%u): %f events/second (%.2f%% of max).\n",
           metrics[n].thread_id,
           metrics[n].events_per_sec,
           100 * metrics[n].events_per_sec / metrics[n].events_per_sec_max);
  }
}