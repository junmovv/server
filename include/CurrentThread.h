#ifndef CURRENTTHREAD_H
#define CURRENTTHREAD_H
#include <unistd.h>
#include <sys/syscall.h>
namespace CurrentThread
{
    extern __thread int t_cachedTid;
    extern __thread const char *t_threadName;
    void cacheTid();

    inline int tid()
    {
        if (__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}
#endif