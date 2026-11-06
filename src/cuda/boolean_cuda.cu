#include <stp/sim/boolean_plan.hpp>
#include <cuda_runtime.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace stp {
namespace {
void check(cudaError_t error, const char *operation) {
  if (error != cudaSuccess)
    throw std::runtime_error(std::string(operation)+": "+cudaGetErrorString(error));
}
template <class T> class DeviceBuffer {
public:
  explicit DeviceBuffer(size_t count) {
    if (count > std::numeric_limits<size_t>::max()/sizeof(T))
      throw std::length_error("CUDA allocation size overflow");
    if (count) check(cudaMalloc(reinterpret_cast<void **>(&data),count*sizeof(T)),
                     "Boolean CUDA allocation");
  }
  explicit DeviceBuffer(const std::vector<T> &values):DeviceBuffer(values.size()) {
    if (!values.empty()) {
      auto status=cudaMemcpy(data,values.data(),values.size()*sizeof(T),cudaMemcpyHostToDevice);
      if (status!=cudaSuccess) {
        cudaFree(data); data=nullptr;
        check(status,"Boolean plan upload");
      }
    }
  }
  ~DeviceBuffer() { if (data) cudaFree(data); }
  DeviceBuffer(const DeviceBuffer &)=delete;
  DeviceBuffer &operator=(const DeviceBuffer &)=delete;
  T *data=nullptr;
};

__global__ void initialize_patterns(uint64_t *values, const uint32_t *slots,
                                    size_t input_count, size_t words,
                                    size_t stride, size_t first_word)
{
  size_t count=input_count*words;
  for (size_t task=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
       task<count; task+=size_t(blockDim.x)*gridDim.x) {
    size_t input=task/words, word=task%words;
    uint64_t value;
    switch (input) {
      case 0: value=0x5555555555555555ULL; break;
      case 1: value=0x3333333333333333ULL; break;
      case 2: value=0x0F0F0F0F0F0F0F0FULL; break;
      case 3: value=0x00FF00FF00FF00FFULL; break;
      case 4: value=0x0000FFFF0000FFFFULL; break;
      case 5: value=0x00000000FFFFFFFFULL; break;
      default: value=((first_word+word)>>(input-6))&1 ? 0 : ~uint64_t{0};
    }
    values[size_t(slots[input])*stride+word]=value;
  }
}

__global__ void evaluate_level(const BooleanGate *gates, size_t gate_count,
                               const uint32_t *inputs, const uint32_t *terms,
                               uint64_t *values, size_t stride, size_t words)
{
  const size_t count=gate_count*words;
  for (size_t task=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
       task<count; task+=size_t(blockDim.x)*gridDim.x) {
    const auto gate=gates[task/words];
    const size_t word=task%words;
    values[size_t(gate.output_slot)*stride+word]=
        evaluate_boolean_word(gate,inputs,terms,values,stride,word);
  }
}

unsigned blocks_for(size_t count) {
  return static_cast<unsigned>(std::min<size_t>(65535,count/256+(count%256!=0)));
}
} // namespace

std::vector<std::vector<uint64_t>> simulate_boolean_cuda(
    const BooleanPlan &plan, size_t pattern_count, size_t batch_words)
{
  if (!batch_words || !pattern_count ||
      plan.input_slots.size() >= std::numeric_limits<size_t>::digits ||
      pattern_count != (size_t{1} << plan.input_slots.size()))
    throw std::invalid_argument("invalid Boolean CUDA pattern count or batch size");
  const size_t total_words=pattern_count/64+(pattern_count%64!=0);
  std::vector<std::vector<uint64_t>> outputs(plan.output_slots.size(),
                                             std::vector<uint64_t>(total_words));
  if (outputs.empty()) return outputs;
  if (!plan.slot_count) throw std::invalid_argument("empty Boolean CUDA signal storage");
  DeviceBuffer<BooleanGate> gates(plan.gates);
  DeviceBuffer<uint32_t> inputs(plan.inputs), terms(plan.terms), pi_slots(plan.input_slots);
  size_t free_bytes=0,total_bytes=0;
  check(cudaMemGetInfo(&free_bytes,&total_bytes),"query Boolean CUDA memory");
  // Leave half the currently available memory to the driver and other work.
  const size_t affordable_words=(free_bytes/2/sizeof(uint64_t))/plan.slot_count;
  const size_t stride=std::min({batch_words,total_words,affordable_words});
  if (!stride) throw std::runtime_error("insufficient GPU memory for one Boolean pattern word");
  if (plan.slot_count>std::numeric_limits<size_t>::max()/stride)
    throw std::length_error("Boolean CUDA workspace size overflow");
  DeviceBuffer<uint64_t> values(plan.slot_count*stride);
  for (size_t first=0; first<total_words; first+=stride) {
    const size_t words=std::min(stride,total_words-first);
    if (!plan.input_slots.empty()) {
      initialize_patterns<<<blocks_for(plan.input_slots.size()*words),256>>>(
          values.data,pi_slots.data,plan.input_slots.size(),words,stride,first);
      check(cudaGetLastError(),"launch Boolean pattern initialization");
    }
    for (size_t level=1; level<plan.level_offsets.size(); ++level) {
      const size_t begin=plan.level_offsets[level-1];
      const size_t count=plan.level_offsets[level]-begin;
      if (!count) continue;
      evaluate_level<<<blocks_for(count*words),256>>>(
          gates.data+begin,count,inputs.data,terms.data,values.data,stride,words);
      check(cudaGetLastError(),"launch Boolean level");
      // Default-stream ordering enforces dependencies without a host barrier.
    }
    for (size_t output=0; output<outputs.size(); ++output)
      check(cudaMemcpy(outputs[output].data()+first,
                       values.data+size_t(plan.output_slots[output])*stride,
                       words*sizeof(uint64_t),cudaMemcpyDeviceToHost),
            "copy Boolean output"); // Also surfaces asynchronous kernel errors.
  }
  if (pattern_count%64)
    for (auto &output:outputs) output.back() &= (uint64_t{1}<<(pattern_count%64))-1;
  return outputs;
}
} // namespace stp

