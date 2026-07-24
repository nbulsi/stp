// Copyright (c) 2023-2026 The STP Authors
// SPDX-License-Identifier: MIT

#pragma once

#include <cmath>
#include <iostream>
#include <stp/sim/execute.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#define STP_K 2

bool _using_CUDA = false;

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
                    const std::vector<int64_t> &old_pi_index)
      : expr_chain(expr_chain), input_names(old_pi_index)
  {
    // print_expr_chain(expr_chain);
    normalize_expr_chain();
    // print_expr_chain(expr_chain);
    if (_using_CUDA == true)
    {
#ifdef ENABLE_CUDA
      from_expr_to_matrix_cuda();
      if ((expr_chain.size() - pi_num) > 5)
        from_expr_to_matrix_cuda();
      else
        from_expr_to_matrix();
#endif
    }
    else
    {
      from_expr_to_matrix();
    }
    exchange_vars_mixed();
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
};

} // namespace stp
