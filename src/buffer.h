#ifndef BUFFER_H
#define BUFFER_H

typedef struct {
  size_t length;
  char* data;
} buffer_t;

void init_buffer(buffer_t* buffer);
void deinit_buffer(buffer_t* buffer);
void fill_buffer(buffer_t* buffer, int fh);
int copy_buffer(buffer_t* buffer, char* dest, size_t size, off_t offset);

#endif
