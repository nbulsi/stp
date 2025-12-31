#include <stp/core/circuit_graph.hpp>
#include <stp/utils/stp_utils.hpp>
#include <iostream>
#include <cmath>

#ifndef LUT_PARSER_H
#define LUT_PARSER_H

class LutParser
{
public:
	bool parse(std::istream& is, CircuitGraph& graph)
	{
    std::string line;
    const std::string isInput = "INPUT";
    const std::string isOutput = "OUTPUT";
    const std::string isLut = "LUT";
    const std::string isGnd = "gnd";

		while(std::getline(is, line))
    {
      if ( line.find( isLut ) != std::string::npos )
      {
        match_gate( graph, line );
        continue;
      }
      if ( line.find( isInput ) != std::string::npos )
      {
        match_input( graph, line );
        continue;
      }
      if ( line.find( isOutput ) != std::string::npos )
      {
        match_output( graph, line );
        continue;
      }
      if(line.empty()) continue;
    }
    return true;
	}

private:
	void match_input(CircuitGraph& graph, const std::string& line)
	{
		std::string input_name = get_io_name(line);
		graph.add_input(input_name);
	}

	void match_output(CircuitGraph& graph, const std::string& line)
	{
		std::string output_name = get_io_name(line);
		graph.add_output(output_name);
	}

	void match_gate(CircuitGraph& graph, const std::string& line)
	{ 
		std::vector<std::string> gate = m_split(line, ",=( )");
		std::string output = gate[0];
		std::string tt = gate[2];
		gate.erase(gate.begin(), gate.begin() + 3);
		std::vector<std::string> inputs(gate);
		tt.erase(0, 2); //delete 0x
		Type type = get_stp_vec(tt, inputs.size());
    graph.add_gate(type, inputs, output);
	}
	std::string get_io_name(const std::string& str) 
	{
		size_t start = str.find('(');
		size_t end = str.rfind(')');  // from right to left
		//need to delete space
		if (start != std::string::npos && end != std::string::npos && start < end) {
			return str.substr(start + 1, end - start - 1);
		}
		return ""; // error situation
	}
  
  stp_vec get_stp_vec(const std::string& tt, const int& inputs_num)
  {
    //buff or not
    if(inputs_num == 1 && tt.size() == 1)
    {
      stp_vec type(3);
      type(0) = 2;
      switch (tt[0])
      {
      case '0':
        type(1) = 1; type(2) = 1;
        break;
      case '1':
        type(1) = 1; type(2) = 0;
        break;
      case '2':
        type(1) = 0; type(2) = 1;
        break;
      case '3':
        type(1) = 0; type(2) = 0;
        break;
      default:
        break;
      }
      return type;
    }
    stp_vec type((1 << inputs_num) + 1);
    type(0) = 2;
    int type_idx;
    for(int i = 0, len = tt.size(); i < len; i++)
    {
      type_idx = 4 * i + 1;
      switch (tt[i])
      {
      case '0': //0000 - > 1111
        type(type_idx) = 1; type(type_idx + 1) = 1; type(type_idx + 2) = 1; type(type_idx + 3) = 1;
        break;
      case '1': //0001 - > 1110
        type(type_idx) = 1; type(type_idx + 1) = 1; type(type_idx + 2) = 1; type(type_idx + 3) = 0;
        break;
      case '2': //0010 - > 1101
        type(type_idx) = 1; type(type_idx + 1) = 1; type(type_idx + 2) = 0; type(type_idx + 3) = 1;
        break;
      case '3': //0011 - > 1100
        type(type_idx) = 1; type(type_idx + 1) = 1; type(type_idx + 2) = 0; type(type_idx + 3) = 0;
        break;
      case '4': //0100 - > 1011
        type(type_idx) = 1; type(type_idx + 1) = 0; type(type_idx + 2) = 1; type(type_idx + 3) = 1;
        break;
      case '5': //0101 - > 1010
        type(type_idx) = 1; type(type_idx + 1) = 0; type(type_idx + 2) = 1; type(type_idx + 3) = 0;
        break;
      case '6': //0110 - > 1001
        type(type_idx) = 1; type(type_idx + 1) = 0; type(type_idx + 2) = 0; type(type_idx + 3) = 1;
        break;
      case '7': //0111 - > 1000
        type(type_idx) = 1; type(type_idx + 1) = 0; type(type_idx + 2) = 0; type(type_idx + 3) = 0;
        break;
      case '8': //1000 - > 0111
        type(type_idx) = 0; type(type_idx + 1) = 1; type(type_idx + 2) = 1; type(type_idx + 3) = 1;
        break;
      case '9': //1001 - > 0110
        type(type_idx) = 0; type(type_idx + 1) = 1; type(type_idx + 2) = 1; type(type_idx + 3) = 0;
        break;
      case 'a': //1010 - > 0101
        type(type_idx) = 0; type(type_idx + 1) = 1; type(type_idx + 2) = 0; type(type_idx + 3) = 1;
        break;
      case 'b': //1011 - > 0100
        type(type_idx) = 0; type(type_idx + 1) = 1; type(type_idx + 2) = 0; type(type_idx + 3) = 0;
        break;
      case 'c': //1100 - > 0011
        type(type_idx) = 0; type(type_idx + 1) = 0; type(type_idx + 2) = 1; type(type_idx + 3) = 1;
        break;
      case 'd': //1101 - > 0010
        type(type_idx) = 0; type(type_idx + 1) = 0; type(type_idx + 2) = 1; type(type_idx + 3) = 0;
        break; 
      case 'e': //1110 - > 0001
        type(type_idx) = 0; type(type_idx + 1) = 0; type(type_idx + 2) = 0; type(type_idx + 3) = 1;
        break;
      case 'f': //1111 - > 0000
        type(type_idx) = 0; type(type_idx + 1) = 0; type(type_idx + 2) = 0; type(type_idx + 3) = 0;
        break;
      default:
        break;
      }
    }
    return type;
  }
};

#endif