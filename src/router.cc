#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  entries_.push_back( RouterEntry { route_prefix, prefix_length, next_hop, interface_num } );
}

void Router::route()
{
  for ( auto& face : interfaces_ ) {
    auto op_dgram = face.maybe_receive();
    while ( op_dgram.has_value() ) {
      auto dgram = op_dgram.value();
      // drop packet when ttl <= 1
      if ( dgram.header.ttl > 1 ) {
        // after ttl change, should recompute checksum
        dgram.header.ttl -= 1;
        dgram.header.compute_checksum();
        auto dst = dgram.header.dst;
        auto idx = match( dst );
        // if match
        if ( idx.has_value() ) {
          auto& entry = entries_[idx.value()];
          auto& dst_face = interface( entry.interface_num );
          if ( entry.next_hop.has_value() ) {
            dst_face.send_datagram( dgram, entry.next_hop.value() );
          } else {
            dst_face.send_datagram( dgram, Address::from_ipv4_numeric( dgram.header.dst ) );
          }
        }
      }
      op_dgram = face.maybe_receive();
    }
  }
}

std::optional<size_t> Router::match( uint32_t ip_addr )
{
  std::optional<size_t> idx;
  for ( size_t i = 0; i < entries_.size(); ++i ) {
    bool is_match = entries_[i].match( ip_addr );
    if ( is_match ) {
      if ( !idx.has_value() ) {
        idx = i;
      } else if ( entries_[i].prefix_length > entries_[idx.value()].prefix_length ) {
        idx = i;
      }
    }
  }
  return idx;
}
