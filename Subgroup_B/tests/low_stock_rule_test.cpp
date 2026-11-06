// Tests for the low stock rule: one test per row of the check() table,
// plus the boundary at exactly 25%.
#include <cassert>
#include <cstdio>

#include "behaviour_hiding/low_stock_rule.hpp"

static void row1_zero_capacity_rejected() {
    LowStockRule rule;
    assert(rule.check(10, 0) == LowStockVerdict::InvalidCapacity);
}

static void row1_negative_capacity_rejected() {
    LowStockRule rule;
    assert(rule.check(10, -5) == LowStockVerdict::InvalidCapacity);
}

static void row2_negative_count_rejected() {
    LowStockRule rule;
    assert(rule.check(-1, 100) == LowStockVerdict::InvalidCount);
}

static void row3_below_25_percent_is_low() {
    LowStockRule rule;
    assert(rule.check(24, 100) == LowStockVerdict::Low);
}

static void row3_zero_count_is_low() {
    LowStockRule rule;
    assert(rule.check(0, 100) == LowStockVerdict::Low);
}

static void row3_just_below_25_percent_with_odd_capacity_is_low() {
    LowStockRule rule;   // 25% of 10 is 2.5: 2 is below it
    assert(rule.check(2, 10) == LowStockVerdict::Low);
}

static void row4_exactly_25_percent_is_not_low() {
    LowStockRule rule;
    assert(rule.check(25, 100) == LowStockVerdict::NotLow);
}

static void row4_above_25_percent_is_not_low() {
    LowStockRule rule;
    assert(rule.check(3, 10) == LowStockVerdict::NotLow);
    assert(rule.check(100, 100) == LowStockVerdict::NotLow);
}

static void rows_checked_in_order() {
    LowStockRule rule;   // breaks rows 1 and 2 at once: row 1 wins
    assert(rule.check(-1, 0) == LowStockVerdict::InvalidCapacity);
}

int main() {
    row1_zero_capacity_rejected();
    row1_negative_capacity_rejected();
    row2_negative_count_rejected();
    row3_below_25_percent_is_low();
    row3_zero_count_is_low();
    row3_just_below_25_percent_with_odd_capacity_is_low();
    row4_exactly_25_percent_is_not_low();
    row4_above_25_percent_is_not_low();
    rows_checked_in_order();
    std::printf("low_stock_rule_test: 9 tests passed\n");
    return 0;
}
