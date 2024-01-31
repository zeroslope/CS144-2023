#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  bool is_last_substring = false;
  // 忽略后续收到的 SYN 包
  if ( message.SYN && isn_.has_value() ) {
    send( inbound_stream );
    return;
  }
  if ( message.SYN ) {
    isn_ = message.seqno;
  }
  if ( !isn_.has_value() ) {
    send( inbound_stream );
    return;
  }
  if ( message.FIN ) {
    is_last_substring = true;
  }
  Wrap32 seqno { message.seqno };
  if ( message.SYN ) {
    seqno = seqno + 1;
  }
  auto absolute = seqno.unwrap( isn_.value(), inbound_stream.bytes_pushed() );
  if ( absolute > 0 ) {
    reassembler.insert( absolute - 1, message.payload, is_last_substring, inbound_stream );
  }
  send( inbound_stream );
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  auto window_size
    = static_cast<uint16_t>( min( inbound_stream.available_capacity(), static_cast<uint64_t>( UINT16_MAX ) ) );
  if ( !isn_.has_value() ) {
    return { std::nullopt, window_size };
  }
  auto absolute_seqno = inbound_stream.bytes_pushed() + 1;
  if ( inbound_stream.is_closed() ) {
    absolute_seqno += 1;
  }
  return { Wrap32::wrap( absolute_seqno, isn_.value() ), window_size };
}
