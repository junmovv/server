#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H
class noncopyable
{

protected:
    noncopyable() = default;
    ~noncopyable() = default;

public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;
};

#endif