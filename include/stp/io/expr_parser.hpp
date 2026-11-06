// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <stp/sim/execute.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <limits>

#define STP_K 2


namespace stp
{
enum NODE_TYPE
{
  NodeType_None,
  NodeType_Gate,
  NodeType_Variable
};

enum GATE_TYPE
{
  GateType_NONE,
  GateType_W,
  GateType_I,
  GateType_Mr,
  GateType_Lut
};

class expr_node
{
public:
  // default constructor
  expr_node()
      : nodetype(NODE_TYPE::NodeType_None), gatetype(GATE_TYPE::GateType_NONE), dim_1(0), dim_2(0),
        var_id(0), var_k(0), lut_vec({})
  {
  }

  // init variable node
  expr_node(const NODE_TYPE &nt, const id &id, const stp_data &k = STP_K)
      : nodetype(nt), var_id(id), var_k(k)
  {
  }

  // init gate node
  expr_node(const NODE_TYPE &nt, const GATE_TYPE &gt, const stp_data &dim1, const stp_data &dim2,
            const std::vector<stp_data> &vec = {})
      : nodetype(nt), gatetype(gt), dim_1(dim1), dim_2(dim2), lut_vec(vec)
  {
  }

  // get node type
  const NODE_TYPE &Get_NodeType() const
  {
    return nodetype;
  }
  // get gate type
  const GATE_TYPE &Get_GateType() const
  {
    return gatetype;
  }
  // get variable id
  id Get_Var_id() const
  {
    return var_id;
  }
  // get variable k
  id Get_Var_k() const
  {
    return var_k;
  }
  // get dimension 1
  stp_data Get_dim1() const
  {
    return dim_1;
  }
  // get parameter 1
  stp_data Get_dim2() const
  {
    return dim_2;
  }

  // const
  const std::vector<stp_data> &Get_Lut_vec() const
  {
    return lut_vec;
  }
  std::vector<stp_data> &Get_Lut_vec()
  {
    return lut_vec;
  }

  void Set_Lut_vec(const std::vector<stp_data> &new_vec)
  {
    lut_vec = new_vec;
  }
  void Set_dim2(stp_data d2)
  {
    dim_2 = d2;
  }
  void Set_Var_id(id ID)
  {
    var_id = ID;
  }

private:
  NODE_TYPE nodetype;
  GATE_TYPE gatetype;
  stp_data dim_1;
  stp_data dim_2;
  id var_id;
  stp_data var_k;
  std::vector<stp_data> lut_vec;
};

class expr_chain_parser
{
public:
  // initialize
  expr_chain_parser(const std::vector<expr_node> &expr_chain,
                    const std::vector<int64_t> &old_pi_index, const bool use_cuda = false)
      : expr_chain(expr_chain), input_names(old_pi_index), use_cuda_(use_cuda)
  {
    //normalize_expr_chain();
    normalize_expr_chain_new();

    if (use_cuda_)
    {
#ifdef ENABLE_CUDA
      from_expr_to_matrix_cuda();
#else
      throw std::runtime_error("CUDA expression evaluation is unavailable in this build");
#endif
    }
    else
    {
      from_expr_to_matrix();
    }
    exchange_vars_mixed();
    //exchange_vars_by_W();
  }

  void print_expr_chain(const std::vector<expr_node> &expr_chain)
  {
    for (const auto &n : expr_chain)
    {
      if (n.Get_NodeType() == NodeType_Variable)
      {
        std::cout << " " << "id: " << n.Get_Var_id() << ", " << "k: " << n.Get_Var_k() << " "
                  << std::endl;
      }
      else
      {
        std::cout << " " << "dim1: " << n.Get_dim1() << " " << "dim2: " << n.Get_dim2() << " "
                  << "gate type: " << n.Get_GateType() << std::endl;
      }
    }
    std::cout << "---------------" << std::endl;
  }

  bool Is_Variable(const expr_node n)
  {
    return (n.Get_NodeType() == NodeType_Variable);
  }
  bool Is_Gate(const expr_node n)
  {
    return (n.Get_NodeType() == NodeType_Gate);
  }
  GATE_TYPE Get_GateType(const expr_node n)
  {
    return n.Get_GateType();
  }
  std::vector<expr_node> Get_Expr_Chain() const
  {
    return expr_chain;
  }

  void normalize_expr_chain(void)
  {
    // print_expr_chain(expr_chain);
    expr_chain = move_vars_to_rightside(expr_chain, pi_num);
    // print_expr_chain(expr_chain);
    expr_chain = sort_variables(expr_chain, pi_num);
    // print_expr_chain(expr_chain);
  }

  void normalize_expr_chain_new()
  {
      // 第一步：先合并重复变量
      expr_chain = merge_same_variables(expr_chain);

      // 第二步：再把变量统一右移
      expr_chain = move_vars_to_rightside_new(
          expr_chain, pi_num
      );

      // 第三步：建立独立变量顺序信息
      // 重复变量已消除，正常情况下不再插入 W/Mr
      if (pi_num != 0)
          expr_chain = sort_variables(expr_chain, pi_num);
      else
      {
          new_pi_list.clear();
          old_pi_list.clear();
      }
  }

    std::vector<expr_node> merge_same_variables(
      std::vector<expr_node> chain)
  {
      while (true)
      {
          size_t left = chain.size();
          size_t right = chain.size();

          // 1. 寻找一对重复变量
          for (size_t i = 0; i < chain.size(); ++i)
          {
              if (chain[i].Get_NodeType() != NodeType_Variable)
                  continue;

              for (size_t j = i + 1; j < chain.size(); ++j)
              {
                  if (chain[j].Get_NodeType() == NodeType_Variable &&
                      chain[i].Get_Var_id() == chain[j].Get_Var_id())
                  {
                      left = i;
                      right = j;
                      break;
                  }
              }

              if (right != chain.size())
                  break;
          }

          if (right == chain.size())
              break;

          const auto x = chain[left];
          const stp_data k = x.Get_Var_k();

          std::vector<expr_node> result;

          result.insert(
              result.end(),
              chain.begin(),
              chain.begin() + left
          );

          // 2. 把第一个重复变量向右移动
          for (size_t i = left + 1; i < right;)
          {
              if (chain[i].Get_NodeType() == NodeType_Variable)
              {
                  // X Y -> W Y X
                  // W 的参数顺序为 (Y的维度, X的维度)
                  result.emplace_back(
                      NodeType_Gate,
                      GateType_W,
                      chain[i].Get_Var_k(),
                      k
                  );

                  result.push_back(chain[i]);
                  ++i;
              }
              else
              {
                  // X M -> (I_k ⊗ M) X
                  stp_data dim = k;

                  // 兼容之前已经插入的 I
                  while (i < right &&
                        chain[i].Get_NodeType() == NodeType_Gate &&
                        chain[i].Get_GateType() == GateType_I)
                  {
                      if (chain[i].Get_dim1() == 0 ||
                          dim > std::numeric_limits<stp_data>::max()
                                    / chain[i].Get_dim1())
                          throw std::overflow_error("I dimension overflow");

                      dim *= chain[i].Get_dim1();
                      ++i;
                  }

                  if (i >= right)
                      throw std::runtime_error("Invalid I marker");

                  result.emplace_back(
                      NodeType_Gate,
                      GateType_I,
                      dim,
                      0
                  );

                  result.push_back(chain[i]);
                  ++i;
              }
          }

          // 3. X X -> Mr X
          result.emplace_back(
              NodeType_Gate,
              GateType_Mr,
              k,
              0
          );

          result.push_back(x);

          result.insert(
              result.end(),
              chain.begin() + right + 1,
              chain.end()
          );

          chain = std::move(result);
      }

      return chain;
  }


  std::vector<expr_node> move_vars_to_rightside_new(
    const std::vector<expr_node>& chain,
    stp_data& pi_num)
  {
      std::vector<expr_node> new_chain;
      std::vector<expr_node> pi_chain;

      stp_data var_dim = 1;
      stp_data pending_I = 1;

      auto multiply_dim = [](stp_data a, stp_data b) {
          if (b == 0 ||
              a > std::numeric_limits<stp_data>::max() / b)
              throw std::overflow_error("STP dimension overflow");
          return static_cast<stp_data>(a * b);
      };

      for (const auto& node : chain)
      {
          if (node.Get_NodeType() == NodeType_Variable)
          {
              if (pending_I != 1)
                  throw std::runtime_error("I before variable");

              pi_chain.push_back(node);
              var_dim = multiply_dim(var_dim, node.Get_Var_k());
          }
          else if (node.Get_GateType() == GateType_I)
          {
              pending_I = multiply_dim(
                  pending_I, node.Get_dim1()
              );
          }
          else
          {
              // 合并原有 I 与变量右移需要的 I
              stp_data dim = multiply_dim(var_dim, pending_I);

              if (dim != 1)
              {
                  new_chain.emplace_back(
                      NodeType_Gate,
                      GateType_I,
                      dim,
                      0
                  );
              }

              new_chain.push_back(node);
              pending_I = 1;
          }
      }

      if (pending_I != 1)
          throw std::runtime_error("Trailing I marker");

      pi_num = static_cast<stp_data>(pi_chain.size());

      new_chain.insert(
          new_chain.end(),
          pi_chain.begin(),
          pi_chain.end()
      );

      return new_chain;
  }

#ifdef ENABLE_CUDA
  void from_expr_to_matrix_cuda(void)
  {
    int32_t init_flag = 0;
    int32_t I_flag = 0;
    CUDA_DATA cuda_result;

    for (size_t i = 0; i < expr_chain.size(); i++)
    {
      if (expr_chain[i].Get_NodeType() == NodeType_Variable)
      {
        result_vec = Memcpy_To_Host(cuda_result);
        return;
      }

      switch (expr_chain[i].Get_GateType())
      {
        case GateType_W:
        {
          std::vector<stp_data> temp =
              generate_swap_vec(expr_chain[i].Get_dim1(), expr_chain[i].Get_dim2());
          CUDA_DATA temp_data = Memcpy_To_Device(temp);
          if (init_flag)
          {
            if (I_flag)
            {
              I_flag = 0;
              CUDA_DATA temp_data1 = my_cuda_In_KR_Matrix(expr_chain[i - 1].Get_dim1(), temp_data);
              cuda_result = my_cuda_semi_tensor_product(cuda_result, temp_data1);
            }
            else
            {

              cuda_result = my_cuda_semi_tensor_product(cuda_result, temp_data);
            }
          }
          else
          {
            if (I_flag)
            {
              I_flag = 0;
              CUDA_DATA temp_data1 = my_cuda_In_KR_Matrix(expr_chain[i - 1].Get_dim1(), temp_data);
              cuda_result = my_cuda_semi_tensor_product(cuda_result, temp_data1);
            }
            else
            {
              cuda_result = temp_data;
            }
            init_flag = 1;
          }
          break;
        }
        case GateType_I:
        {
          I_flag = 1;
          break;
        }
        case GateType_Mr:
        {
          std::vector<stp_data> temp = generate_Mr_vec(expr_chain[i].Get_dim1());
          CUDA_DATA temp_data = Memcpy_To_Device(temp);

          if (init_flag)
          {
            if (I_flag)
            {
              I_flag = 0;
              CUDA_DATA temp_data1 = my_cuda_In_KR_Matrix(expr_chain[i - 1].Get_dim1(), temp_data);
              cuda_result = my_cuda_semi_tensor_product(cuda_result, temp_data1);
            }
            else
            {
              cuda_result = my_cuda_semi_tensor_product(cuda_result, temp_data);
            }
          }
          else
          {
            if (I_flag)
            {
              I_flag = 0;
              CUDA_DATA temp_data1 = my_cuda_In_KR_Matrix(expr_chain[i - 1].Get_dim1(), temp_data);
              cuda_result = my_cuda_semi_tensor_product(cuda_result, temp_data1);
            }
            else
            {
              cuda_result = temp_data;
            }
            init_flag = 1;
          }
          break;
        }
        case GateType_Lut:
        {
          std::vector<stp_data> temp = expr_chain[i].Get_Lut_vec();
          CUDA_DATA temp_data = Memcpy_To_Device(temp);

          if (init_flag)
          {
            if (I_flag)
            {
              I_flag = 0;
              CUDA_DATA temp_data1 = my_cuda_In_KR_Matrix(expr_chain[i - 1].Get_dim1(), temp_data);
              cuda_result = my_cuda_semi_tensor_product(cuda_result, temp_data1);
            }
            else
            {
              cuda_result = my_cuda_semi_tensor_product(cuda_result, temp_data);
            }
          }
          else
          {
            if (I_flag)
            {
              I_flag = 0;
              CUDA_DATA temp_data1 = my_cuda_In_KR_Matrix(expr_chain[i - 1].Get_dim1(), temp_data);
              cuda_result = my_cuda_semi_tensor_product(cuda_result, temp_data1);
            }
            else
            {
              cuda_result = temp_data;
            }
            init_flag = 1;
          }
          break;
        }
        default:
        {
          std::cout << "error: gate type not found" << std::endl;
          break;
        }
      }
    }
  }
#endif

  void from_expr_to_matrix(void)
  {
    int32_t init_flag = 0;
    int32_t I_flag = 0;

    for (size_t i = 0; i < expr_chain.size(); i++)
    {
      if (expr_chain[i].Get_NodeType() == NodeType_Variable)
      {
        return;
      }

      switch (expr_chain[i].Get_GateType())
      {
        case GateType_W:
        {
          if (init_flag)
          {
            if (I_flag)
            {
              I_flag = 0;
              std::vector<stp_data> temp =
                  generate_swap_vec(expr_chain[i].Get_dim1(), expr_chain[i].Get_dim2());
              result_vec = Vec_semi_tensor_product(result_vec,
                                                   In_KR_Vec(expr_chain[i - 1].Get_dim1(), temp));
            }
            else
            {

              result_vec =
                  Vec_semi_tensor_product(result_vec, generate_swap_vec(expr_chain[i].Get_dim1(),
                                                                        expr_chain[i].Get_dim2()));
              std::vector<stp_data> temp =
                  generate_swap_vec(expr_chain[i].Get_dim1(), expr_chain[i].Get_dim2());
            }
          }
          else
          {
            if (I_flag)
            {
              I_flag = 0;
              std::vector<stp_data> temp =
                  generate_swap_vec(expr_chain[i].Get_dim1(), expr_chain[i].Get_dim2());
              result_vec = Vec_semi_tensor_product(result_vec,
                                                   In_KR_Vec(expr_chain[i - 1].Get_dim1(), temp));
            }
            else
            {
              result_vec = generate_swap_vec(expr_chain[i].Get_dim1(), expr_chain[i].Get_dim2());
            }
            init_flag = 1;
          }
          break;
        }
        case GateType_I:
        {
          I_flag = 1;
          break;
        }
        case GateType_Mr:
        {
          if (init_flag)
          {
            if (I_flag)
            {
              I_flag = 0;
              std::vector<stp_data> temp = generate_Mr_vec(expr_chain[i].Get_dim1());
              result_vec = Vec_semi_tensor_product(result_vec,
                                                   In_KR_Vec(expr_chain[i - 1].Get_dim1(), temp));
            }
            else
            {
              result_vec =
                  Vec_semi_tensor_product(result_vec, generate_Mr_vec(expr_chain[i].Get_dim1()));
            }
          }
          else
          {
            if (I_flag)
            {
              I_flag = 0;
              std::vector<stp_data> temp = generate_Mr_vec(expr_chain[i].Get_dim1());
              result_vec = Vec_semi_tensor_product(result_vec,
                                                   In_KR_Vec(expr_chain[i - 1].Get_dim1(), temp));
            }
            else
            {
              result_vec = generate_Mr_vec(expr_chain[i].Get_dim1());
            }
            init_flag = 1;
          }
          break;
        }
        case GateType_Lut:
        {
          std::vector<stp_data> temp = expr_chain[i].Get_Lut_vec();
          if (init_flag)
          {
            if (I_flag)
            {
              I_flag = 0;
              result_vec = Vec_semi_tensor_product(result_vec,
                                                   In_KR_Vec(expr_chain[i - 1].Get_dim1(), temp));
            }
            else
            {
              result_vec = Vec_semi_tensor_product(result_vec, temp);
            }
          }
          else
          {
            if (I_flag)
            {
              I_flag = 0;
              result_vec = Vec_semi_tensor_product(result_vec,
                                                   In_KR_Vec(expr_chain[i - 1].Get_dim1(), temp));
            }
            else
            {
              result_vec = temp;
            }
            init_flag = 1;
          }
          break;
        }
        default:
        {
          std::cout << "error: gate type not found" << std::endl;
          break;
        }
      }
    }
  }

  void exchange_vars_by_W()
{
    const size_t n = new_pi_list.size();

    // 当前结构矩阵对应的变量顺序
    std::vector<id> order(n);
    std::vector<stp_data> dims(n);

    for (size_t i = 0; i < n; ++i)
    {
        order[i] = new_pi_list[i].Get_Var_id();
        dims[i]  = new_pi_list[i].Get_Var_k();
    }

    // 计算一段变量维度的乘积
    auto get_dim = [&](size_t begin, size_t end) -> stp_data
    {
        stp_data dim = 1;

        for (size_t i = begin; i < end; ++i)
        {
            if (dims[i] == 0 ||
                dim > std::numeric_limits<stp_data>::max() / dims[i])
                throw std::overflow_error("STP dimension overflow");

            dim *= dims[i];
        }

        return dim;
    };

    // 按变量 ID 从 0 到 n-1 恢复原始变量顺序
    for (size_t target = 0; target < n; ++target)
    {
        size_t pos = target;

        // 找到目标变量的位置
        while (pos < n && order[pos] != static_cast<id>(target))
            ++pos;

        if (pos == n)
            throw std::runtime_error("STP variable not found");

        // 通过相邻 W 矩阵逐步交换
        while (pos > target)
        {
            const size_t j = pos - 1;

            // 当前相邻变量维度为 dims[j], dims[pos]
            // 右乘 W 时，参数使用交换后的输入维度顺序
            auto W = generate_swap_vec(
                dims[pos],
                dims[j]
            );

            // I_left ⊗ W ⊗ I_right
            const stp_data left_dim  = get_dim(0, j);
            const stp_data right_dim = get_dim(pos + 1, n);

            if (right_dim > 1)
                W = Vec_KR_In(right_dim, W);

            if (left_dim > 1)
                W = In_KR_Vec(left_dim, W);

            // 真正执行 STP 乘法，不使用真值表索引重排
            result_vec = Vec_semi_tensor_product(
                result_vec, W
            );

            // 更新变量顺序
            std::swap(order[j], order[pos]);
            std::swap(dims[j], dims[pos]);

            --pos;
        }
    }

    // 与 exchange_vars_mixed() 保持相同的输出格式
    // 存储结构矩阵的零基行索引，而不是逻辑值
    out_vec = result_vec;
}

  // exchange variables
  void exchange_vars_mixed(void)
  {
    int64_t new_num = 0;
    int32_t remain = 0;

    std::vector<stp_data> vec = Vec_to_tt(result_vec);

    out_vec.resize(vec.size() + 1);
    out_vec[0] = k;
    for (size_t i = 0; i < out_vec.size() - 1; i++)
    {
      new_num = 0;
      remain = i;

      for (size_t j = 0; j < old_pi_list.size(); j++)
      {
        int64_t old_weight = pow(old_pi_list[j].Get_Var_k(), old_pi_list.size() - 1 - j);
        int64_t old_var = remain / old_weight;
        int64_t new_weight = pow(old_pi_list[j].Get_Var_k(),
                                 old_pi_list.size() - 1 - find_id(old_pi_list[j].Get_Var_id()));

        new_num += old_var * new_weight;
        remain -= old_weight * old_var;
      }

      out_vec[i + 1] = k - 1 - vec[new_num];
    }
  }

  std::vector<stp_data> Vec_to_tt(std::vector<stp_data> &A)
  {
    stp_data size = A.size() - 1;
    std::vector<stp_data> Result(size);
    // operate on each column
    for (stp_data i = 1; i < A.size(); i++)
    {
      Result[i - 1] = k - A[i] - 1;
    }
    return Result;
  }

  // move variables to right side
  std::vector<expr_node> move_vars_to_rightside(const std::vector<expr_node> &expr_chain,
                                                stp_data &pi_num)
  {
    std::vector<expr_node> new_chain;
    std::vector<expr_node> pi_chain;
    stp_data I_dim = 1;

    // move variables to right side
    for (size_t i = 0; i < expr_chain.size(); i++)
    {
      // gate
      if (expr_chain[i].Get_NodeType() == NodeType_Gate)
      {
        if (I_dim != 1)
        {
          expr_node new_node(NodeType_Gate, GateType_I, I_dim, 0);
          new_chain.push_back(new_node);
        }
        new_chain.push_back(expr_chain[i]);
      }
      // variable
      else
      {
        pi_chain.push_back(expr_chain[i]);
        I_dim *= expr_chain[i].Get_Var_k();
      }
    }
    pi_num = pi_chain.size();

    new_chain.insert(new_chain.end(), pi_chain.begin(), pi_chain.end());

    return new_chain;
  }

  // sort variables
  std::vector<expr_node> sort_variables(const std::vector<expr_node> &expr_chain,
                                        const stp_data &pi_num)
  {
    std::vector<expr_node> pi_chain;
    std::vector<expr_node> new_chain;
    stp_data W_dim = 1;
    stp_data I_dim = 1;

    // variables
    new_chain.insert(new_chain.end(), expr_chain.begin(), expr_chain.end() - pi_num);

    // not variables
    pi_chain.insert(pi_chain.end(), expr_chain.end() - pi_num, expr_chain.end());

    for (int64_t i = pi_chain.size() - 1; i > 0; i--)
    {
      for (int64_t j = i - 1; j >= 0; j--)
      {
        // equal id
        if (pi_chain[i].Get_Var_id() == pi_chain[j].Get_Var_id())
        {
          if (i != j + 1)
          {
            pi_chain.insert(pi_chain.begin() + i, pi_chain[i]);
            pi_chain.erase(pi_chain.begin() + j);

            I_dim = get_I_dim_before_var(pi_chain, 0, j);
            if (I_dim != 1)
            {
              expr_node new_node(NodeType_Gate, GateType_I, I_dim, 0);
              new_chain.push_back(new_node);
            }

            expr_node new_node1(NodeType_Gate, GateType_W, W_dim, pi_chain[i].Get_Var_k());
            new_chain.push_back(new_node1);
          }

          I_dim = get_I_dim_before_var(pi_chain, 0, i - 1);
          if (I_dim != 1)
          {
            expr_node new_node2(NodeType_Gate, GateType_I, I_dim, 0);
            new_chain.push_back(new_node2);
          }
          expr_node new_node3(NodeType_Gate, GateType_Mr, pi_chain[i].Get_Var_k(), 0);
          new_chain.push_back(new_node3);
          break;
        }
        else
        {
          W_dim *= pi_chain[j].Get_Var_k();
        }
      }
      W_dim = 1;
    }

    new_pi_list.resize(input_names.size());
    old_pi_list.resize(input_names.size());

    new_pi_list[0] = pi_chain[0];
    id last_id = new_pi_list[0].Get_Var_id();
    stp_data last_len = 1;
    old_pi_list[last_id] = pi_chain[0];

    for (size_t i = 0; i < pi_chain.size(); i++)
    {
      if (last_id != pi_chain[i].Get_Var_id())
      {
        new_pi_list[last_len] = pi_chain[i];
        old_pi_list[pi_chain[i].Get_Var_id()] = pi_chain[i];
        last_id = pi_chain[i].Get_Var_id();
        last_len++;
      }
    }

    new_chain.insert(new_chain.end(), new_pi_list.begin(), new_pi_list.end());

    return new_chain;
  }

  stp_data get_I_dim_before_var(const std::vector<expr_node> &pi_chain, const stp_data &start,
                                const stp_data &end)
  {
    stp_data I_dim = 1;

    for (stp_data i = start; i < end; i++)
    {
      I_dim *= pi_chain[i].Get_Var_k();
    }
    return I_dim;
  }

  stp_data find_id(id index)
  {
    for (size_t i = 0; i < new_pi_list.size(); i++)
    {
      if (new_pi_list[i].Get_Var_id() == index)
        return i;
    }
    return 0;
  }

  std::vector<stp_data> out_vec;

private:
  std::vector<expr_node> old_pi_list;
  std::vector<expr_node> new_pi_list;
  std::vector<int64_t> input_names;
  std::vector<expr_node> expr_chain;
  bool verbose;
  std::vector<stp_data> result_vec;
  int k = STP_K;
  stp_data pi_num = 0;
  bool use_cuda_ = false;
};

} // namespace stp
