#include <iostream>
#include <vector>
#include <list>
#include <stp/utils/stp_utils.hpp>

#ifndef STP_VECTOR
#define STP_VECTOR

class stp_vec;
using stp_data = uint32_t;
using word = stp_data;
using m_chain = std::vector<stp_vec>;

class stp_vec{

  friend std::ostream &operator<<(std::ostream &os, const stp_vec &v)
  {
    const unsigned length = v.cols();
    for(unsigned i = 0; i < length; ++i)
      os << v.vec[i] << " ";
    os << std::endl;
    return os;
  }

  friend std::istream &operator>>(std::istream &is, stp_vec &v)
  {
    const unsigned length = v.cols();
    for(unsigned i = 0; i < length; ++i)
      is >> v.vec[i];
    return is;
  }

public:
  stp_vec()                                                { this->vec.clear(); }
  stp_vec(unsigned cols, unsigned value = 0)               { this->vec.resize(cols, value); }
  stp_vec(const stp_vec& v)                                { this->vec = v.vec; }
  stp_vec &operator=(const stp_vec &v)                     { this->vec = v.vec; return *this;}

  bool operator==(const stp_vec &v)
  {
    unsigned m_length = this->cols();
    unsigned length = v.cols();
    if(m_length != length)
      return false;
    for(unsigned i = 0; i < length; ++i)
    {
      if(this->vec[i] != v.vec[i])  { return false; }
    }
    return true;
  }

  word& operator() (unsigned idx)
  {
    return vec[idx];
  }

  const word& operator() (unsigned idx) const
  {
    return vec[idx];
  }
  

  const unsigned cols() const { return this->vec.size(); }


  bool is_variable() 
  {
    return this->cols() == 1;
  }

  bool block(const stp_vec &v, int m_begin, int begin, int num)
  {
    if(this->cols() < num || v.cols() < num)
    {
      std::cout << "block abnormal" << std::endl;
      return false;
    }
    for(unsigned i = 0; i < num; ++i)
    {
      this->vec[m_begin + i] = v.vec[begin + i];
    }
    return true;
  }

  std::vector<word> vec; 
};





#endif