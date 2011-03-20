#ifndef FILE_STATE_H
#define FILE_STATE_H

#include <pthread.h>

#include "buffer.h"

#define FILE_TYPE_REGULAR 1
#define FILE_TYPE_SPECIAL 2

#define FILE_FH_NOT_PRESENT (-1)

typedef struct {

} fstate_regular_t;

typedef struct {
  buffer_t buffer;
} fstate_special_t;

typedef struct {
  int fh;
  int type;
  union {
    fstate_regular_t regular;
    fstate_special_t special;
  };
} fstate_t;

typedef struct {
  pthread_mutex_t mutex;
  fstate_t* states;
  size_t states_len;
} fstate_table_t;

typedef struct {
  fstate_table_t* fstate_table;
} internal_t;

internal_t* get_internal();

fstate_table_t* create_table();
fstate_t* create_state(fstate_table_t*, int);
void destroy_state(fstate_table_t*, int);
fstate_t* get_state(fstate_table_t*, int);

#endif
