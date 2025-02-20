#ifndef DLIST_H
#define DLIST_H

#include <stdbool.h>
#include <stddef.h>


// Comparison function type for searching and sorting
typedef int (*dlist_compare_fn)(const void *key1, const void *key2);
// Optional destructor for data
typedef void (*dlist_destructor_fn)(void *data);

// Node structure
struct dlist_node {
  void *data;
  struct dlist_node *prev;
  struct dlist_node *next;
};

struct dlist {
  struct dlist_node *head;
  struct dlist_node *tail;
  size_t size;
  dlist_compare_fn compare;
  dlist_destructor_fn destructor;
};

// Iterator structure
struct dlist_iter {
  struct dlist_node *current;
  struct dlist *list;
};

// List operations
struct dlist *dlist_create(dlist_compare_fn compare, dlist_destructor_fn destructor);
void dlist_destroy(struct dlist *list);
bool dlist_append(struct dlist *list, void *data);
bool dlist_prepend(struct dlist* list, void *data);
void *dlist_get_first(struct dlist *list);
void *dlist_get_last(struct dlist *list);
void *dlist_remove_first(struct dlist *list);
void *dlist_remove_last(struct dlist *list);
void *dlist_search(const struct dlist *list, const void *key);
size_t dlist_size(const struct dlist *list);
bool dlist_is_empty(const struct dlist *list);

// Sorting
void dlist_sort(struct dlist *list);

// Iterator operations
struct dlist_iter dlist_iter_create(struct dlist *list);
void *dlist_iter_next(struct dlist_iter *iter);
bool dlist_iter_has_next(const struct dlist_iter *iter);
void dlist_iter_reset(struct dlist_iter *iter);

#endif // DLIST_H
