#include <fuse.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <attr/attributes.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include "file_state.h"

#define VIEW_FILE_ATTR_NAME "viewfile"
#define VIEW_FILE_ATTR_VALUE "enabled"
#define LOG_FILE "/tmp/viewfs.log"
#define SHELL_NAME "/bin/sh"
#define SHELL_CMD_ARG "-c"

static int log_file;

#define debug(s) debug_f(s, sizeof(s) - 1)

static void debug_f(const char* s, size_t length) {
  write(log_file, s, length);
}

static int view_getattr(const char* path, struct stat* statbuffer) {
  int ret;

  debug("view_getattr\n");
  
  ret = lstat(path, statbuffer);
  if (ret == -1) {
    return -errno;
  }
  return 0;
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

static int view_open_special(const char* path, struct fuse_file_info* info) {
  int pipefds[2];
  pid_t pid;
  int ret;
  
  ret = pipe(pipefds);
  if (ret == -1) {
    return -errno;
  }
  
  pid = fork();
  if (pid == -1) {
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
  execl(SHELL_NAME, SHELL_CMD_ARG, path, NULL);
  debug("this should not happen\n");
  
 parent:
  debug("parent\n");
  close(pipefds[1]);
  
  info->fh = pipefds[0];

  return 0;
}

static int view_open_regular(const char* path, struct fuse_file_info* info) {
  int fh;

  fh = open(path, info->flags);
  if (fh == -1) {
    return -errno;
  }
  info->fh = fh;

  return 0;
}

static int view_open(const char* path, struct fuse_file_info* info) {
  char attr_val[256];
  int attr_val_len;
  int special;
  int ret;

  special = 0;
  
  debug("view_open\n");
  
  attr_val_len = sizeof(attr_val);
  ret = attr_get(path, VIEW_FILE_ATTR_NAME, attr_val, &attr_val_len, 0);
  if (ret == 0) {
    if (strncmp(attr_val, VIEW_FILE_ATTR_VALUE, attr_val_len) == 0) {
      goto special;
    } else {
      goto regular;
    }
  } else {
    if (errno != ENODATA) {
      return -errno;
    } else {
      goto regular;
    }
  }  
 special:
  return view_open_special(path, info);
 regular:
  return view_open_regular(path, info);
}

static int view_read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* info) {
  int ret;

  debug("view_read\n");
  
  ret = pread(info->fh, buffer, size, offset);
  if (ret == -1) {
    ret = -errno;
  }
  return ret;
}

static void* view_init(struct fuse_conn_info* conn) {
  internal_t* internal = malloc(sizeof(internal_t));
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
  log_file = open(LOG_FILE, O_CREAT | O_TRUNC | O_WRONLY);
  if (log_file < 0) {
    exit(42);
  }
  umask(0);
  return fuse_main(argc, argv, &view_operations, NULL);
}
