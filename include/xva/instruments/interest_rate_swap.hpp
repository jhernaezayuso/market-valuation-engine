/// @file interest_rate_swap.hpp
/// @brief Financial instrument representation of an Interest Rate Swap (IRS).

#pragma once

// std
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace xva::instruments
{

  /// @enum SwapType
  /// @brief Determines the direction of the fixed cash flows.
  /// @details Bound to a single byte to maximize cache density in large portfolios.
  enum class SwapType : std::uint8_t
  {
    Payer,
    Receiver
  };

  /// @struct CashflowPeriod
  /// @brief High-performance representation of a single payment period.
  struct CashflowPeriod
  {
    double start_time;
    double payment_time;
    double accrual_fraction;
  };

  /// @struct InterestRateSwapTerms
  /// @brief Contractual terms grouping to prevent parameter swapping bugs.
  struct InterestRateSwapTerms
  {
    SwapType type;
    double notional;
    double fixed_rate;
  };

  /// @class InterestRateSwap
  /// @brief Contractual data model for a plain-vanilla Interest Rate Swap.
  class InterestRateSwap
  {
   public:
    /// @brief Constructs an Interest Rate Swap and precomputes its cashflow schedule.
    /// @param terms The contractual terms of the swap (type, notional, fixed rate).
    /// @param schedule_times A strictly increasing sequence of times (in years) representing
    /// the start date and subsequent payment dates.
    InterestRateSwap(const InterestRateSwapTerms& terms, const std::vector<double>& schedule_times)
        : type_(terms.type), notional_(terms.notional), fixed_rate_(terms.fixed_rate)
    {
      if (schedule_times.size() < 2) [[unlikely]]
      {
        throw std::invalid_argument("Swap schedule must contain at least a start time and one payment time.");
      }

      periods_.reserve(schedule_times.size() - 1);

      for (std::size_t i = 1; i < schedule_times.size(); ++i)
      {
        const double start = schedule_times[i - 1];
        const double payment = schedule_times[i];

        if (payment <= start) [[unlikely]]
        {
          throw std::invalid_argument("Swap schedule times must be strictly increasing.");
        }

        periods_.push_back({ start, payment, payment - start });
      }
    }

    [[nodiscard]] auto type() const -> SwapType
    {
      return type_;
    }

    [[nodiscard]] auto notional() const -> double
    {
      return notional_;
    }

    [[nodiscard]] auto fixed_rate() const -> double
    {
      return fixed_rate_;
    }

    [[nodiscard]] auto periods() const -> const std::vector<CashflowPeriod>&
    {
      return periods_;
    }

   private:
    SwapType type_;
    double notional_;
    double fixed_rate_;
    std::vector<CashflowPeriod> periods_;
  };

}  // namespace xva::instruments
