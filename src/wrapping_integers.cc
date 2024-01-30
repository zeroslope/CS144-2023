#include "wrapping_integers.hh"

using namespace std;

const uint64_t MOD = 1UL << 32;

uint64_t sub_abs( uint64_t a, uint64_t b )
{
  if ( b > a ) {
    return b - a;
  }
  return a - b;
}

uint64_t min( uint64_t a, uint64_t b )
{
  if ( a < b ) {
    return a;
  }
  return b;
}

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( ( n % MOD + zero_point.raw_value_ ) % MOD ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // auto raw_value = static_cast<uint64_t>(zero_point.raw_value_);
  // raw_value_
  /**
   * checkpoint: round * MOD + remain
   * this <  zero_point
   * (MOD - zero_point + this)
   * ---
   * this >= zero_point
   * (this - zero_point)
   * */
  const uint64_t round = ( checkpoint / MOD );

  uint64_t val = 0;
  if ( raw_value_ < zero_point.raw_value_ ) {
    val = static_cast<uint64_t>( MOD - zero_point.raw_value_ + raw_value_ );
  } else {
    val = static_cast<uint64_t>( raw_value_ - zero_point.raw_value_ );
  }
  const uint64_t v1 = ( ( round >= 1UL ) ? ( round - 1 ) : 0 ) * MOD + val;
  const uint64_t v2 = round * MOD + val;
  const uint64_t v3 = ( round + 1 ) * MOD + val;
  if ( sub_abs( v1, checkpoint ) <= sub_abs( v2, checkpoint )
       && sub_abs( v1, checkpoint ) <= sub_abs( v3, checkpoint ) ) {
    return v1;
  }
  if ( sub_abs( v2, checkpoint ) <= sub_abs( v1, checkpoint )
       && sub_abs( v2, checkpoint ) <= sub_abs( v3, checkpoint ) ) {
    return v2;
  }
  return v3;
}
