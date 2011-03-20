#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "buffer.h"

static void buffer_append(buffer_t* buffer, char* src, size_t size) {
  int new_len;
  char* new_data;

  new_len = buffer->length + size;
  new_data = realloc(buffer->data, new_len);
  // new_data better not be NULL
  memcpy(new_data + buffer->length, src, size);
  buffer->length = new_len;
  buffer->data = new_data;
}

void init_buffer(buffer_t* buffer) {
  buffer->length = 0;
  buffer->data = malloc(0);
}

void deinit_buffer(buffer_t* buffer) {
  free(buffer->data);
}

void fill_buffer(buffer_t* buffer, int fh) {
  int ret;
  char local_buf[1024];

  while(1) {
    ret = read(fh, local_buf, sizeof(local_buf));
    if (ret == -1) {
      return;
    } else if (ret == 0) {
      return;
    } else {
      buffer_append(buffer, local_buf, ret);
    }
  }
}

int copy_buffer(buffer_t* buffer, char* dest, size_t size, off_t offset) {
  if (offset > buffer->length) {
    return 0;
  }
  if (size + offset > buffer->length) {
    size = buffer->length - offset;
  }
  memcpy(dest, buffer->data + offset, size);
  return size;
}
