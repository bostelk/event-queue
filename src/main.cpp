#include <stdio.h>
#include <windows.h>
#include <conio.h>
#include <assert.h>

#include "event.h"
#include "producer.h"

/*
        Creates n producers threads while the main thread polls for events.
*/

int
main()
{
  int64_t event_count = 10000;
  size_t buffer_size =
    sizeof(event_queue_t) + event_count * sizeof(event_entry_t);
  void* buffer = malloc(buffer_size);

  event_queue_t* queue = init_event_queue(buffer, buffer_size);

  // Find the number of logical processors.
  SYSTEM_INFO system_info = {};
  GetSystemInfo(&system_info);

  // Create n producer threads.
  int32_t producer_count = (int64_t)system_info.dwNumberOfProcessors;
  producer_params_t* producer_params =
    (producer_params_t*)malloc(producer_count * sizeof(*producer_params));
  HANDLE* producer_handles =
    (HANDLE*)malloc(producer_count * sizeof(*producer_handles));
  uint32_t* producer_ids =
    (uint32_t*)malloc(producer_count * sizeof(*producer_ids));
  for (int n = 0; n < producer_count; n++) {
    producer_params_t* params = producer_params + n;
    params->keep_alive = true;
    params->do_work = true;
    params->queue = queue;
    params->period_ms = 1;
    params->produced_count = 0;
    params->elapsed_sec = 0;

    // Produces events in a worker thread.
    thread_t thread = create_producer_thread(params);
    assert(thread.handle != NULL);

    producer_handles[n] = thread.handle;
    producer_ids[n] = thread.id;
  }

  // Print events in the main thread.
  bool keep_alive = true;
  while (keep_alive) {
    event_t e = {};
    if (poll_event(queue, &e)) {
      print_event(&e);
      printf("\n");
    }

    if (_kbhit()) {
      switch (_getch()) {
          // Quit.
        case 'q': {
          keep_alive = false;

          for (int n = 0; n < producer_count; n++) {
            producer_params[n].keep_alive = false;
            producer_params[n].do_work = false;
          }
          break;
        }
          // Toggle work.
        case 'p': {
          for (int n = 0; n < producer_count; n++) {
            producer_params[n].do_work = !producer_params[n].do_work;
          }
          break;
        }
        case '1': {
          if (producer_count > 0) {
            producer_params[0].do_work = !producer_params[0].do_work;
          }
          break;
        }
        case '2': {
          if (producer_count > 1) {
            producer_params[1].do_work = !producer_params[1].do_work;
          }
          break;
        }
      }
    }
  }

  // Note(kbostelmann): The time needs to be larger than the period_ms otherwise
  // the wait can timeout.
  uint32_t wait_timeout = 10001;
  unsigned long wait_ret = WaitForMultipleObjects(
    producer_count, producer_handles, true, wait_timeout);
  switch (wait_ret) {
    case WAIT_TIMEOUT: {
      for (int n = 0; n < producer_count; n++) {
        // Force the producer thread to exit; which can cause bad behaviour.
        TerminateThread(producer_handles[n], -1);
        CloseHandle(producer_handles[n]);

        printf("thread timeout (%u)\n", producer_ids[n]);
      }
      break;
    }
    case WAIT_OBJECT_0: {
      for (int n = 0; n < producer_count; n++) {
        unsigned long producer_exit = -1;
        GetExitCodeThread(producer_handles[n], &producer_exit);
        assert(producer_exit == 0);
      }

      break;
    }
    case WAIT_ABANDONED:
    case WAIT_FAILED:
    default: {
      printf(
        "something bad happend while waiting for the producer thread to exit.");
    }
  }

  producer_metrics_t* metrics =
    (producer_metrics_t*)malloc(producer_count * sizeof(*metrics));
  calc_metrics(producer_params, producer_ids, metrics, producer_count);
  print_metrics(metrics, producer_count);

  // The average number of events per-second.
  double sum_events_per_sec = 0;
  // An estimate of the max events per-second.
  double sum_events_per_sec_max = 0;
  for (int n = 0; n < producer_count; n++) {
    sum_events_per_sec += metrics[n].events_per_sec;
    sum_events_per_sec_max += metrics[n].events_per_sec_max;
  }

  printf("sum: %f events/second (%.2f%% of max).",
         sum_events_per_sec,
         100 * sum_events_per_sec / sum_events_per_sec_max);

  free(buffer);
  free(producer_params);
  free(producer_handles);
  free(producer_ids);
  free(metrics);

  return 0;
}
