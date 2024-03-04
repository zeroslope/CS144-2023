#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

ARPMessage make_arp_( const uint16_t opcode,
                      const EthernetAddress sender_ethernet_address,
                      const uint32_t& sender_ip_address,
                      const EthernetAddress target_ethernet_address,
                      const uint32_t& target_ip_address )
{
  ARPMessage arp;
  arp.opcode = opcode;
  arp.sender_ethernet_address = sender_ethernet_address;
  arp.sender_ip_address = sender_ip_address;
  arp.target_ethernet_address = target_ethernet_address;
  arp.target_ip_address = target_ip_address;
  return arp;
}

EthernetFrame make_frame_( const EthernetAddress& src,
                           const EthernetAddress& dst,
                           const uint16_t type,
                           vector<Buffer> payload )
{
  EthernetFrame frame;
  frame.header.src = src;
  frame.header.dst = dst;
  frame.header.type = type;
  frame.payload = std::move( payload );
  return frame;
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto next_hop_ip = next_hop.ipv4_numeric();
  if ( ip2mac_.contains( next_hop_ip ) && tick_ - ip_tick_[next_hop_ip] >= CACHE_TIMEOUT ) {
    ip2mac_.erase( next_hop_ip );
    ip_tick_.erase( next_hop_ip );
  }

  if ( ip2mac_.contains( next_hop_ip ) ) {
    auto payload = serialize( dgram );
    EthernetFrame eth_frame
      = make_frame_( ethernet_address_, ip2mac_[next_hop_ip], EthernetHeader::TYPE_IPv4, std::move( payload ) );
    frame_queue_.push( std::move( eth_frame ) );
  } else {
    if ( ip_tick_.contains( next_hop_ip ) && tick_ - ip_tick_[next_hop_ip] < DEBOUNCE_TIMEOUT ) {
      return;
    }
    ip_tick_[next_hop_ip] = tick_;
    // cache ip dgram
    ip_dgram_map_[next_hop_ip].push( dgram );

    ARPMessage arp {};
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ip_address = ip_address_.ipv4_numeric();
    arp.sender_ethernet_address = ethernet_address_;
    arp.target_ip_address = next_hop_ip;

    EthernetHeader header {};
    header.src = ethernet_address_;
    header.dst = ETHERNET_BROADCAST;
    header.type = EthernetHeader::TYPE_ARP;
    EthernetFrame frame {};
    frame.header = header;
    Serializer s1;
    arp.serialize( s1 );
    frame.payload = s1.output();

    frame_queue_.push( std::move( frame ) );
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage msg {};
    parse( msg, frame.payload );
    if ( msg.opcode == ARPMessage::OPCODE_REPLY && msg.target_ethernet_address == ethernet_address_ ) {
      ip_tick_[msg.sender_ip_address] = tick_;
      ip2mac_[msg.sender_ip_address] = msg.sender_ethernet_address;
      if ( ip_dgram_map_.contains( msg.sender_ip_address ) ) {
        auto& dgram_queue = ip_dgram_map_[msg.sender_ip_address];
        while ( !dgram_queue.empty() ) {
          auto payload = serialize( dgram_queue.front() );
          dgram_queue.pop();

          EthernetFrame eth_frame = make_frame_(
            ethernet_address_, msg.sender_ethernet_address, EthernetHeader::TYPE_IPv4, std::move( payload ) );
          frame_queue_.push( std::move( eth_frame ) );
        }
      }
    } else if ( msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric() ) {
      ip_tick_[msg.sender_ip_address] = tick_;
      // TODO: send quene messages
      ip2mac_[msg.sender_ip_address] = msg.sender_ethernet_address;
      auto arp = make_arp_( ARPMessage::OPCODE_REPLY,
                            ethernet_address_,
                            ip_address_.ipv4_numeric(),
                            frame.header.src,
                            msg.sender_ip_address );
      auto payload = serialize( arp );
      auto eth_frame
        = make_frame_( ethernet_address_, frame.header.src, EthernetHeader::TYPE_ARP, std::move( payload ) );
      frame_queue_.push( std::move( eth_frame ) );
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    if ( frame.header.dst == ethernet_address_ ) {
      InternetDatagram dgram {};
      parse( dgram, frame.payload );
      return dgram;
    }
  }
  return std::nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  tick_ += ms_since_last_tick;
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if ( !frame_queue_.empty() ) {
    auto frame = frame_queue_.front();
    frame_queue_.pop();
    return frame;
  }
  return std::nullopt;
}
