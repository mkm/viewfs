#include <fuse.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <attr/attributes.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "file_state.h"

#define VIEW_FILE_ATTR_NAME "viewfile"
#define VIEW_FILE_ATTR_VALUE "enabled"
#define LOG_FILE "/tmp/viewfs.log"
#define SHELL_NAME "/bin/sh"

static int log_file;

#define debug(s) debug_f(s, sizeof(s) - 1)

static void debug_f(const char* s, size_t length) {
  write(log_file, s, length);
}

static void debugi(int n) {
  char s[12];
  sprintf(s, "%11i\n", n);
  debug_f(s, sizeof(s));
}

static int get_file_type(const char* path) {
  char attr_val[256];
  int attr_val_len;
  int ret;

  attr_val_len = sizeof(attr_val);
  ret = attr_get(path, VIEW_FILE_ATTR_NAME, attr_val, &attr_val_len, 0);
  if (ret == 0) {
    if (strncmp(attr_val, VIEW_FILE_ATTR_VALUE, attr_val_len) == 0) {
      return FILE_TYPE_SPECIAL;
    } else {
      return FILE_TYPE_REGULAR;
    }
  } else {
    if (errno != ENODATA) {
      return -errno;
    } else {
      return FILE_TYPE_REGULAR;
    }
  }  
}

static int get_file_data(const char* path, buffer_t* buffer) {
  int pipefds[2];
  pid_t pid;
  int ret;
  
  ret = pipe(pipefds);
  if (ret == -1) {
    return -errno;
  }
  
  pid = fork();
  if (pid == -1) {
    close(pipefds[0]);
    close(pipefds[1]);
    return -errno;
  } else if (pid == 0) {
    goto child;
  } else {
    goto parent;
  }
 child:
  debug("child\n");
  close(pipefds[0]);
  dup2(pipefds[1], 1);
  dup2(pipefds[1], 2);
  execl(SHELL_NAME, SHELL_NAME, path, NULL);
  debug("this should not happen\n");
  exit(1);
  
 parent:
  debug("parent\n");
  close(pipefds[1]);
  
  init_buffer(buffer);
  fill_buffer(buffer, pipefds[0]);

  return pipefds[0];
}

static int view_getattr_regular(const char* path, struct stat* statbuffer) {
  int ret;
  
  ret = lstat(path, statbuffer);
  if (ret == -1) {
    return -errno;
  }
  return 0;
}

static int view_getattr_special(const char* path, struct stat* statbuffer) {
  int ret;
  buffer_t file_data;

  ret = lstat(path, statbuffer);
  if (ret == -1) {
    return -errno;
  }
  ret = get_file_data(path, &file_data);
  if (ret < 0) {
    return ret;
  } else {
    close(ret);
  }
  statbuffer->st_size = file_data.length;
  deinit_buffer(&file_data);
  return 0;
}

static int view_getattr(const char* path, struct stat* statbuffer) {
  int file_type;

  debug("view_getattr\n");

  file_type = get_file_type(path);
  if (file_type == FILE_TYPE_REGULAR) {
    return view_getattr_regular(path, statbuffer);
  } else if (file_type == FILE_TYPE_SPECIAL) {
    return view_getattr_special(path, statbuffer);
  } else {
    return file_type;
  }
}

static int view_access(const char* path, int mask) {
  int ret;

  ret = access(path, mask);
  if (ret == -1) {
    return -errno;
  }
  return 0;
}

static int view_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info) {
  DIR *dp;
  struct dirent *de;

  (void)offset;
  (void)info;
  
  dp = opendir(path);
  if (dp == NULL)
    return -errno;

  while ((de = readdir(dp)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;
    if (filler(buffer, de->d_name, &st, 0))
      break;
  }

  closedir(dp);
  return 0;
}

static int view_open_regular(const char* path, struct fuse_file_info* info) {
  int fh;
  fstate_table_t* table;
  fstate_t* state;

  fh = open(path, info->flags);
  if (fh == -1) {
    return -errno;
  }
  info->fh = fh;
  table = get_internal()->fstate_table;
  state = create_state(table, info->fh);
  state->type = FILE_TYPE_REGULAR;

  return 0;
}

static int view_open_special(const char* path, struct fuse_file_info* info) {
  fstate_t* state;
  fstate_table_t* table;
  buffer_t buffer;
  int ret;

  ret = get_file_data(path, &buffer);
  if (ret < 0) {
    return ret;
  }
  info->fh = ret;
  table = get_internal()->fstate_table;
  state = create_state(table, info->fh);
  state->type = FILE_TYPE_SPECIAL;
  state->special.buffer = buffer;
  return 0;
}

static int view_open(const char* path, struct fuse_file_info* info) {
  int file_type;

  debug("view_open\n");

  file_type = get_file_type(path);
  if (file_type == FILE_TYPE_REGULAR) {
    return view_open_regular(path, info);    
  } else if (file_type == FILE_TYPE_SPECIAL) {
    return view_open_special(path, info);
  } else {
    return file_type;
  }
}

static int view_read_regular(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* info) {
  int ret;

  (void)path;
  
  ret = pread(info->fh, buffer, size, offset);
  if (ret == -1) {
    ret = -errno;
  }
  return ret;
}

static int view_read_special(char* buffer, size_t size, off_t offset, fstate_t* state) {
  int ret;

  ret = copy_buffer(&state->special.buffer, buffer, size, offset);
  return ret;
}

static int view_read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* info) {
  internal_t* internal;
  fstate_t* state;
  
  debug("view_read\n");

  internal = get_internal();
  state = get_state(internal->fstate_table, info->fh);
  if (state == NULL) {
    debug("get_state failed\n");
    return -EBADFD;
  }

  if (state->type == FILE_TYPE_REGULAR) {
    return view_read_regular(path, buffer, size, offset, info);
  } else if (state->type == FILE_TYPE_SPECIAL) {
    return view_read_special(buffer, size, offset, state);
  } else {
    debug("unreachable");
    return -EBADFD;
  }
}

static void* view_init(struct fuse_conn_info* conn) {
  internal_t* internal;

  (void)conn;

  internal = malloc(sizeof(internal_t));
  internal->fstate_table = create_table();
  return internal;
}

static struct fuse_operations view_operations = {
  .getattr = view_getattr,
  .access = view_access,
  .readdir = view_readdir,
  .open = view_open,
  .read = view_read,
  .init = view_init
};

int main(int argc, char** argv) {
  char* base_dir;

  if (argc > 2) {
    base_dir = argv[argc - 2];
    argv[argc - 2] = argv[argc - 1];
    argc -= 1;
  } else {
    base_dir = "/";
  }
  
  log_file = open(LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY);
  if (log_file < 0) {
    exit(42);
  }
  umask(0);
  return fuse_main(argc, argv, &view_operations, NULL);
}
