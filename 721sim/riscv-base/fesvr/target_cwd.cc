#include <fcntl.h>
#include <climits>
#include <cstring>
#include <unistd.h>
#include "syscall.h"
#include "htif.h"
#include "target_cwd.h"

//#define _DEBUG_MSG(...) fprintf(stderr, __VA_ARGS__)
#define _DEBUG_MSG(...)

static char *normalize_path(const char *pwd, const char *src, char *res) {
  size_t res_len;
  size_t src_len = strlen(src);

  const char *ptr = src;
  const char *end = &src[src_len];
  const char *next;

  if (src_len == 0 || src[0] != '/') {
    // relative path
    size_t pwd_len;

    pwd_len = strlen(pwd);
    memcpy(res, pwd, pwd_len);
    res_len = pwd_len;
  } else {
    res_len = 0;
  }

  for (ptr = src; ptr < end; ptr = next + 1) {
    size_t len;
    next = (char *) memchr(ptr, '/', end - ptr);
    if (next == NULL) {
      next = end;
    }
    len = next - ptr;
    switch (len) {
      case 2:
        if (ptr[0] == '.' && ptr[1] == '.') {
          const char *slash = (char *) memrchr(res, '/', res_len);
          if (slash != NULL) {
            res_len = slash - res;
          }
          continue;
        }
        break;
      case 1:
        if (ptr[0] == '.') {
          continue;
        }
        break;
      case 0:
        continue;
    }

    if (res_len != 1)
      res[res_len++] = '/';

    memcpy(&res[res_len], ptr, len);
    res_len += len;
  }

  if (res_len == 0) {
    res[res_len++] = '/';
  }
  res[res_len] = '\0';
  return res;
}

target_cwd::target_cwd(syscall_t *syscall) : m_syscall(syscall), m_fd_cwd(AT_FDCWD) {
  // initialized with default cwd
  char path_buf[PATH_MAX];
  if (syscall->htif->chroot.empty()) {
    // Use host cwd as the default target cwd if chroot disabled
    _DEBUG_MSG("FESVR started with chroot disabled\n");
    if (!getcwd(path_buf, sizeof(path_buf))) {
      fprintf(stderr, "Fatal: getcwd() failed (%s).\n", strerror(errno));
      exit(-1);
    }
  } else {
    // Use root as the default target cwd if chroot enabled
    _DEBUG_MSG("FESVR started with chroot enabled [%s <-> /]\n", syscall->htif->chroot.c_str());
    path_buf[0] = '/';
    path_buf[1] = '\0';
  }
  _DEBUG_MSG("Initialize target cwd with default path [%s]\n", path_buf);
  if (this->target_chdir(path_buf)) {
    fprintf(stderr, "Cannot init target cwd using the default value (%s) - %s\n", strerror(errno), path_buf);
    exit(-1);
  }
}

int target_cwd::target_chdir(const char *path) {
  char normalized_path[PATH_MAX];
  normalize_path(m_target_cwd.c_str(), path, normalized_path);
  std::string new_host_path = m_syscall->do_chroot(normalized_path);

  int new_fd_dir = openat(m_fd_cwd, new_host_path.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NONBLOCK);

  if (new_fd_dir < 0) {
    _DEBUG_MSG("Target chdir() failed %s %s %s (%s)\n", path, normalized_path, new_host_path.c_str(), strerror(errno));
    return -1;
  }

  _DEBUG_MSG("Target chdir() succeed %s %s %s, new fd_dir = %d\n", path, normalized_path, new_host_path.c_str(),
             new_fd_dir);

  close(m_fd_cwd);
  m_fd_cwd = new_fd_dir;
  m_target_cwd = normalized_path;

  return 0;
}

char *target_cwd::target_getcwd(char *buf, size_t size) {
  if (size < m_target_cwd.size() + 1) {

    _DEBUG_MSG("Target getcwd() failed (buffer size too small)\n");
    errno = ERANGE;
    return nullptr;
  }
  memcpy(buf, m_target_cwd.c_str(), m_target_cwd.size());
  buf[m_target_cwd.size()] = '\0';
  _DEBUG_MSG("Target getcwd() returns %s\n", buf);
  return buf;
}
