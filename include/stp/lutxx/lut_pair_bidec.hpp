#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <algorithm>
#include <iostream>
#include <cstdint>
#include <numeric>
#include <unordered_map>

namespace stp::lutxx {

// =====================================================
// Utilities
// =====================================================
inline uint64_t pow2(int k) { return 1ull << k; }

// Map subgroup indices directly to positions in the original truth table.
// Indices preserve storage order: 11...1 -> 00...0.
inline std::vector<size_t> cofactor_offsets(
    const std::vector<int>& variables,
    const std::vector<int>& order)
{
    std::vector<size_t> offsets{0};
    offsets.reserve(size_t{1} << variables.size());
    for (auto it = variables.rbegin(); it != variables.rend(); ++it) {
        const auto position = std::find(order.begin(), order.end(), *it);
        const size_t bit = size_t{1} << (order.size() - 1 - (position - order.begin()));
        const size_t count = offsets.size();
        for (size_t i = 0; i < count; ++i)
            offsets.push_back(offsets[i] | bit);
    }
    return offsets;
}

// =====================================================
// Enumerate (x, y, z) in ascending order of y.
// Constraints: x+y <= K, y+z+1 <= K, x+y+z = n (K = LUT_PAIR_FANIN_LIMIT).
// =====================================================
inline std::vector<std::tuple<int,int,int>>
enumerate_xyz(int n)
{
    std::vector<std::tuple<int,int,int>> out;
    const int k = LUT_PAIR_FANIN_LIMIT;
    for (int y = 1; y <= std::max(1, k - 2); ++y)
    for (int x = 0; x <= k; ++x)
    {
        int z = n - x - y;
        if (z < 0) continue;
        if (x + y > k) continue;
        if (y + z + 1 > k) continue;
        out.emplace_back(x,y,z);
    }
    return out;
}

// =====================================================
// Enumerate variable partitions (A, B, C), preserving MSB-to-LSB order.
// =====================================================
inline std::vector<
    std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>>
enumerate_variable_partitions(const std::vector<int>& vars_msb2lsb,
                              int x, int y, int z)
{
    std::vector<
        std::tuple<std::vector<int>,std::vector<int>,std::vector<int>>> parts;

     std::vector<int> vars = vars_msb2lsb;
    int n = (int)vars.size();

    std::vector<bool> sel_b(n,false);
    std::fill(sel_b.begin(), sel_b.begin()+y, true);

    do {
        std::vector<int> B, rest;
        for (int i=0;i<n;i++)
            (sel_b[i]?B:rest).push_back(vars[i]);

        if (x == 0) {
            parts.emplace_back(std::vector<int>{}, B, rest);
            continue;
        }

        std::vector<bool> sel_a(rest.size(), false);
        std::fill(sel_a.begin(), sel_a.begin()+x, true);

        do {
            std::vector<int> A,C;
            for (size_t i=0;i<rest.size();i++)
                (sel_a[i]?A:C).push_back(rest[i]);
            parts.emplace_back(A,B,C);
        } while (std::prev_permutation(sel_a.begin(), sel_a.end()));

    } while (std::prev_permutation(sel_b.begin(), sel_b.end()));

    return parts;
}

// Extract and merge one cofactor at a time without materializing a
// reordered truth table or the expanded MXY matrix.
inline bool solve_MX_MY_from_cofactors(
    const std::string& mf,
    const std::vector<int>& order,
    const std::vector<int>& a,
    const std::vector<int>& b,
    const std::vector<int>& c,
    std::string& MX,          // Original output: used by the DAG (x -> 0).
    std::string& MX_with_x,   // Additional output: used for printing (preserves x).
    std::string& MY)
{
    const auto a_offsets = cofactor_offsets(a, order);
    const auto b_offsets = cofactor_offsets(b, order);
    const auto c_offsets = cofactor_offsets(c, order);
    const size_t MYN = a_offsets.size() * b_offsets.size();
    const size_t block_size = b_offsets.size() * c_offsets.size();

    // Concrete blocks: resolve x during merging wherever possible.
    std::vector<std::string> blocks;
    // Symbolic blocks: preserve each block as first encountered, including x; merging leaves them unchanged.
    std::vector<std::string> blocks_symbolic;

    MY.resize(MYN);

    for (uint64_t i=0;i<MYN;i++)
    {
        const size_t a_index = i / b_offsets.size();
        const size_t b_index = i % b_offsets.size();
        const size_t fixed_offset = a_offsets[a_index] | b_offsets[b_index];
        std::string blk(block_size, 'x');
        // The shared B assignment occupies only its diagonal slice;
        // all other slices remain unconstrained, as in the original MXY.
        for (size_t j = 0; j < c_offsets.size(); ++j)
            blk[b_index * c_offsets.size() + j] = mf[fixed_offset | c_offsets[j]];

        bool hit=false;
        for (size_t k=0;k<blocks.size();k++)
        {
            bool ok=true;
            for (uint64_t j=0;j<block_size;j++)
            {
                if (blk[j]!='x' && blocks[k][j]!='x'
                    && blk[j]!=blocks[k][j]) { ok=false; break; }
            }

            if (ok)
            {
                // Update only concrete blocks: fill existing x entries with values from blk.
                for (uint64_t j=0;j<block_size;j++)
                    if (blocks[k][j]=='x') blocks[k][j]=blk[j];

                // Leave symbolic blocks unchanged to preserve the positions of x entries.
                MY[i]=(k==0?'1':'0');
                hit=true; break;
            }
        }

        if (!hit)
        {
            if (blocks.size()==2) return false;
            blocks.push_back(blk);
            blocks_symbolic.push_back(blk);
            MY[i]=(blocks.size()==1?'1':'0');
        }
    }

    if (blocks.size()!=2) return false;

    // ===== Output MX_with_x (preserve x).=====
    MX_with_x.clear();
    for (auto& b: blocks_symbolic)
        for (char c: b)
            MX_with_x.push_back(c);

    // ===== Output MX (preserve existing behavior: x -> 0 for the DAG).=====
    MX.clear();
    for (auto& b: blocks)
        for (char c: b)
            MX.push_back(c=='x'?'0':c);

    return true;
}

// =====================================================
// Construct children (including placeholders).
// =====================================================
} // namespace stp::lutxx

