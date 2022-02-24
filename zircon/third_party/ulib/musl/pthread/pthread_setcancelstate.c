#include <errno.h>
#include <pthread.h>

int pthread_setcancelstate(int new, int* old) {
  if (new != PTHREAD_CANCEL_ENABLE&& new != PTHREAD_CANCEL_DISABLE)
    return EINVAL;
  return ENOSYS;
}
