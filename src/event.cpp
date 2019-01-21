#include <stdio.h>
#include <windows.h>
#include <assert.h>
#include "event.h"

/*
        A lock-free event queue.

        The reference implementation:
                Simple, fast, and practical non-blocking and blocking concurrent
   queue algorithms. by M. M. Michael and M. L. Scott. 15th ACM Symp. on
   Principles of Distributed Computing (PODC), Philadelphia, PA, May 1996.
                Earlier version published as TR 600, Computer Science Dept.,
   Univ. of Rochester, Dec. 1995.
                (http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf)

*/

event_queue_t*
init_event_queue(void* buffer, size_t size)
{
  // The buffer must have enough space for the queue and at least one entry.
  assert(size >= sizeof(event_queue_t) + sizeof(event_entry_t));
  event_queue_t* queue = (event_queue_t*)buffer;
  queue->free_count = (size - sizeof(event_queue_t)) / sizeof(event_entry_t);
  queue->count = 0;

  queue->head = NULL;
  queue->tail = NULL;
  queue->free = (event_entry_t*)((char*)buffer + sizeof(event_queue_t));

  // Initialize the events to a default state.
  for (int n = 0; n < queue->free_count; n++) {
    event_entry_t* e = queue->free + n;
    e->event = {};
    e->next = NULL;
  }
  // Initialize the free list.
  for (int n = 0; n < queue->free_count - 1; n++) {
    event_entry_t* e = queue->free + n;
    e->next = queue->free + n + 1;
  }

  // Initialize the head and tail with a first entry.
  event_entry_t* first = pop_free_event(queue);
  first->next = NULL;
  queue->head = queue->tail = first;

  return queue;
}

void
print_event(event_t* e)
{
  FILETIME filetime;

  // Unpack from 64-bit integer to a filetime.
  filetime.dwLowDateTime = (DWORD)(e->timestamp & 0xFFFFFFFF);
  filetime.dwHighDateTime = (DWORD)(e->timestamp >> 32);

  SYSTEMTIME st;
  char szLocalDate[255], szLocalTime[255];

  // Note(kbostelmann): The zero event will cause these functions to generate
  // garbage.
  if (FileTimeToLocalFileTime(&filetime, &filetime)) {
    if (FileTimeToSystemTime(&filetime, &st)) {
      GetDateFormat(
        LOCALE_INVARIANT, DATE_SHORTDATE, &st, NULL, szLocalDate, 255);
      GetTimeFormat(
        LOCALE_INVARIANT, TIME_FORCE24HOURFORMAT, &st, NULL, szLocalTime, 255);

      printf("%s %s.%d (%u): ",
             szLocalDate,
             szLocalTime,
             st.wMilliseconds,
             e->thread_id);
    }
  }

  switch (e->type) {
    case EVENT_RANDOM: {
      printf("random event occured.");
      break;
    }
    case EVENT_UNKNOWN:
    default: {
      printf("unknown event occured.");
    }
  }
}

event_entry_t*
pop_free_event(event_queue_t* queue)
{
  event_entry_t* free_entry = NULL;
  event_entry_t* next_entry = NULL;
  do {
    free_entry = queue->free;
    if (free_entry == NULL) {
      return free_entry;
    }
    next_entry = free_entry->next;
  } while (InterlockedCompareExchangePointer(
             (PVOID*)&queue->free, next_entry, free_entry) != free_entry);
  queue->free_count--;
  return free_entry;
}

void
push_free_event(event_queue_t* queue, event_entry_t* entry)
{
  do {
    entry->next = queue->free;
  } while (InterlockedCompareExchangePointer(
             (PVOID*)&queue->free, entry, entry->next) != entry->next);
  queue->free_count++;
}

void
push_event(event_queue_t* queue, event_t event)
{
  // Never push an uninitialized event.
  assert(event.type != EVENT_UNKNOWN);

  if (queue->free_count > 0) {
    // There must be a free entry available.
    assert(queue->free != NULL);
    event_entry_t* entry = pop_free_event(queue);
    assert(entry != NULL);

    // Populate the event timestamp.
    FILETIME filetime = {};
    GetSystemTimePreciseAsFileTime(&filetime);

    // Pack the filetime in to a 64-bit integer.
    event.timestamp =
      (((uint64_t)filetime.dwHighDateTime) << 32) + filetime.dwLowDateTime;

    event.thread_id = GetCurrentThreadId();

    entry->event = event;
    entry->next = NULL;

    event_entry_t* tail = NULL;

    do {
      tail = queue->tail;
      event_entry_t* next = tail->next;
      if (tail == queue->tail) {
        if (next == NULL) {
          if (InterlockedCompareExchangePointer(
                (PVOID*)&tail->next, entry, next) == next) {
            break;
          }
        } else {
          InterlockedCompareExchangePointer((PVOID*)&queue->tail, next, tail);
        }
      }
    } while (true);

    InterlockedCompareExchangePointer((PVOID*)&queue->tail, entry, tail);

    queue->count++;
  }
}

bool
poll_event(event_queue_t* queue, event_t* event)
{
  event_entry_t* head = NULL;
  do {
    head = queue->head;
    event_entry_t* tail = queue->tail;
    event_entry_t* next = head->next;

    if (head == queue->head) {
      if (head == tail) {
        if (next == NULL) {
          return false;
        }
        InterlockedCompareExchangePointer((PVOID*)&queue->tail, next, tail);
      } else {
        // Note(kbostelmann): The reference implmentation reads the value to
        // prevent the dequeue from freeing the next node. Why?
        event_t garbage = next->event;
        if (InterlockedCompareExchangePointer(
              (PVOID*)&queue->head, next, head) == head) {
          break;
        }
      }
    }
  } while (true);

  *event = head->event;
  push_free_event(queue, head);

  queue->count--;

  return true;
}
