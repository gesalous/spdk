#ifndef DEQUE_H
#define DEQUE_H

#include "dlist.h"
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

struct deque {
  struct dlist *list;
  pthread_mutex_t lock;
};

// Core deque operations
struct deque *deque_create(dlist_destructor_fn destructor);
void deque_destroy(struct deque *deque);

// Front operations
bool deque_push_front(struct deque *deque, void *data);
void *deque_pop_front(struct deque *deque);
void *deque_peek_front(struct deque *deque);

// Back operations
bool deque_push_back(struct deque *deque, void *data);
void *deque_pop_back(struct deque *deque);
void *deque_peek_back(struct deque *deque);

// Utility functions
size_t deque_size(struct deque *deque);
bool deque_is_empty(struct deque *deque);
void deque_clear(struct deque *deque);

// Iterator support
struct dlist_iter deque_iter_create(struct deque *deque);
void *deque_iter_next(struct dlist_iter *iter);
bool deque_iter_has_next(struct dlist_iter *iter);

#endif

