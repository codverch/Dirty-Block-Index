#ifndef __LEARNING_GEM5_YO_HH__
#define __LEARNING_GEM5_YO_HH__

#include <string>

#include "learning_gem5/part2/yo.hh"
#include "params/Yo.hh"
#include "sim/sim_object.hh"

namespace gem5
{

    class Yo : public SimObject
    {

    public:
        Yo(const YoParams &params);
    };

} // namespace gem5

#endif // __LEARNING_GEM5_YO_HH__
