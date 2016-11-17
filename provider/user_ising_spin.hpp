#ifndef user_ising_spin_hpp
#define user_ising_spin_hpp

#include "puzzler/puzzles/ising_spin.hpp"

class IsingSpinProvider : public puzzler::IsingSpinPuzzle
{
public:
	IsingSpinProvider() {}

	virtual void Execute(
		puzzler::ILog *log,
		const puzzler::IsingSpinInput *pInput,
		puzzler::IsingSpinOutput *pOutput
	) const override {
		int n=pInput->n;

		std::vector<int> current(n*n), next(n*n);

		std::vector<double> sums(pInput->maxTime, 0.0);
		std::vector<double> sumSquares(pInput->maxTime, 0.0);

		log->LogInfo("Starting steps.");

		std::mt19937 rng(pInput->seed); // Gives the same sequence on all platforms
		uint32_t seed;
		for(unsigned i=0; i<pInput->repeats; i++){
			seed=rng();

			log->LogVerbose("  Repeat %u", i);

			init(pInput, seed, &current[0]);

			for(unsigned t=0; t<pInput->maxTime; t++){
				log->LogDebug("    Step %u", t);

				// Dump the state of spins on high log levels
				dump(Log_Debug, pInput, &current[0], log);

				step(pInput, seed, &current[0], &next[0]);
				std::swap(current, next);

				// Track the statistics
				double countPositive=count(pInput, &current[0]);
				sums[t] += countPositive;
				sumSquares[t] += countPositive*countPositive;
			}

			seed=lcg(seed);
		}

		log->LogInfo("Calculating final statistics");

		pOutput->means.resize(pInput->maxTime);
		pOutput->stddevs.resize(pInput->maxTime);
		for(unsigned i=0; i<pInput->maxTime; i++){
			pOutput->means[i] = sums[i] / pInput->maxTime;
			pOutput->stddevs[i] = sqrt( sumSquares[i]/pInput->maxTime - pOutput->means[i]*pOutput->means[i] );
			log->LogVerbose("  time %u : mean=%8.6f, stddev=%8.4f", i, pOutput->means[i], pOutput->stddevs[i]);
		}

		log->LogInfo("Finished");
	}

private:
	// This has some interesting and maybe useful properties...
	// The recurrence equation is:
	//   x_{i+1} = x_i * 1664525 + 1013904223 mod 2^32
	uint32_t lcg(uint32_t x) const
	{
		return x*1664525+1013904223;
	}

	void init(
		const IsingSpinInput *pInput,
		uint32_t &seed,
		int *out
	 ) const {
		unsigned n=pInput->n;

		for(unsigned x=0; x<n; x++){
			for(unsigned y=0; y<n; y++){
				out[y*n+x] = (seed < 0x80001000ul) ? +1 : -1;
				seed = lcg(seed);
			}
		}
	}

	void dump(
		int logLevel,
		const IsingSpinInput *pInput,
		int *in,
		ILog *log
	 ) const {
		if(logLevel > log->Level())
			return;

		unsigned n=pInput->n;

		log->Log(logLevel, [&](std::ostream &dst){
				dst<<"\n";
				for(unsigned y=0; y<n; y++){
				for(unsigned x=0; x<n; x++){
				dst<<(in[y*n+x]<0?"-":"+");
				} 
				dst<<"\n";
				}
				});
	}

	int count(
		const IsingSpinInput *pInput,
		const int *in
	 ) const {
		unsigned n=pInput->n;

		return std::accumulate(in, in+n*n, 0);
	}

	void step(
		const IsingSpinInput *pInput,
		uint32_t &seed,
		const int *in,
		int *out
	 ) const {
		unsigned n=pInput->n;

		for(unsigned x=0; x<n; x++){
			for(unsigned y=0; y<n; y++){
				int W = x==0 ?    in[y*n+n-1]   : in[y*n+x-1];
				int E = x==n-1 ?  in[y*n+0]     : in[y*n+x+1];
				int N = y==0 ?    in[(n-1)*n+x] : in[(y-1)*n+x];
				int S = y==n-1 ?  in[0*n+x]     : in[(y+1)*n+x];
				int nhood=W+E+N+S;

				int C = in[y*n+x];

				unsigned index=(nhood+4)/2 + 5*(C+1)/2;
				float prob=pInput->probs[index];

				if( seed < prob){
					C *= -1; // Flip
				}

				out[y*n+x]=C;

				seed = lcg(seed);
			}
		}
	}
};

#endif
