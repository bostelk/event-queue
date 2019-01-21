#pragma once
#include <stdint.h>

// A type of event which can occur.
enum event_type : uint8_t
{
  // An unknown event occured.
  EVENT_UNKNOWN,
  // A random event occured.
  EVENT_RANDOM
};

// Event data.
struct event_t
{
  // The thread id for debugging.
  uint32_t thread_id;
  // A Windows FILETIME packed into a 64-bit integer.
  uint64_t timestamp;
  // The type of event.
  int8_t type;
};

// An entry in an event queue.
struct event_entry_t
{
  // The event data.
  event_t event;
  // The next event entry.
  event_entry_t* next;
};

struct event_queue_t
{
  // The head of the queue.
  event_entry_t* head;
  // The tail of the queue.
  event_entry_t* tail;

  // The free events list.
  event_entry_t* free;

  // The number of events in the queue.
  int64_t count;
  // The number of events in the free list.
  int64_t free_count;
};

// Initialize an event queue given some amount of memory.
event_queue_t*
init_event_queue(void* buffer, size_t size);

// Print an event.
void
print_event(event_t* e);

// Pop an event from the free list.
event_entry_t*
pop_free_event(event_queue_t* queue);

// Push an event on to the free list.
void
push_free_event(event_queue_t* queue, event_entry_t* entry);

// Push a new event on the queue.
void
push_event(event_queue_t* queue, event_t event);

// Dequeue an event.
// True on success, otherwise false.
bool
poll_event(event_queue_t* queue, event_t* event);
