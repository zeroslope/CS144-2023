#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <queue>

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  uint64_t next_seqno_ { 0 };
  std::queue<TCPSenderMessage> msg_queue_ {};
  std::queue<TCPSenderMessage> outstanding_msg_queue_ {};
  bool end_ { false };
  uint64_t bytes_in_flight_ { 0 };
  // 初始化为1，否则 tick 里面 RTO_timeout_ 判断会出错
  uint64_t receiver_window_size_ { 1 };

  uint64_t tick_ { 0 };
  uint64_t consecutive_retransmission_ { 0 };
  uint64_t RTO_timeout_;
  std::optional<uint64_t> retransmission_timer_ {};

  void fill_window( Reader& outbound_stream );
  bool is_window_not_full( uint64_t window_size );

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
