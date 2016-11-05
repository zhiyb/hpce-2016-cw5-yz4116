#ifndef puzzler_puzzles_random_walk_hpp
#define puzzler_puzzles_random_walk_hpp

#include <random>
#include <sstream>
#include <algorithm>

#include "puzzler/core/puzzle.hpp"

namespace puzzler
{
  class RandomWalkPuzzle;
  class RandomWalkInput;
  class RandomWalkOutput;

  struct dd_node_t
  {
    uint32_t id;
    std::vector<uint32_t> edges;

    uint32_t count;

    void persist(PersistContext &ctxt)
    {
      ctxt.SendOrRecv(id);
      ctxt.SendOrRecv(edges);
      ctxt.SendOrRecv(count);
    }
  };

  class RandomWalkInput
    : public Puzzle::Input
  {
  public:
    uint32_t seed;
    uint32_t numSamples;
    uint32_t lengthWalks;

    std::vector<dd_node_t> nodes;

    RandomWalkInput(const Puzzle *puzzle, int scale)
      : Puzzle::Input(puzzle, scale)
    {}

    RandomWalkInput(std::string format, std::string name, PersistContext &ctxt)
      : Puzzle::Input(format, name, ctxt)
    {
      PersistImpl(ctxt);
    }

    virtual void PersistImpl(PersistContext &conn) override final
    {
      conn.SendOrRecv(seed);
      conn.SendOrRecv(numSamples);
      conn.SendOrRecv(lengthWalks);
      conn.SendOrRecv(nodes);

      for(unsigned i=0; i<nodes.size(); i++){
        if(nodes[i].id!=i)
          throw std::runtime_error("RandomWalkInput::Persist - ids are corrupt.");
        for(unsigned j=0; j<nodes[i].edges.size(); j++){
          if(nodes[i].edges[j] >= nodes.size())
            throw std::runtime_error("RandomWalkInput::Persist - edges are corrupt.");
        }
      }

    }
  };

  class RandomWalkOutput
    : public Puzzle::Output
  {
  public:
    std::vector<std::pair<uint32_t,uint32_t> > histogram;

    RandomWalkOutput(const Puzzle *puzzle, const Puzzle::Input *input)
      : Puzzle::Output(puzzle, input)
    {}

    RandomWalkOutput(std::string format, std::string name, PersistContext &ctxt)
      : Puzzle::Output(format, name, ctxt)
    {
      PersistImpl(ctxt);
    }

    virtual void PersistImpl(PersistContext &conn) override
    {
      conn.SendOrRecv(histogram);
    }

    virtual bool Equals(const Output *output) const override
    {
      auto pOutput=As<RandomWalkOutput>(output);
      return histogram==pOutput->histogram;
    }

  };


  class RandomWalkPuzzle
    : public PuzzleBase<RandomWalkInput,RandomWalkOutput>
  {
  private:

    uint32_t step(uint32_t x) const
    {
      return x*1664525+1013904223;
    };


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
        rng=step(rng);
        
        current=nodes[current].edges[edgeIndex];
      }
    }


    virtual void Execute(
			 ILog *log,
			 const RandomWalkInput *input,
			 RandomWalkOutput *output
			 ) const =0;


    void ReferenceExecute(
			  ILog *log,
			  const RandomWalkInput *pInput,
			  RandomWalkOutput *pOutput
			  ) const
    {

      // Take a copy, as we'll need to modify the "count" flags
      std::vector<dd_node_t> nodes(pInput->nodes);
      
      log->Log(Log_Debug, [&](std::ostream &dst){
        dst<<"  Scale = "<<nodes.size()<<"\n";
        for(unsigned i=0;i<nodes.size();i++){
          dst<<"  "<<i<<" -> [";
          for(unsigned j=0;j<nodes[i].edges.size();j++){
            if(j!=0)
              dst<<",";
            dst<<nodes[i].edges[j];
          }
          dst<<"]\n";
        }
      });

      log->LogVerbose("Starting random walks");

      // This gives the same sequence on all platforms
      std::mt19937 rng(pInput->seed);

      for(unsigned i=0; i<pInput->numSamples; i++){
        unsigned seed=rng();
        unsigned start=rng() % nodes.size();    // Choose a random node
        unsigned length=pInput->lengthWalks;           // All paths the same length

        random_walk(nodes, seed, start, length);
      }

      log->LogVerbose("Done random walks, converting histogram");

      // Map the counts from the nodes back into an array
      pOutput->histogram.resize(nodes.size());
      for(unsigned i=0; i<nodes.size(); i++){
        pOutput->histogram[i]=std::make_pair(uint32_t(nodes[i].count),uint32_t(i));
        nodes[i].count=0;
      }
      // Order them by how often they were visited
      std::sort(pOutput->histogram.rbegin(), pOutput->histogram.rend());
      

      // Debug only. No cost in normal execution
      log->Log(Log_Debug, [&](std::ostream &dst){
        for(unsigned i=0; i<pOutput->histogram.size(); i++){
          dst<<"  "<<i<<" : "<<pOutput->histogram[i].first<<", "<<pOutput->histogram[i].second<<"\n";
        }
      });

      log->LogVerbose("Finished");
    }

  public:
    virtual std::string Name() const override
    { return "random_walk"; }

    virtual std::shared_ptr<Input> CreateInput(
					       ILog *,
					       int scale
					       ) const override
    {
      std::mt19937 rnd(time(0));  // Not the best way of seeding...

      auto params=std::make_shared<RandomWalkInput>(this, scale);

      params->seed=rnd();
      params->numSamples=scale;
      params->lengthWalks=scale;

      params->nodes.resize(scale);
      for(unsigned i=0; i<(unsigned)scale; i++){
        unsigned degree=1 + unsigned(sqrt(scale));

        params->nodes[i].id=i;
        params->nodes[i].count=0;
        params->nodes[i].edges.reserve(degree);
        for(unsigned j=0; j<degree; j++){
          params->nodes[i].edges.push_back(rnd()%scale);
        }
      }

      return params;
    }

  };

};

#endif
