#include "UID.h"

#include <atomic>
#include <ostream>

using namespace osc;

constinit std::atomic<UID::element_type> osc::UID::g_next_available_id = 1;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

std::ostream& osc::operator<<(std::ostream& out, const UID& id)
{
    return out << id.get();
}
