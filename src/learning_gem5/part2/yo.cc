#include "learning_gem5/part2/yo.hh"

#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/Yo.hh"

namespace gem5
{

    Yo::Yo(const YoParams &params) : SimObject(params)
    {
        std::cout << "Hello World! From a SimObject!" << std::endl;
    }

} // namespace gem5
