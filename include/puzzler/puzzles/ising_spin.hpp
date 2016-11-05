#ifndef puzzler_puzzles_ising_spin_hpp
#define puzzler_puzzles_ising_spin_hpp

#include <random>
#include <sstream>

#include "puzzler/core/puzzle.hpp"

namespace puzzler
{
  class IsingSpinPuzzle;
  class IsingSpinInput;
  class IsingSpinOutput;


  class IsingSpinInput
    : public Puzzle::Input
  {
  public:
    uint32_t n;
    uint32_t seed;
    uint32_t maxTime;
    uint32_t repeats;
  
    std::vector<uint32_t> probs;

    IsingSpinInput(const Puzzle *puzzle, int scale)
      : Puzzle::Input(puzzle, scale)
    {}

    IsingSpinInput(std::string format, std::string name, PersistContext &ctxt)
      : Puzzle::Input(format, name, ctxt)
    {
      PersistImpl(ctxt);
    }

    virtual void PersistImpl(PersistContext &conn) override final
    {
      conn.SendOrRecv(n);
      conn.SendOrRecv(seed);
      conn.SendOrRecv(maxTime);
      conn.SendOrRecv(repeats);
      conn.SendOrRecv(probs);
    }
  };

  class IsingSpinOutput
    : public Puzzle::Output
  {
  public:
    std::vector<double> means;
    std::vector<double> stddevs;

    IsingSpinOutput(const Puzzle *puzzle, const Puzzle::Input *input)
      : Puzzle::Output(puzzle, input)
    {}

    IsingSpinOutput(std::string format, std::string name, PersistContext &ctxt)
      : Puzzle::Output(format, name, ctxt)
    {
      PersistImpl(ctxt);
    }

    virtual void PersistImpl(PersistContext &conn) override
    {
      conn.SendOrRecv(means);
      conn.SendOrRecv(stddevs);
    }

    virtual bool Equals(const Output *output) const override
    {
      auto pOutput=As<IsingSpinOutput>(output);
      double eps = 1e-9;
      
      for(unsigned i=0;i<means.size();i++){
        if( std::abs( means[i] - pOutput->means.at(i)) > eps)
          return false;
        if( std::abs( stddevs[i] - pOutput->stddevs.at(i)) > eps)
          return false;
      }
      return true;
    }

  };


  class IsingSpinPuzzle
    : public PuzzleBase<IsingSpinInput,IsingSpinOutput>
  {
  private:

    // This has some interesting and maybe useful properties...
    // The recurrence equation is:
    //   x_{i+1} = x_i * 1664525 + 1013904223 mod 2^32
    uint32_t lcg(uint32_t x) const
    {
      return x*1664525+1013904223;
    };
    
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


  protected:
    /* Start from node start, then follow a random walk of length nodes, incrementing
       the count of all the nodes we visit. */
    void random_walk(std::vector<dd_node_t> &nodes, uint32_t seed, unsigned start, unsigned length) const
    {
      uint32_t rng=seed;
      unsigned current=start;
      for(unsigned i=0; i<length; i++){
        nodes[current].count++;

        unsigned edgeIndex = rng % nodes[current].edges.size();
        rng=lcg(rng);
        
        current=nodes[current].edges[edgeIndex];
      }
    }


    virtual void Execute(
			 ILog *log,
			 const IsingSpinInput *input,
			 IsingSpinOutput *output
			 ) const =0;


    void ReferenceExecute(
			  ILog *log,
			  const IsingSpinInput *pInput,
			  IsingSpinOutput *pOutput
			  ) const
    {
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

  public:
    virtual std::string Name() const override
    { return "ising_spin"; }

    virtual std::shared_ptr<Input> CreateInput(
					       ILog *,
					       int scale
					       ) const override
    {
      std::mt19937 rnd(time(0));  // Not the best way of seeding...
      std::uniform_real_distribution<> udist;
      
      auto params=std::make_shared<IsingSpinInput>(this, scale);

      params->n=scale;
      params->seed=rnd();
      params->maxTime=scale;
      params->repeats=3+sqrt(scale);
      
      params->probs.resize(10);
      
      double T=0.6+0.4*udist(rnd);
      double J=1;
      double H=0.05*udist(rnd);
      
      // Efficient Parallel Simulations of Asynchronous Cellular Arrays", 1987, Boris D. Lubachevsky
      for(unsigned i=0; i<5; i++){
        for(unsigned j=0; j<2; j++){
          int index = i + 5*j; // index == 0,1,... ,9
          int my_spin = 2*j - 1;
          int sum_nei = 2*i - 4;
          double d_E = 2.0*(J * my_spin * sum_nei + H * my_spin);
          double x = exp(-d_E/T);
          double p = x/(1.0+x);
          params->probs[index]= ldexp(p, 32); // scale value in [0,1) to value in 0..2^32-1
        }
      }
      return params;
    }

  };

};

#endif
