#ifndef user_random_walk_hpp
#define user_random_walk_hpp

#include "puzzler/puzzles/random_walk.hpp"

class RandomWalkProvider
  : public puzzler::RandomWalkPuzzle
{
public:
  RandomWalkProvider()
  {}

  virtual void Execute(
		       puzzler::ILog *log,
		       const puzzler::RandomWalkInput *input,
		       puzzler::RandomWalkOutput *output
		       ) const override {
    return ReferenceExecute(log, input, output);
  }

};

#endif
