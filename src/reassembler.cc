#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  if ( !has_init_ ) {
    has_init_ = true;
    cap_ = output.available_capacity();
    buffer_ = string( cap_, 0 );
    vis = vector<bool>( cap_, false );
  }

  const auto data_len = static_cast<uint64_t>( data.size() );
  auto cur_cap = output.available_capacity();

  if ( is_last_substring ) {
    last_index_ = static_cast<int64_t>( first_index + data_len );
  }

  if ( first_index > next_index_ ) {
    /**
     * n_i     f_i
     *   | delta |  data_len          |
     *   |_______|____________________|
     *   |  cur_cap             e_i
     *   |________________________|
     */
    const uint64_t delta = first_index - next_index_;
    const uint64_t buf_len = min( data_len, cur_cap - delta );
    uint64_t buf_idx = ( buf_index_ + delta ) % cap_;
    for ( uint64_t offset = 0; offset < buf_len; ++offset ) {
      // do not count when overlap
      if ( !vis[buf_idx] ) {
        vis[buf_idx] = true;
        bytes_pending_ += 1;
        buffer_[buf_idx] = data[offset];
      }
      buf_idx = ( buf_idx + 1 ) % cap_;
    }
  } else if ( first_index <= next_index_ && next_index_ < first_index + data_len ) {
    // data is cuurent next bytes
    auto start = next_index_ - first_index;
    auto substr_len = data_len - start;
    substr_len = min( substr_len, cur_cap );
    output.push( data.substr( start, substr_len ) );

    // data may overlap current cache, clear pending bytes
    cur_cap -= substr_len;
    next_index_ += substr_len;
    for ( uint64_t offset = 0; offset < substr_len; ++offset ) {
      auto i = ( buf_index_ + offset ) % cap_;
      if ( vis[i] ) {
        vis[i] = false;
        bytes_pending_ -= 1;
      }
    }
    buf_index_ = ( buf_index_ + substr_len ) % cap_;

    handle_contiguous_caches( cur_cap, output );
  }

  if ( last_index_ > -1 && next_index_ == static_cast<uint64_t>( last_index_ ) ) {
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}

// check whether there are subsequent bytes and if so, write output
void Reassembler::handle_contiguous_caches( uint64_t cur_cap, Writer& output )
{
  if ( vis[buf_index_] && cur_cap > 0 ) {
    string buf;
    for ( uint64_t i = 0; i < cur_cap; ++i ) {
      auto ii = ( i + buf_index_ ) % cap_;
      if ( !vis[ii] ) {
        break;
      }
      vis[ii] = false;
      bytes_pending_ -= 1;
      buf.push_back( buffer_[ii] );
    }
    output.push( buf );
    auto buf_len = buf.size();
    next_index_ += buf_len;
    buf_index_ = ( buf_index_ + buf_len ) % cap_;
  }
}