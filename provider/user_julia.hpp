#ifndef user_julia_hpp
#define user_julia_hpp

#include "puzzler/puzzles/julia.hpp"

class JuliaProvider
  : public puzzler::JuliaPuzzle
{
public:
  JuliaProvider()
  {}

  virtual void Execute(
		       puzzler::ILog *log,
		       const puzzler::JuliaInput *input,
		       puzzler::JuliaOutput *output
		       ) const override {
    return ReferenceExecute(log, input, output);
  }

};

#endif
