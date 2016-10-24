#ifndef _JULIA_EVENT_H_
#define _JULIA_EVENT_H_

#include <stdint.h>
#include <stdalign.h>

// Workaround, as wgtcc do not support GNU atrribute(packed) extension 
#ifdef __wgtcc__
typedef union {
  void *ptr;
  int fd;
  uint32_t u32;
  uint64_t u64;
} julia_epoll_data_t;

typedef alignas(uint64_t) struct {
  uint32_t events;	/* Epoll events */
  alignas(uint32_t) julia_epoll_data_t data;	/* User data variable */
} julia_epoll_event_t;

#else
#include <sys/epoll.h>
typedef struct epoll_event julia_epoll_event_t;
#endif

#endif
