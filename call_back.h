/*

    copyright 2019 ggyyll

*/

#include <functional>
#include <memory>
class connection;
using connection_ptr = std::shared_ptr<connection>;
using close_call_back = std::function<void(const connection_ptr&)>;
using message_call_back = std::function<void(const connection_ptr&)>;
using connect_call_back = std::function<void(const connection_ptr&)>;
