#include "Utilities.h"

#include <iomanip>
#include <sstream>

std::string Utilities::formatMoney(const float amount)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << amount;
    return stream.str();
}
