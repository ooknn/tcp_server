#ifndef __WRAPPER_H__
#define __WRAPPER_H__
#include <chrono>
#include <memory>
namespace ooknn
{
template <typename Callback>
class scoped_exit
{
public:
    template <typename C>
    scoped_exit(C&& c)
        : callback_(std::forward<C>(c))
    {
    }
    scoped_exit(scoped_exit&& mv)
        : callback_(std::move(mv.callback_))
        , canceled_(mv.canceled_)
    {
        mv.canceled_ = true;
    }

    scoped_exit(const scoped_exit&) = delete;
    scoped_exit& operator=(const scoped_exit&) = delete;

    ~scoped_exit() { call(); }

    scoped_exit& operator=(scoped_exit&& mv)
    {
        if (this != &mv)
        {
            call();
            callback_ = std::move(mv.callback_);
            canceled_ = mv.canceled_;
            mv.canceled_ = true;
        }

        return *this;
    }

    void cancel() { canceled_ = true; }

private:
    void call()
    {
        if (!canceled_)
        {
            try
            {
                callback_();
            } catch (...)
            {
            }
        }
    }
    Callback callback_;
    bool canceled_ = false;
};

namespace unique_ptr
{
    // 注意：此实现不为数组类型禁用此重载
    template <typename T, typename... Args>
    static std::unique_ptr<T> make_unique(Args&&... args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
} // namespace unique_ptr

template <typename Callback>
static scoped_exit<Callback> make_scoped_exit(Callback&& c)
{
    return scoped_exit<Callback>(std::forward<Callback>(c));
}

#if (__cplusplus >= 201402L) || (defined(_MSC_VER) && _MSC_VER >= 1800)
using std::make_unique;
#else
using unique_ptr::make_unique;
#endif

} // namespace ooknn
#endif
