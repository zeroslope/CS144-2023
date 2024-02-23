#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return bytes_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return {};
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if ( !msg_queue_.empty() ) {
    auto msg = msg_queue_.front();
    bytes_in_flight_ += msg.sequence_length();
    outstanding_msg_queue_.push( msg );
    msg_queue_.pop();
    return msg;
  }
  return {};
}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  if ( !has_syn_ ) {
    has_syn_ = true;
    TCPSenderMessage msg {};
    msg.seqno = isn_;
    msg.SYN = true;
    msg_queue_.push( msg );
    abs_idx_ += msg.sequence_length();
  }

  if ( outbound_stream.bytes_buffered() > 0 ) {
    // TODO: window size ?
    auto payload = outbound_stream.peek();
    outbound_stream.pop(payload.size());
    
    auto buffer = Buffer( string( payload ) );
    auto seqno = Wrap32::wrap( abs_idx_, isn_ );
    TCPSenderMessage msg { seqno, false, buffer, false };
    abs_idx_ += msg.sequence_length();
    msg_queue_.push( msg );
  }

  if ( outbound_stream.is_finished() ) {}
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  auto seqno = Wrap32::wrap( abs_idx_, isn_ );
  return { seqno };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( !msg.ackno.has_value() ) {
    return;
  }
  auto ackno = msg.ackno.value().unwrap( isn_, abs_idx_ );
  while ( !outstanding_msg_queue_.empty() ) {
    auto seqno = outstanding_msg_queue_.front().seqno.unwrap( isn_, abs_idx_ );
    if ( seqno < ackno ) {
      bytes_in_flight_ -= outstanding_msg_queue_.front().sequence_length();
      outstanding_msg_queue_.pop();
    } else {
      break;
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  (void)ms_since_last_tick;
}
