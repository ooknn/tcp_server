/*

    copyright 2019 ggyyll

*/

#ifndef CONNECTION_H_
#define CONNECTION_H_

#include <vector>
#include <string.h>
#include "call_back.h"
#include "socket.h"
#include "log.h"
#include "error.h"

class io_context;

class connection : public std::enable_shared_from_this<connection>
{
public:
    static const int kInitSize = 1024;
    explicit connection(io_context* context, int fd);
    ~connection();
    void send(const std::string& message);
    std::string readAll();
    std::string readNByte(int n);
    std::string to_string();
    void set_on_message(message_call_back call) { _on_message = call; }
    void set_on_close(close_call_back call) { _on_close = call; }
    bool connected() { return !_closed; };
    void close();
    void shutdown();

private:
    void peer_address();
    void local_address();

    void enable_write()
    {
        _ev |= write_event;
        update();
    }
    void disenable_write()
    {
        _ev &= ~write_event;
        update();
    }

    void enable_read()
    {
        _ev |= read_event;
        update();
    }
    void disenable_read()
    {
        _ev &= ~read_event;
        update();
    }

    bool is_write() { return _ev & write_event; }
    bool is_read() { return _ev & read_event; }

    void update();
    void cancle();

private:
    void call_back();
    void write_buf();
    ssize_t readv_buff();

private:
    int _fd;
    int _ev; // event
    ssize_t input_index_;
    ssize_t output_index_;
    struct sockaddr_in peer_addr_;
    struct sockaddr_in local_addr_;
    std::vector<char> input_buffer; //
    std::vector<char> output_buffer;
    close_call_back _on_close;
    message_call_back _on_message;
    //  size_t _length;
    bool _closed;
    io_context* _context;
};
#endif // CONNECTION_H_
