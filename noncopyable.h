/*

   copyright 2019 ggyyll

*/

#ifndef __NOCOPYABLE_H__
#define __NOCOPYABLE_H__

class noncopyable
{
public:
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};
#endif
