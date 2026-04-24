/// @file interest_rate_swap_test.cpp
/// @brief Unit tests validating the construction and schedule generation of the IRS instrument.

// xva
#include "xva/instruments/interest_rate_swap.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace xva::instruments::test
{

  /// @brief Tolerance for floating-point time calculations.
  constexpr double time_tolerance = 1e-9;

  /// @brief Verifies that providing a valid schedule generates the correct cashflow periods.
  TEST(InterestRateSwapTest, GeneratesCorrectCashflowPeriods)
  {
    constexpr double notional_val = 1'000'000.0;
    constexpr double fixed_rate_val = 0.03;

    constexpr InterestRateSwapTerms terms{ .type = SwapType::Payer,
                                           .notional = notional_val,
                                           .fixed_rate = fixed_rate_val };

    const std::vector<double> schedule = { 0.0, 0.5, 1.0, 1.5, 2.0 };

    const InterestRateSwap swap(terms, schedule);

    EXPECT_EQ(swap.type(), SwapType::Payer);
    EXPECT_DOUBLE_EQ(swap.notional(), notional_val);
    EXPECT_DOUBLE_EQ(swap.fixed_rate(), fixed_rate_val);

    const auto& periods = swap.periods();

    constexpr std::size_t expected_num_periods = 4;
    EXPECT_EQ(periods.size(), expected_num_periods);

    EXPECT_NEAR(periods[0].start_time, 0.0, time_tolerance);
    EXPECT_NEAR(periods[0].payment_time, 0.5, time_tolerance);
    EXPECT_NEAR(periods[0].accrual_fraction, 0.5, time_tolerance);

    EXPECT_NEAR(periods[3].start_time, 1.5, time_tolerance);
    EXPECT_NEAR(periods[3].payment_time, 2.0, time_tolerance);
    EXPECT_NEAR(periods[3].accrual_fraction, 0.5, time_tolerance);
  }

  /// @brief Ensures the instrument rejects invalid schedule timelines to prevent silent pricing bugs.
  TEST(InterestRateSwapTest, ThrowsOnInvalidSchedules)
  {
    constexpr double notional_val = 1'000'000.0;
    constexpr double fixed_rate_val = 0.03;

    constexpr InterestRateSwapTerms terms{ .type = SwapType::Receiver,
                                           .notional = notional_val,
                                           .fixed_rate = fixed_rate_val };

    const std::vector<double> empty_schedule = {};
    const std::vector<double> short_schedule = { 0.0 };

    const std::vector<double> unordered_schedule = { 0.0, 1.0, 0.5 };
    const std::vector<double> flat_schedule = { 0.0, 1.0, 1.0 };

    EXPECT_THROW(InterestRateSwap(terms, empty_schedule), std::invalid_argument);
    EXPECT_THROW(InterestRateSwap(terms, short_schedule), std::invalid_argument);
    EXPECT_THROW(InterestRateSwap(terms, unordered_schedule), std::invalid_argument);
    EXPECT_THROW(InterestRateSwap(terms, flat_schedule), std::invalid_argument);
  }

}  // namespace xva::instruments::test
