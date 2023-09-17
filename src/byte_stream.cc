#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), buffer(string()) {}

void Writer::push( string data )
{
    // Your code here.
    if (closed_) set_error();
    unsigned availableSpace = available_capacity();

    availableSpace = availableSpace < data.size()? availableSpace : data.size();
    for (unsigned i = 0; i < availableSpace; ++i)
        buffer.push_back(data[i]);

    pushed_count_ += availableSpace;
}

void Writer::close()
{
    // Your code here.
    closed_ = true;
}

void Writer::set_error()
{
  // Your code here.
    error_ = true;
}

bool Writer::is_closed() const
{
    // Your code here.
    return closed_;
}

uint64_t Writer::available_capacity() const
{
    // Your code here.
    return capacity_ - buffer.size();
}

uint64_t Writer::bytes_pushed() const
{
    // Your code here.
    return pushed_count_;
}

string_view Reader::peek() const
{
    // Your code here.
    return string_view(buffer);
}

bool Reader::is_finished() const
{
    // Your code here.
    return closed_ && buffer.empty();
}

bool Reader::has_error() const
{
    // Your code here.
    return error_;
}

void Reader::pop( uint64_t len )
{
    int actual_len = len < buffer.size()? len:buffer.size();
    buffer = buffer.substr(actual_len, buffer.size());
    popped_count_ += len;
}

uint64_t Reader::bytes_buffered() const
{
    // Your code here.
    return buffer.size();
}

uint64_t Reader::bytes_popped() const
{
    // Your code here.
    return popped_count_;
}
