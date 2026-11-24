#pragma once
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>
#include <stp/core/circuit_graph.hpp>
#include <stp/sim/boolean_word.hpp>

namespace stp {
struct CompiledBooleanLut {
  BooleanGate gate;
  std::vector<uint32_t> terms;
};

inline CompiledBooleanLut compile_boolean_lut(const stp_vec &lut, unsigned k)
{
  if (k >= 31 || lut.cols() != (size_t{1} << k) + 1 || lut(0) != 2)
    throw std::invalid_argument("Boolean backend requires a binary LUT with matching arity");
  const size_t count = size_t{1} << k;
  std::vector<uint8_t> truth(count);
  bool all_zero=true, all_one=true, is_and=true, is_nand=true,
       is_or=true, is_nor=true, is_xor=true, is_xnor=true;
  size_t ones = 0;
  for (size_t a=0; a<count; ++a) {
    if (lut(count-a) > 1) throw std::invalid_argument("nonbinary LUT entry");
    bool v = 1-lut(count-a);
    truth[a] = v; ones += v;
    all_zero &= !v; all_one &= v;
    bool conjunction = a == count-1, disjunction = a != 0;
    unsigned parity=0;
    for (size_t x=a; x; x>>=1) parity ^= x&1;
    is_and &= v == conjunction; is_nand &= v != conjunction;
    is_or &= v == disjunction; is_nor &= v != disjunction;
    is_xor &= v == bool(parity); is_xnor &= v != bool(parity);
  }
  CompiledBooleanLut result;
  result.gate.input_count=k;
  if (all_zero) result.gate.op=BooleanOp::Zero;
  else if (all_one) result.gate.op=BooleanOp::One;
  else if (is_and) result.gate.op=BooleanOp::And;
  else if (is_nand) result.gate.op=BooleanOp::Nand;
  else if (is_or) result.gate.op=BooleanOp::Or;
  else if (is_nor) result.gate.op=BooleanOp::Nor;
  else if (is_xor) result.gate.op=BooleanOp::Xor;
  else if (is_xnor) result.gate.op=BooleanOp::Xnor;
  else {
    // Boolean Mobius transform: f(x) = XOR of AND monomials.
    auto coefficients=truth;
    for (unsigned bit=0; bit<k; ++bit)
      for (size_t mask=0; mask<count; ++mask)
        if (mask & (size_t{1}<<bit)) coefficients[mask] ^= coefficients[mask^(size_t{1}<<bit)];
    size_t anf_cost=0;
    for (size_t mask=0; mask<count; ++mask)
      if (coefficients[mask]) {
        result.terms.push_back(static_cast<uint32_t>(mask));
        size_t cost=1;
        for (size_t x=mask; x; x>>=1) cost += x&1;
        anf_cost += cost;
      }
    // Dense ANFs may be worse than sparse minterms (or their complement).
    const size_t sop_cost=std::min(ones,count-ones)*(k+1);
    result.gate.op=BooleanOp::Anf;
    if (sop_cost < anf_cost) {
      result.terms.clear();
      result.gate.op=BooleanOp::Sop;
      result.gate.invert = ones > count-ones;
      for (size_t a=0; a<count; ++a)
        if (truth[a] != result.gate.invert) result.terms.push_back(static_cast<uint32_t>(a));
    }
    result.gate.term_count=result.terms.size();
  }
  return result;
}

struct BooleanPlan {
  std::vector<BooleanGate> gates;
  std::vector<size_t> level_offsets{0};
  std::vector<uint32_t> inputs, terms, input_slots, output_slots;
  size_t slot_count=0;
};

// The caller computes graph levels first. Slots released after a whole level
// cannot alias that level's inputs or outputs, even when CUDA runs gates in parallel.
inline BooleanPlan compile_boolean_plan(const CircuitGraph &graph,
                                        const std::vector<line_idx> &active_inputs)
{
  BooleanPlan plan;
  const auto &lines=graph.get_lines();
  std::vector<size_t> uses(lines.size(),0);
  std::vector<uint32_t> slots(lines.size(),std::numeric_limits<uint32_t>::max());
  std::vector<uint32_t> free_slots;
  auto allocate=[&]() {
    if (!free_slots.empty()) {
      auto slot=free_slots.back(); free_slots.pop_back(); return slot;
    }
    if (plan.slot_count >= std::numeric_limits<uint32_t>::max())
      throw std::length_error("too many Boolean signal slots");
    return static_cast<uint32_t>(plan.slot_count++);
  };
  for (const auto &level:graph.get_m_node_level())
    for (auto id:level)
      for (auto input:graph.get_gates()[id].get_inputs()) ++uses[input];
  for (auto input:active_inputs) {
    slots[input]=allocate(); plan.input_slots.push_back(slots[input]);
  }
  for (const auto &level:graph.get_m_node_level()) {
    for (auto id:level) {
      const auto &gate=graph.get_gates()[id];
      auto compiled=compile_boolean_lut(gate.get_type(),gate.get_inputs().size());
      compiled.gate.input_offset=plan.inputs.size();
      compiled.gate.term_offset=plan.terms.size();
      for (auto input:gate.get_inputs()) {
        if (slots[input]==std::numeric_limits<uint32_t>::max())
          throw std::runtime_error("Boolean plan input is not topologically initialized");
        plan.inputs.push_back(slots[input]);
      }
      slots[gate.get_output()]=allocate();
      compiled.gate.output_slot=slots[gate.get_output()];
      plan.terms.insert(plan.terms.end(),compiled.terms.begin(),compiled.terms.end());
      plan.gates.push_back(compiled.gate);
    }
    plan.level_offsets.push_back(plan.gates.size());
    for (auto id:level) {
      const auto &gate=graph.get_gates()[id];
      for (auto input:gate.get_inputs())
        if (--uses[input]==0 && !lines[input].is_output) {
          free_slots.push_back(slots[input]);
          slots[input]=std::numeric_limits<uint32_t>::max();
        }
    }
  }
  for (auto output:graph.get_outputs()) {
    if (slots[output]==std::numeric_limits<uint32_t>::max())
      throw std::runtime_error("Boolean plan output is uninitialized");
    plan.output_slots.push_back(slots[output]);
  }
  return plan;
}

#ifdef ENABLE_CUDA
// Output ordering follows graph outputs. batch_words is an upper bound; the
// allocator reduces it to fit available device memory. Results stay bit packed.
std::vector<std::vector<uint64_t>> simulate_boolean_cuda(
    const BooleanPlan &plan, size_t pattern_count, size_t batch_words=4096);
#endif
} // namespace stp

