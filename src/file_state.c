#include <stdlib.h>

#include "file_state.h"

static void expand_table(fstate_table_t* table) {
  size_t i;
  size_t new_len;
  fstate_t* new_states;

  new_len = table->states_len * 2;
  new_states = realloc(table->states, new_len * sizeof(fstate_t));
  // new_states better not be NULL
  for (i = table->states_len; i < new_len; i++) {
    new_states[i].fh = FILE_FH_NOT_PRESENT;
  }
  table->states_len = new_len;
  table->states = new_states;
}

static fstate_t* find_state(fstate_table_t* table, int fh) {
  size_t i;
  for (i = 0; i < table->states_len; i++) {
    if (table->states[i].fh == fh) {
      return &table->states[i];
    }
  }
  return NULL;
}

fstate_table_t* create_table() {
  fstate_table_t* table;

  table = malloc(sizeof(fstate_table_t));
  pthread_mutex_init(&table->mutex, NULL);
  table->states_len = 1;
  table->states = malloc(sizeof(fstate_t));
  table->states[0].fh = FILE_FH_NOT_PRESENT;
}

fstate_t* create_state(fstate_table_t* table, int fh) {
  size_t i;
  fstate_t* state;

  pthread_mutex_lock(&table->mutex);
  state = NULL;
  for (i = 0; i < table->states_len; i++) {
    if (table->states[i].fh == FILE_FH_NOT_PRESENT) {
      state = &table->states[i];
      break;
    }
  }
  if (state == NULL) {
    expand_table(table);
    state = &table->states[i];
  }
  state->fh = fh;
  pthread_mutex_unlock(&table->mutex);
  return state;
}

void destroy_state(fstate_table_t* table, int fh) {
  fstate_t* state;

  pthread_mutex_lock(&table->mutex);
  state = find_state(table, fh);
  if (state != NULL) {
    state->fh = FILE_FH_NOT_PRESENT;
  }
  pthread_mutex_unlock(&table->mutex);
}

fstate_t* get_state(fstate_table_t* table, int fh) {
  fstate_t* state;

  pthread_mutex_lock(&table->mutex);
  state = find_state(table, fh);
  pthread_mutex_unlock(&table->mutex);
  return state;
}
