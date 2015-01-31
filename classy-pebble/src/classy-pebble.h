#ifndef CLASSY_PEBBLE_H
#define CLASSY_PEBBLE_H

struct event{
  char name[EVENT_STR_LIMIT+1];
  time_t start;
  time_t end;
};


typedef struct event Event;

#endif