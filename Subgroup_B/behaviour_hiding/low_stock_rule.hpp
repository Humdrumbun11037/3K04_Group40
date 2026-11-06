//low stock rule

/*
Hides: what counts as low stock (FR-INV-02): the 25% threshold, that it is
       compared against a capacity, and which side exactly 25% falls on (not low)

Interface
takes: the count and capacity of one medication, in one ADC (inventory record)
       or across the hospital (hospital stock)
sends: Low or NotLow, or a rejection for impossible inputs
uses: nothing
*/
#pragma once

enum class LowStockVerdict { NotLow, Low, InvalidCapacity, InvalidCount };

// Decides whether a medication is running low. Inventory record and hospital
// stock both ask it, so the 25% rule lives in exactly one place.
class LowStockRule {
public:
    // Answers "is this count low for this capacity?" so the pharmacy hears about it
    // before an ADC runs out.
    // { Pre: none (count and capacity belong to one medication) }
    //   check(count, capacity)
    // { Post: the first matching row decides the result:
    //         1. capacity <= 0                   => InvalidCapacity
    //         2. count < 0                       => InvalidCount
    //         3. count is strictly below 25%
    //            of capacity (0 included)        => Low
    //         4. otherwise (25% exactly or more) => NotLow }
    LowStockVerdict check(int count, int capacity) const;
};
