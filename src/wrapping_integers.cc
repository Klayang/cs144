#include "wrapping_integers.hh"
#include <math.h>
#include <stdint.h>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
    // (a + b) % c = (a % c + b % c) % c
    return Wrap32((uint32_t)n + zero_point.raw_value_);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
    uint64_t offset = (uint64_t)(this->raw_value_ - zero_point.raw_value_);
    uint64_t times = checkpoint / (uint64_t)pow(2, 32);
    uint64_t base = (uint64_t)UINT32_MAX + 1;
    uint64_t gap1 = base, gap2 = 0, gap3 = 0;
    if (times > 0) gap1 = get_gap_to_checkpoint(times - 1, checkpoint, offset);
    gap2 = get_gap_to_checkpoint(times, checkpoint, offset);
    gap3 = get_gap_to_checkpoint(times + 1, checkpoint, offset);

    if (gap1 < gap2) {
        if (gap1 < gap3) return (times - 1) * base + offset;
        else return (times + 1) * base + offset;
    }
    else {
        if (gap2 < gap3) return times * base + offset;
        else return (times + 1) * base + offset;
    }
}

uint64_t get_gap_to_checkpoint(uint64_t times, uint64_t checkpoint, uint64_t offset) {
    uint64_t absolute = times * (uint64_t)pow(2, 32) + offset;
    if (absolute > checkpoint) return absolute - checkpoint;
    else return checkpoint - absolute;
}
