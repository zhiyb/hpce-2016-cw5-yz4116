#ifndef user_random_walk_hpp
#define user_random_walk_hpp

#include <tbb/parallel_for.h>
#include "puzzler/puzzles/random_walk.hpp"

using namespace puzzler;

class RandomWalkProvider
: public puzzler::RandomWalkPuzzle
{
public:
	RandomWalkProvider()
	{}

	virtual void Execute(
			puzzler::ILog *log,
			const puzzler::RandomWalkInput *pInput,
			puzzler::RandomWalkOutput *pOutput
			) const override {

		// Take a copy, as we'll need to modify the "count" flags
		const std::vector<dd_node_t> &nodes(pInput->nodes);
		std::vector<std::vector<uint32_t> > count(pInput->numSamples, std::vector<uint32_t>(nodes.size()));

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

		std::vector<unsigned> seed(pInput->numSamples);
		std::vector<unsigned> start(pInput->numSamples);

		for(unsigned i=0; i<pInput->numSamples; i++){
			seed[i] = rng();
			start[i] = rng() % nodes.size();    // Choose a random node
		}

		tbb::parallel_for(0u, pInput->numSamples, [&](unsigned i){
			unsigned length=pInput->lengthWalks;           // All paths the same length
			random_walk(nodes, count[i], seed[i], start[i], length);
		});

		log->LogVerbose("Done random walks, converting histogram");

		// Map the counts from the nodes back into an array
		pOutput->histogram.resize(nodes.size());
		tbb::parallel_for((size_t)0, nodes.size(), [&](size_t i){
			uint32_t cnt = nodes[i].count;
			for (unsigned j = 0; j != pInput->numSamples; j++)
				cnt += count[j][i];
			pOutput->histogram[i]=std::make_pair(uint32_t(cnt),uint32_t(i));
			//nodes[i].count=0;
		});
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

protected:
	/* Start from node start, then follow a random walk of length nodes, incrementing
	   the count of all the nodes we visit. */
	void random_walk(const std::vector<dd_node_t> &nodes, std::vector<uint32_t> &count, \
			uint32_t seed, unsigned start, unsigned length) const
	{
		uint32_t rng=seed;
		unsigned current=start;
		for(unsigned i=0; i<length; i++){
			//nodes[current].count++;
			count[current]++;

			unsigned edgeIndex = rng % nodes[current].edges.size();
			rng=step(rng);

			current=nodes[current].edges[edgeIndex];
		}
	}

private:
	uint32_t step(uint32_t x) const
	{
		return x*1664525+1013904223;
	}
};

#endif
