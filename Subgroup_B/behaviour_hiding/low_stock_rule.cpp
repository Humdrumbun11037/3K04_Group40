//low stock rule

#include "behaviour_hiding/low_stock_rule.hpp"

// The threshold. Changing it means changing only this number.
static const long long LOW_STOCK_PERCENT = 25;

// Rows are checked in table order, so the first matching row wins.
// Whole-number arithmetic (count * 100 < capacity * 25) avoids rounding errors,
// so exactly 25% is reliably NotLow.
LowStockVerdict LowStockRule::check(int count, int capacity) const {
    if (capacity <= 0)                                     return LowStockVerdict::InvalidCapacity; // row 1
    if (count < 0)                                         return LowStockVerdict::InvalidCount;    // row 2
    if (count * 100LL < capacity * LOW_STOCK_PERCENT)      return LowStockVerdict::Low;             // row 3
    return LowStockVerdict::NotLow;                                                                 // row 4
}
