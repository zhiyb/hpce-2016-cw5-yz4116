#ifndef user_random_walk_hpp
#define user_random_walk_hpp

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
};

#endif
