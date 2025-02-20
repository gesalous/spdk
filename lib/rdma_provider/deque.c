#include "deque.h"
#include "portals_log.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct deque *deque_create(dlist_destructor_fn destructor) {
  struct deque *deque = calloc(1UL, sizeof(struct deque));
  if (!deque)
    return NULL;

  // Create the underlying list without compare function
  deque->list = dlist_create(NULL, destructor);
  if (!deque->list) {
    free(deque);
    return NULL;
  }
  if (pthread_mutex_init(&deque->lock, NULL)) {
    SPDK_PTL_FATAL("FAILED to initialize mutex");
  }
  return deque;
}

void deque_destroy(struct deque *deque) {
  if (!deque)
    return;

  dlist_destroy(deque->list);
  free(deque);
}

// Front operations
bool deque_push_front(struct deque *deque, void *data) {
  assert(deque != NULL);
  return dlist_prepend(deque->list, data);
}

void *deque_pop_front(struct deque *deque) {
  assert(deque != NULL);

  if (deque_is_empty(deque)) {
    return NULL;
  }

  void *data = dlist_get_first(deque->list);
  dlist_remove_first(deque->list);
  return data;
}

void *deque_peek_front(struct deque *deque) {
  assert(deque != NULL);
  return dlist_get_first(deque->list);
}

// Back operations
bool deque_push_back(struct deque *deque, void *data) {
  assert(deque != NULL);
  return dlist_append(deque->list, data);
}

void *deque_pop_back(struct deque *deque) {
  assert(deque != NULL);

  if (deque_is_empty(deque)) {
    // fprintf(stderr,"Queue is empty\n");
    return NULL;
  }

  void *data = dlist_remove_last(deque->list);
  assert(data);
  return data;
}

void *deque_peek_back(struct deque *deque) {
  assert(deque != NULL);
  return dlist_get_last(deque->list);
}

// Utility functions
size_t deque_size(struct deque *deque) {
  assert(deque != NULL);
  return dlist_size(deque->list);
}

bool deque_is_empty(struct deque *deque) {
  assert(deque != NULL);
  return dlist_is_empty(deque->list);
}

void deque_clear(struct deque *deque) {
  assert(deque != NULL);

  while (!deque_is_empty(deque)) {
    deque_pop_front(deque);
  }
}

// Iterator support
struct dlist_iter deque_iter_create(struct deque *deque) {
  assert(deque != NULL);
  return dlist_iter_create(deque->list);
}

void *deque_iter_next(struct dlist_iter *iter) { return dlist_iter_next(iter); }

bool deque_iter_has_next(struct dlist_iter *iter) {
  return dlist_iter_has_next(iter);
}

