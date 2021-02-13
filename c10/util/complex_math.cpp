#include <c10/util/complex.h>

#include <cmath>
#include <istream>
#include <limits>
#include <iomanip>

// Note [ Complex Square root in libc++]
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// In libc++ complex square root is computed using polar form
// This is a reasonably fast algorithm, but can result in significant
// numerical errors when arg is close to 0, pi/2, pi, or 3pi/4
// In that case provide a more conservative implementation which is
// slower but less prone to those kinds of errors

#ifdef _LIBCPP_VERSION

namespace {
template<typename T>
c10::complex<T> compute_csqrt(const c10::complex<T>& z) {
  constexpr auto half = T(.5);

  // Trust standard library to correctly handle infs and NaNs
  if (std::isinf(z.real()) || std::isinf(z.imag()) ||
      std::isnan(z.real()) || std::isnan(z.imag())) {
    return static_cast<c10::complex<T>>(std::sqrt(static_cast<std::complex<T>>(z)));
  }

  // Special case for square root of pure imaginary values
  if (z.real() == T(0)) {
    if (z.imag() == T(0)) {
      return c10::complex<T>(T(0), z.imag());
    }
    auto v = std::sqrt(half * std::abs(z.imag()));
    return c10::complex<T>(v, std::copysign(v, z.imag()));
  }

  // At this point, z is non-zero and finite
  if (z.real() >= 0.0) {
    auto t = std::sqrt((z.real() + std::abs(z)) * half);
    return c10::complex<T>(t, half * (z.imag() / t));
  }

  auto t = std::sqrt((-z.real() + std::abs(z)) * half);
  return c10::complex<T>(half * std::abs(z.imag() / t), std::copysign(t, z.imag()));
}
} // anonymous namespace

namespace c10_complex_math { namespace _detail {
c10::complex<float> sqrt(const c10::complex<float>& in) {
  return compute_csqrt(in);
}

c10::complex<double> sqrt(const c10::complex<double>& in) {
  return compute_csqrt(in);
}

}}
#endif
