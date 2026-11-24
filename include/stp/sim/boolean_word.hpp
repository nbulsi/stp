#pragma once
#include <cstddef>
#include <cstdint>

#ifdef __CUDACC__
#define STP_WORD_HD __host__ __device__
#else
#define STP_WORD_HD
#endif

namespace stp {
enum class BooleanOp : uint32_t { Zero, One, And, Nand, Or, Nor, Xor, Xnor, Anf, Sop };
struct BooleanGate {
  size_t input_offset = 0;
  size_t term_offset = 0;
  size_t term_count = 0;
  uint32_t input_count = 0;
  uint32_t output_slot = 0;
  BooleanOp op = BooleanOp::Zero;
  uint32_t invert = 0;
};

// Each word holds 64 independent patterns. Input order is MSB -> LSB,
// matching Gate::get_inputs() and the existing simulator.
STP_WORD_HD inline uint64_t evaluate_boolean_word(
    const BooleanGate &gate, const uint32_t *inputs, const uint32_t *terms,
    const uint64_t *values, size_t stride, size_t word)
{
  if (gate.op == BooleanOp::Zero) return 0;
  if (gate.op == BooleanOp::One) return ~uint64_t{0};
  if (gate.op != BooleanOp::Anf && gate.op != BooleanOp::Sop) {
    const bool conjunction = gate.op == BooleanOp::And || gate.op == BooleanOp::Nand;
    const bool disjunction = gate.op == BooleanOp::Or || gate.op == BooleanOp::Nor;
    uint64_t result = conjunction ? ~uint64_t{0} : 0;
    for (uint32_t i = 0; i < gate.input_count; ++i) {
      uint64_t value = values[size_t(inputs[gate.input_offset+i])*stride+word];
      if (conjunction) result &= value;
      else if (disjunction) result |= value;
      else result ^= value;
    }
    return gate.op == BooleanOp::Nand || gate.op == BooleanOp::Nor ||
                   gate.op == BooleanOp::Xnor ? ~result : result;
  }
  uint64_t result = 0;
  for (size_t t = 0; t < gate.term_count; ++t) {
    uint32_t mask = terms[gate.term_offset+t];
    uint64_t product = ~uint64_t{0};
    for (uint32_t i = 0; i < gate.input_count; ++i) {
      bool selected = ((mask >> (gate.input_count-1-i)) & 1u) != 0;
      if (gate.op == BooleanOp::Anf && !selected) continue;
      uint64_t value = values[size_t(inputs[gate.input_offset+i])*stride+word];
      product &= selected ? value : ~value;
    }
    if (gate.op == BooleanOp::Anf) result ^= product;
    else result |= product;
  }
  return gate.invert ? ~result : result;
}
} // namespace stp
#undef STP_WORD_HD

