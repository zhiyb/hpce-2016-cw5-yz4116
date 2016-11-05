#ifndef local_puzzles_hpp
#define local_puzzles_hpp

#include "user_random_walk.hpp"
#include "user_ising_spin.hpp"
#include "user_julia.hpp"
#include "user_logic_sim.hpp"

void puzzler::PuzzleRegistrar::UserRegisterPuzzles()
{
  Register(std::make_shared<RandomWalkProvider>());
  Register(std::make_shared<IsingSpinProvider>());
  Register(std::make_shared<JuliaProvider>());
  Register(std::make_shared<LogicSimProvider>());
}


#endif
