#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity + 1 ), buffer_( string( capacity + 1, 0 ) ) {}

bool ByteStream::is_full() const
{
  return ( tail_ + 1 ) % capacity_ == head_;
}

bool ByteStream::is_empty() const
{
  return head_ == tail_;
}

void ByteStream::push( char ch )
{
  buffer_[tail_] = ch;
  tail_ += 1;
  if ( tail_ == capacity_ ) {
    const size_t len = tail_ - head_;
    std::copy( buffer_.begin() + static_cast<int64_t>( head_ ), buffer_.end(), buffer_.begin() );
    head_ = 0;
    tail_ = len;
  }
}

char ByteStream::pop()
{
  const char ch = buffer_[head_];
  head_ = ( head_ + 1 ) % capacity_;
  return ch;
}

void Writer::push( const string& data )
{
  for ( const char ch : data ) {
    if ( ByteStream::is_full() ) {
      return;
    }
    bytes_pushed_ += 1;
    ByteStream::push( ch );
  }
}

void Writer::close()
{
  is_closed_ = true;
}

void Writer::set_error()
{
  has_error_ = true;
}

bool Writer::is_closed() const
{
  return is_closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - ( tail_ + capacity_ - head_ ) % capacity_ - 1;
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  return { buffer_.begin() + static_cast<int64_t>( head_ ), buffer_.begin() + static_cast<int64_t>( tail_ ) };
}

bool Reader::is_finished() const
{
  return is_closed_ && bytes_buffered() == 0;
}

bool Reader::has_error() const
{
  return has_error_;
}

void Reader::pop( uint64_t len )
{
  for ( uint64_t i = 0; i < len; ++i ) {
    if ( ByteStream::is_empty() ) {
      return;
    }
    bytes_popped_ += 1;
    ByteStream::pop();
  }
}

uint64_t Reader::bytes_buffered() const
{
  return ( tail_ + capacity_ - head_ ) % capacity_;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}
