#ifndef user_ising_spin_hpp
#define user_ising_spin_hpp

#include "puzzler/puzzles/ising_spin.hpp"

class IsingSpinProvider
  : public puzzler::IsingSpinPuzzle
{
public:
  IsingSpinProvider()
  {}

  virtual void Execute(
		       puzzler::ILog *log,
		       const puzzler::IsingSpinInput *input,
		       puzzler::IsingSpinOutput *output
		       ) const override {
    return ReferenceExecute(log, input, output);
  }

};

#endif
