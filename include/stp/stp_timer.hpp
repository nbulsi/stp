/* stp: C++ semi-tensor product library for electronic design automation (EDA)
 * Copyright (C) 2023-  Ningbo University, Zhejiang, China
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*!
  \file stp_timer.hpp
  \brief header file for a simple timer
  \author Zhufei Chu
  \author Ruibing Zhang
*/

#pragma once

#include <fmt/format.h>
#include <chrono>
#include <iostream>
#include <type_traits>

template <class Clock = std::chrono::steady_clock>
class stopwatch
{
 public:
  using clock = Clock;
  using duration = typename Clock::duration;
  using time_point = typename Clock::time_point;

  /*! \brief Default constructor.
   *
   * Starts tracking time.
   */
  explicit stopwatch( duration& dur ) : dur( dur ), beg( clock::now() ) {}

  /*! \brief Default deconstructor.
   *
   * Stops tracking time and updates duration.
   */
  ~stopwatch() { dur += ( clock::now() - beg ); }

 private:
  duration& dur;
  time_point beg;
};

/*! \brief Calls a function and tracks time.
 *
 * The function that is passed as second parameter can be any callable object
 * that takes no parameters.  This construction can be used to avoid
 * pre-declaring the result type of a computation that should be tracked.
 *
   \verbatim embed:rst

   Example

   .. code-block:: c++

      stopwatch<>::duration time{0};

      auto result = call_with_stopwatch( time, [&]() { return function(
 parameters ); } ); \endverbatim
 *
 * \param dur Duration reference (time will be added to it)
 * \param fn Callable object with no arguments
 */

template <class Fn, class Clock = std::chrono::steady_clock>
std::invoke_result_t<Fn> call_with_stopwatch( typename Clock::duration& dur,
                                              Fn&& fn )
{
  stopwatch<Clock> t( dur );
  return fn();
}

template <class Duration>
inline double to_seconds( Duration const& dur )
{
  return std::chrono::duration_cast<std::chrono::duration<double>>( dur )
      .count();
}

template <class Duration>
inline double to_millisecond( Duration const& dur )
{
  return 1000 * std::chrono::duration_cast<std::chrono::duration<double>>( dur )
                    .count();
}