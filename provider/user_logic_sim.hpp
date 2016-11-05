#ifndef user_logic_sim_hpp
#define user_logic_sim_hpp

#include "puzzler/puzzles/logic_sim.hpp"

class LogicSimProvider
  : public puzzler::LogicSimPuzzle
{
public:
  LogicSimProvider()
  {}

  virtual void Execute(
		       puzzler::ILog *log,
		       const puzzler::LogicSimInput *input,
		       puzzler::LogicSimOutput *output
		       ) const override {
    return ReferenceExecute(log, input, output);
  }

};

#endif
