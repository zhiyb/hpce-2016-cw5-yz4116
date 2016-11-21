#ifndef user_random_walk_hpp
#define user_random_walk_hpp

#include <tbb/parallel_for.h>
#include "puzzler/puzzles/random_walk.hpp"

#include <fstream>

// Update: this doesn't work in windows - if necessary take it out. It is in
// here because some unix platforms complained if it wasn't heere.
#if !defined(WIN32) && !defined(_WIN32)
#include <alloca.h>
#endif

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS 
#define __CL_ENABLE_EXCEPTIONS 
#include "CL/cl.hpp"

#define DEBUG_CL

using namespace puzzler;

class RandomWalkProvider
: public puzzler::RandomWalkPuzzle
{
public:
	RandomWalkProvider()
	{
#ifdef DEBUG_CL
#define ERRMAP(e)	errmap[e] = # e
		ERRMAP(CL_BUILD_PROGRAM_FAILURE);
		ERRMAP(CL_COMPILER_NOT_AVAILABLE);
		ERRMAP(CL_DEVICE_NOT_AVAILABLE);
		ERRMAP(CL_INVALID_ARG_INDEX);
		ERRMAP(CL_INVALID_ARG_SIZE);
		ERRMAP(CL_INVALID_ARG_VALUE);
		ERRMAP(CL_INVALID_BINARY);
		ERRMAP(CL_INVALID_BUFFER_SIZE);
		ERRMAP(CL_INVALID_BUILD_OPTIONS);
		ERRMAP(CL_INVALID_COMMAND_QUEUE);
		ERRMAP(CL_INVALID_CONTEXT);
		ERRMAP(CL_INVALID_DEVICE);
		ERRMAP(CL_INVALID_EVENT_WAIT_LIST);
		ERRMAP(CL_INVALID_GLOBAL_OFFSET);
		ERRMAP(CL_INVALID_HOST_PTR);
		ERRMAP(CL_INVALID_KERNEL);
		ERRMAP(CL_INVALID_KERNEL_ARGS);
		ERRMAP(CL_INVALID_MEM_OBJECT);
		ERRMAP(CL_INVALID_OPERATION);
		ERRMAP(CL_INVALID_PROGRAM_EXECUTABLE);
		ERRMAP(CL_INVALID_PROPERTY);
		ERRMAP(CL_INVALID_SAMPLER);
		ERRMAP(CL_INVALID_VALUE);
		ERRMAP(CL_INVALID_WORK_DIMENSION);
		ERRMAP(CL_INVALID_WORK_GROUP_SIZE);
		ERRMAP(CL_INVALID_WORK_ITEM_SIZE);
		ERRMAP(CL_MEM_OBJECT_ALLOCATION_FAILURE);
		ERRMAP(CL_OUT_OF_HOST_MEMORY);
		ERRMAP(CL_OUT_OF_RESOURCES);
		ERRMAP(CL_SUCCESS);
#endif	// DEBUG_CL
	}

	virtual void Execute(
		puzzler::ILog *log,
		const puzzler::RandomWalkInput *pInput,
		puzzler::RandomWalkOutput *pOutput
	) const override {

		const std::vector<dd_node_t> &nodes(pInput->nodes);

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

		unsigned nodesCount = nodes.size();
		unsigned edgesCount = nodes[0].edges.size();
		unsigned length = pInput->lengthWalks;	// All paths the same length

		// This gives the same sequence on all platforms
		std::mt19937 rng(pInput->seed);

		std::vector<unsigned> seeds(pInput->numSamples);
		std::vector<unsigned> starts(pInput->numSamples);
		for(unsigned i=0; i<pInput->numSamples; i++){
			seeds[i] = rng();
			starts[i] = rng() % nodesCount;    // Choose a random node
		}

		std::vector<unsigned> edges(nodesCount * edgesCount);
		std::vector<uint32_t> counts(nodesCount);

		try {
			// Enumerate available OpenCL platforms
			std::vector<cl::Platform> platforms;

			cl::Platform::get(&platforms);
			if (platforms.size() == 0)
				throw std::runtime_error("No OpenCL platforms found.");

			// Select an OpenCL platform
			int selectedPlatform = 0;
			char *str;
			if ((str = getenv("HPCE_SELECT_PLATFORM")) != NULL)
				selectedPlatform = atoi(str);
			cl::Platform platform(platforms.at(selectedPlatform));

			log->Log(Log_Debug, [&](std::ostream &dst) {
				dst << "Found " << platforms.size() << " platforms\n";
				for (unsigned i = 0; i < platforms.size(); i++) {
					std::string vendor = platforms[i].getInfo<CL_PLATFORM_NAME>();
					dst << "  Platform " << i << " : " << vendor << "\n";
				}
				dst << "Choosing platform " << selectedPlatform << ": " << platform.getInfo<CL_PLATFORM_NAME>() << "\n";
			});

			// Enumerate available OpenCL devices
			std::vector<cl::Device> devices;
			platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);	
			if (devices.size() == 0)
				throw std::runtime_error("No opencl devices found.");

			// Select an OpenCL device
			int selectedDevice = 0;
			// Selecting 'Graphics' as default
			for (unsigned i = 0; i < devices.size(); i++) {
				if (devices[i].getInfo<CL_DEVICE_NAME>().find("Graphics") != std::string::npos) {
					selectedDevice = i;
					break;
				}
			}
			if ((str = getenv("HPCE_SELECT_DEVICE")) != NULL)
				selectedDevice = atoi(str);
			cl::Device device = devices.at(selectedDevice);
			//size_t globalmem = device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
			size_t allocmem = device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
			//std::clog << "Global memory: " << globalmem << ", alloc: " << allocmem << std::endl;

			log->Log(Log_Debug, [&](std::ostream &dst) {
				dst << "Found " << devices.size() << " devices\n";
				for (unsigned i = 0; i < devices.size(); i++) {
					std::string name = devices[i].getInfo<CL_DEVICE_NAME>();
					dst << "  Device " << i << " : " << name << "\n";
				}
				dst << "Choosing device " << selectedDevice << ": " << device.getInfo<CL_DEVICE_NAME>() << "\n";
			});

			log->LogInfo("Starting");

			// Create context for the specified device
			devices.clear();
			devices.push_back(device);
			cl::Context context(devices);

			// Create command queue
			cl::CommandQueue queue(context, device);

			// Allocate and write buffers
			cl::Buffer buffSeeds(context, CL_MEM_READ_ONLY, sizeof(uint32_t) * seeds.size());
			queue.enqueueWriteBuffer(buffSeeds, CL_FALSE, 0, sizeof(uint32_t) * seeds.size(), seeds.data());

			cl::Buffer buffStarts(context, CL_MEM_READ_ONLY, sizeof(unsigned) * starts.size());
			queue.enqueueWriteBuffer(buffStarts, CL_FALSE, 0, sizeof(unsigned) * starts.size(), starts.data());

			unsigned *cptr = counts.data(), *eptr = edges.data();
			for (const dd_node_t &node: nodes) {
				*cptr++ = node.count;
				for (const uint32_t &edge: node.edges)
					*eptr++ = edge;
			}

			cl::Buffer buffEdges(context, CL_MEM_READ_ONLY, sizeof(unsigned) * edges.size());
			queue.enqueueWriteBuffer(buffEdges, CL_FALSE, 0, sizeof(unsigned) * edges.size(), edges.data());

			// Split working buffer because of CL_DEVICE_MAX_MEM_ALLOC_SIZE limitation
			uint32_t blockSize = allocmem / sizeof(uint32_t) / nodesCount;
			blockSize = std::min(blockSize, pInput->numSamples);
			uint32_t blockCount = (pInput->numSamples + blockSize - 1) / blockSize;
			cl::Buffer buffCount(context, CL_MEM_READ_WRITE, sizeof(uint32_t) * nodesCount * blockSize);

			// Summarise and output buffer
			cl::Buffer buffSum(context, CL_MEM_READ_WRITE, sizeof(uint32_t) * nodesCount * (blockCount + 1));
			queue.enqueueWriteBuffer(buffSum, CL_FALSE, 0, sizeof(uint32_t) * counts.size(), counts.data());

			// Create and compile OpenCL program
			std::string kernelSource = LoadSource("random_walk.cl");
			cl::Program::Sources sources;
			sources.push_back(std::make_pair(kernelSource.c_str(), kernelSource.size() + 1));

			cl::Program program(context, sources);
			try {
				program.build(devices);
			} catch (...) {
				for (unsigned i = 0; i < devices.size(); i++) {
					std::cerr << "Log for device " << devices[i].getInfo<CL_DEVICE_NAME>() << ":\n\n";
					std::cerr << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[i]) << "\n\n";
				}
				throw;
			}

			// Create kernel
			cl::Kernel kernel(program, "random_walk");
			cl::Kernel kernel_comb(program, "random_walk_comb");

			// Set kernel parameters
			kernel.setArg(0, buffEdges);
			kernel.setArg(1, buffCount);
			kernel.setArg(2, buffSeeds);
			kernel.setArg(3, buffStarts);
			kernel.setArg(4, length);
			kernel.setArg(5, nodesCount);
			kernel.setArg(6, edgesCount);

			kernel_comb.setArg(1, buffCount);
			kernel_comb.setArg(3, buffSum);

			// Execute the kernel
			for (uint32_t i = 0; i != blockCount; i++) {
				uint32_t size = std::min(blockSize, pInput->numSamples - i * blockSize);
				kernel.setArg(7, i * blockSize);
				cl::NDRange offset(0);			// Iteration starting offset
				cl::NDRange globalSize(size);		// Global size
				cl::NDRange localSize = cl::NullRange;	// Local work-groups N/A
				//queue.enqueueFillBuffer(buffCount, 0u, 0, size * sizeof(uint32_t));
				//queue.enqueueBarrier();
				queue.enqueueNDRangeKernel(kernel, offset, globalSize, localSize);
				kernel_comb.setArg(0, size);
				kernel_comb.setArg(2, i + 1);
				//queue.enqueueBarrier();
				queue.enqueueNDRangeKernel(kernel_comb, cl::NDRange(0), cl::NDRange(nodesCount), cl::NullRange);
			}

			// Summarise
			kernel_comb.setArg(0, blockCount + 1);
			kernel_comb.setArg(1, buffSum);
			kernel_comb.setArg(2, 0);
			kernel_comb.setArg(3, buffSum);
			//queue.enqueueBarrier();
			queue.enqueueNDRangeKernel(kernel_comb, cl::NDRange(0), cl::NDRange(nodesCount), cl::NullRange);

			//queue.enqueueBarrier();
			queue.enqueueReadBuffer(buffSum, CL_TRUE, 0, sizeof(uint32_t) * counts.size(), counts.data());
			goto done;
		} catch (const cl::Error &e) {
			std::cerr << "Exception from " << e.what() << ": ";
			std::map<cl_int, std::string>::const_iterator it;
			if ((it = errmap.find(e.err())) != errmap.end())
				std::cerr << it->second << std::endl;
			else
				std::cerr << "Unknown " << e.err() << std::endl;
			return;
		} catch (const std::exception &e) {
			std::cerr<<"Exception: "<<e.what()<<std::endl;
			return;
		}

#if 0
		std::vector<uint32_t> count(pInput->numSamples * nodesCount);

		//log->LogVerbose("Starting random walks");

		tbb::parallel_for(0u, pInput->numSamples, [&, nodesCount, length, edgesCount](unsigned i){
			random_walk(&edges[0], &count[i * nodesCount], seeds[i], starts[i], length, edgesCount);
		});

#endif

done:
		log->LogVerbose("Done random walks, converting histogram");

		// Map the counts from the nodes back into an array
		pOutput->histogram.resize(nodes.size());
		tbb::parallel_for((size_t)0, nodes.size(), [&](size_t i){
			//uint32_t cnt = nodes[i].count;
			//for (unsigned j = 0; j != pInput->numSamples; j++)
			//	cnt += count[j * nodes.size() + i];
			uint32_t cnt = counts[i];
			pOutput->histogram[i]=std::make_pair(uint32_t(cnt),uint32_t(i));
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
	void random_walk(const unsigned *edges, uint32_t *count, \
			uint32_t seed, unsigned start, unsigned length, unsigned edgesCount) const
	{
		uint32_t rng=seed;
		unsigned current=start;
		while (length--) {
			//nodes[current].count++;
			count[current]++;
			current = edges[current * edgesCount + rng % edgesCount];
			rng = step(rng);
		}
	}

private:
	std::map<cl_int, std::string> errmap;

	uint32_t step(uint32_t x) const
	{
		return x*1664525+1013904223;
	}

	std::string LoadSource(const char *fileName) const
	{
		std::string baseDir = "provider";
		char *str;
		if ((str = getenv("HPCE_CL_SRC_DIR")) != NULL)
			baseDir = str;
		std::string fullName = baseDir + "/" + fileName;

		std::ifstream src(fullName, std::ios::in | std::ios::binary);
		if (!src.is_open())
			throw std::runtime_error("LoadSource : Couldn't load cl file from '" + fullName + "'.");

		return std::string(
				std::istreambuf_iterator<char>(src),
				std::istreambuf_iterator<char>());
	}
};

#endif
