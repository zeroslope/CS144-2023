#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , RTO_timeout_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return bytes_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmission_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( !msg_queue_.empty() ) {
    auto msg = std::move( msg_queue_.front() );
    msg_queue_.pop();
    return msg;
  }
  return std::nullopt;
}

bool TCPSender::is_window_not_full( uint64_t window_size )
{
  return window_size > bytes_in_flight_;
}

void TCPSender::fill_window( Reader& outbound_stream )
{
  if ( end_ ) {
    return;
  }

  TCPSenderMessage msg {};
  uint64_t window_size = receiver_window_size_ == 0 ? 1 : receiver_window_size_;

  if ( next_seqno_ == 0 ) {
    msg.SYN = true;
  }
  msg.seqno = Wrap32::wrap( next_seqno_, isn_ );

  uint64_t length = std::min( std::min( window_size - bytes_in_flight_, outbound_stream.bytes_buffered() ),
                              TCPConfig::MAX_PAYLOAD_SIZE );
  if ( length > 0 ) {
    auto payload = outbound_stream.peek();
    msg.payload = Buffer( string( payload.substr( 0, length ) ) );
    outbound_stream.pop( length );
  }
  if ( outbound_stream.is_finished() && msg.sequence_length() + bytes_in_flight_ < window_size ) {
    msg.FIN = true;
    end_ = true;
  }

  // ignore empty message
  if ( msg.sequence_length() == 0 ) {
    return;
  }

  if ( outstanding_msg_queue_.empty() ) {
    retransmission_timer_ = tick_;
  }

  msg_queue_.push( msg );
  outstanding_msg_queue_.push( msg );

  next_seqno_ += msg.sequence_length();
  bytes_in_flight_ += msg.sequence_length();

  if ( is_window_not_full( window_size ) ) {
    fill_window( outbound_stream );
  }
}

void TCPSender::push( Reader& outbound_stream )
{
  fill_window( outbound_stream );
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  auto seqno = Wrap32::wrap( next_seqno_, isn_ );
  return { seqno };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( !msg.ackno.has_value() ) {
    receiver_window_size_ = msg.window_size;
    return;
  }
  auto ackno = msg.ackno.value().unwrap( isn_, next_seqno_ );
  if ( ackno > next_seqno_ ) {
    // impossible case, we should ignore this.
    return;
  }
  // TODO: 这合理吗
  receiver_window_size_ = msg.window_size;
  if ( outstanding_msg_queue_.empty() ) {
    return;
  }
  auto front_seqno = outstanding_msg_queue_.front().seqno.unwrap( isn_, next_seqno_ );
  if ( front_seqno >= ackno ) {
    return;
  }
  // pop all acked segments
  while ( !outstanding_msg_queue_.empty() ) {
    auto seqno = outstanding_msg_queue_.front().seqno.unwrap( isn_, next_seqno_ );
    if ( seqno + outstanding_msg_queue_.front().sequence_length() <= ackno ) {
      // printf( "%lu %lu\n", bytes_in_flight_, outstanding_msg_queue_.front().sequence_length() );
      bytes_in_flight_ -= outstanding_msg_queue_.front().sequence_length();
      outstanding_msg_queue_.pop();
    } else {
      break;
    }
  }
  // reset timer
  consecutive_retransmission_ = 0;
  RTO_timeout_ = initial_RTO_ms_;
  if ( !outstanding_msg_queue_.empty() ) {
    retransmission_timer_ = tick_;
  } else {
    retransmission_timer_ = std::nullopt;
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  tick_ += ms_since_last_tick;
  if ( !outstanding_msg_queue_.empty() && tick_ - retransmission_timer_.value() >= RTO_timeout_ ) {
    msg_queue_.push( outstanding_msg_queue_.front() );

    // 见文档中限制条件
    if ( receiver_window_size_ > 0 ) {
      RTO_timeout_ *= 2;
      consecutive_retransmission_ += 1;
    }
    retransmission_timer_ = tick_;
  }
}
