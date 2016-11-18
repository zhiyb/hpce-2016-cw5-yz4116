#ifndef user_logic_sim_hpp
#define user_logic_sim_hpp

#include <tbb/parallel_for.h>
#include "puzzler/puzzles/logic_sim.hpp"

class LogicSimProvider : public puzzler::LogicSimPuzzle
{
public:
	LogicSimProvider() {}

	virtual void Execute(
		puzzler::ILog *log,
		const puzzler::LogicSimInput *pInput,
		puzzler::LogicSimOutput *pOutput
	) const override {
		log->LogVerbose("About to start running clock cycles (total = %d", pInput->clockCycles);
		std::vector<bool> state(pInput->inputState);
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
		std::vector<unsigned> res(state.size());
		tbb::parallel_for((size_t)0, state.size(), [&](size_t i) {
		//for(unsigned i=0; i<res.size(); i++){
			res[i]=calcSrc(input->flipFlopInputs[i], state, input);
		});
		std::vector<bool> result(state.size());
		for (size_t i = 0; i != state.size(); i++)
			result[i] = res[i];
		return result;
	}
};

#endif
