#ifndef user_logic_sim_hpp
#define user_logic_sim_hpp

#include <tbb/parallel_for.h>
#include "puzzler/puzzles/logic_sim.hpp"

class LogicSimProvider
: public puzzler::LogicSimPuzzle
{
public:
	LogicSimProvider()
	{}

	virtual void Execute(
			puzzler::ILog *log,
			const puzzler::LogicSimInput *pInput,
			puzzler::LogicSimOutput *pOutput
			) const override {
		log->LogVerbose("About to start running clock cycles (total = %d", pInput->clockCycles);
		std::vector<bool> state=pInput->inputState;
		for(unsigned i=0; i<pInput->clockCycles; i++){
			log->LogVerbose("Starting iteration %d of %d\n", i, pInput->clockCycles);

			state=next(state, pInput);

			// The weird form of log is so that there is little overhead
			// if logging is disabled
			log->Log(puzzler::Log_Debug,[&](std::ostream &dst) {
					for(unsigned i=0; i<state.size(); i++){
					dst<<state[i];
					}
					});
		}

		log->LogVerbose("Finished clock cycles");

		pOutput->outputState=state;
	}

protected:

	bool calcSrc(unsigned src, const std::vector<bool> &state, const puzzler::LogicSimInput *input) const
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

	std::vector<bool> next(const std::vector<bool> &state, const puzzler::LogicSimInput *input) const
	{
		std::vector<bool> res(state.size());
		std::vector<unsigned> res_c(state.size());
		unsigned k = state.size();
		tbb::parallel_for(0u, k, [&](unsigned i){
		// for(unsigned i=0; i<res.size(); i++){
			res_c[i]=(unsigned)calcSrc(input->flipFlopInputs[i], state, input);
		});
		for(unsigned i = 0; i < k; i++)
		{
			res[i] = (bool)res_c[i];
		}
		return res;
	}
};

#endif
