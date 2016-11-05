#ifndef puzzler_puzzles_logic_sim_hpp
#define puzzler_puzzles_logic_sim_hpp

#include <random>
#include <sstream>

#include "puzzler/core/puzzle.hpp"

namespace puzzler
{
  class LogicSimPuzzle;
  class LogicSimInput;
  class LogicSimOutput;
    
  class LogicSimInput
    : public Puzzle::Input
  {
  public:
    // A list of pairs (src1,src2). If src<0 it refers to a flip-flop. If src>0 it refers to a xor output
    std::vector<std::pair<int32_t,int32_t> > xorGateInputs;

    // A list of srcs. If src<0 it refers to a flip-flop. If src>0 it refers to a xor output
    std::vector<int32_t> flipFlopInputs;


    uint32_t clockCycles;
    std::vector<bool> inputState;


    LogicSimInput(const Puzzle *puzzle, int scale)
      : Puzzle::Input(puzzle, scale)
    {}

    LogicSimInput(std::string format, std::string name, PersistContext &ctxt)
      : Puzzle::Input(format, name, ctxt)
    {
      PersistImpl(ctxt);
    }

    virtual void PersistImpl(PersistContext &conn) override final
    {
      conn.SendOrRecv(xorGateInputs);
      conn.SendOrRecv(flipFlopInputs);
      conn.SendOrRecv(inputState);
      conn.SendOrRecv(clockCycles);

      if(inputState.size()!=flipFlopInputs.size())
        throw std::runtime_error("LogicSimInput::Persist - state size is inconsistent.");
    }



  };

  class LogicSimOutput
    : public Puzzle::Output
  {
  public:
    std::vector<bool> outputState;

    LogicSimOutput(const Puzzle *puzzle, const Puzzle::Input *input)
      : Puzzle::Output(puzzle, input)
    {}

    LogicSimOutput(std::string format, std::string name, PersistContext &ctxt)
      : Puzzle::Output(format, name, ctxt)
    {
      PersistImpl(ctxt);
    }

    virtual void PersistImpl(PersistContext &conn) override
    {
      conn.SendOrRecv(outputState);
    }

    virtual bool Equals(const Output *output) const override
    {
      auto pOutput=As<LogicSimOutput>(output);
      return outputState==pOutput->outputState;
    }

  };


  class LogicSimPuzzle
    : public PuzzleBase<LogicSimInput,LogicSimOutput>
  {
  protected:

    bool calcSrc(unsigned src, const std::vector<bool> &state, const LogicSimInput *input) const
    {
      if(src < state.size()){
        return state.at(src);
      }else{
        unsigned xorSrc=src - state.size();
        bool a=calcSrc(input->xorGateInputs.at(xorSrc).first, state, input);
        bool b=calcSrc(input->xorGateInputs.at(xorSrc).second, state, input);
        return a != b;
      }
    }

    std::vector<bool> next(const std::vector<bool> &state, const LogicSimInput *input) const
    {
      std::vector<bool> res(state.size());
      for(unsigned i=0; i<res.size(); i++){
        res[i]=calcSrc(input->flipFlopInputs[i], state, input);
      }
      return res;
    }

  protected:

    virtual void Execute(
			 ILog *log,
			 const LogicSimInput *input,
			 LogicSimOutput *output
			 ) const =0;

    void ReferenceExecute(
			  ILog *log,
			  const LogicSimInput *pInput,
			  LogicSimOutput *pOutput
			  ) const
    {
      log->LogVerbose("About to start running clock cycles (total = %d", pInput->clockCycles);
      std::vector<bool> state=pInput->inputState;
      for(unsigned i=0; i<pInput->clockCycles; i++){
	log->LogVerbose("Starting iteration %d of %d\n", i, pInput->clockCycles);

	state=next(state, pInput);

	// The weird form of log is so that there is little overhead
	// if logging is disabled
	log->Log(Log_Debug,[&](std::ostream &dst) {
	    for(unsigned i=0; i<state.size(); i++){
	      dst<<state[i];
	    }
	  });
      }

      log->LogVerbose("Finished clock cycles");

      pOutput->outputState=state;
    }

  public:
    virtual std::string Name() const override
    { return "logic_sim"; }

    virtual std::shared_ptr<Input> CreateInput(
					       ILog *,
					       int scale
					       ) const override
    {
      std::mt19937 rnd(time(0));  // Not the best way of seeding...

      auto params=std::make_shared<LogicSimInput>(this, scale);

      params->clockCycles=scale;
      
      unsigned flipFlopCount=scale;
      unsigned xorGateCount=8*scale;

      params->xorGateInputs.resize(xorGateCount);
      params->flipFlopInputs.resize(flipFlopCount);

      std::vector<unsigned> todo;
      std::vector<unsigned> done;

      for(unsigned i=0; i<flipFlopCount; i++){
        done.push_back(i);
      }
      for(unsigned i=0; i<xorGateCount; i++){
        todo.push_back(i+flipFlopCount);
      }

      while(todo.size()>0){
        unsigned idx=rnd()%todo.size();
        unsigned curr=todo[idx];
        todo.erase(todo.begin()+idx);

        unsigned currXor=curr - flipFlopCount;

        unsigned src1=done[rnd()%done.size()];
        unsigned src2=done[rnd()%done.size()];

        params->xorGateInputs[currXor].first=src1;
        params->xorGateInputs[currXor].second=src2;

        done.push_back(curr);
      }

      for(unsigned i=0; i<flipFlopCount; i++){
        params->flipFlopInputs[i]=done[rnd()%done.size()];
      }

      params->inputState.resize(flipFlopCount);
      for(unsigned i=0; i<flipFlopCount; i++){
        params->inputState[i] = 1 == (rnd()&1);
      }

      return params;
    }

  };

};

#endif
