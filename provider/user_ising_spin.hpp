#ifndef user_ising_spin_hpp
#define user_ising_spin_hpp

#include <fstream>
#include <thread>

#include <tbb/parallel_for.h>
#include "puzzler/puzzles/ising_spin.hpp"

// Work around deprecation warnings
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS 
#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

#define DEBUG_CL

using namespace tbb;

class IsingSpinProvider : public puzzler::IsingSpinPuzzle
{
public:
	IsingSpinProvider()
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
		const IsingSpinInput *pInput,
		puzzler::IsingSpinOutput *pOutput
	) const override {

		std::vector<float> sums(pInput->maxTime, 0.0);
		std::vector<float> sumSquares(pInput->maxTime, 0.0);
		try{
			std::vector<cl::Platform> platforms;

			cl::Platform::get(&platforms);
			if(platforms.size()==0)
				throw std::runtime_error("No OpenCL platforms found.");
			std::cerr<<"Found "<<platforms.size()<<" platforms\n";
			for(unsigned i=0;i<platforms.size();i++){
				std::string vendor=platforms[i].getInfo<CL_PLATFORM_VENDOR>();
				std::cerr<<"  Platform "<<i<<" : "<<vendor<<"\n";
			}
			int selectedPlatform=0;
			if(getenv("HPCE_SELECT_PLATFORM")){
				selectedPlatform=atoi(getenv("HPCE_SELECT_PLATFORM"));
			}
			cl::Platform platform(platforms.at(selectedPlatform));
			log->Log(Log_Debug, [&](std::ostream &dst) {
					dst << "Found " << platforms.size() << " platforms\n";
					for (unsigned i = 0; i < platforms.size(); i++) {
					std::string vendor = platforms[i].getInfo<CL_PLATFORM_NAME>();
					dst << "  Platform " << i << " : " << vendor << "\n";
					}
					dst << "Choosing platform " << selectedPlatform << ": " << platform.getInfo<CL_PLATFORM_NAME>() << "\n";
					});
			// std::cerr<<"Choosing platform "<<selectedPlatform<<"\n";
			// cl::Platform platform=platforms.at(selectedPlatform);

			std::vector<cl::Device> devices;
			platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);	
			if(devices.size()==0){
				throw std::runtime_error("No opencl devices found.\n");
			}

			std::cerr<<"Found "<<devices.size()<<" devices\n";
			for(unsigned i=0;i<devices.size();i++){
				std::string name=devices[i].getInfo<CL_DEVICE_NAME>();
				std::cerr<<"  Device "<<i<<" : "<<name<<"\n";
			}
			int selectedDevice=0;
			if(getenv("HPCE_SELECT_DEVICE")){
				selectedDevice=atoi(getenv("HPCE_SELECT_DEVICE"));
			}
			// std::cerr<<"Choosing device "<<selectedDevice<<"\n";
			cl::Device device=devices.at(selectedDevice);
			log->Log(Log_Debug, [&](std::ostream &dst) {
					dst << "Found " << devices.size() << " devices\n";
					for (unsigned i = 0; i < devices.size(); i++) {
					std::string name = devices[i].getInfo<CL_DEVICE_NAME>();
					dst << "  Device " << i << " : " << name << "\n";
					}
					dst << "Choosing device " << selectedDevice << ": " << device.getInfo<CL_DEVICE_NAME>() << "\n";
					});

			log->LogInfo("Starting");

			devices.clear();
			devices.push_back(device);
			cl::Context context(devices);

			std::string kernelSource = LoadSource("user_ising_spin_kernel.cl");

			cl::Program::Sources sources;   // A vector of (data,length) pairs
			sources.push_back(std::make_pair(kernelSource.c_str(), kernelSource.size()+1)); // push on our single string
			cl::Program program(context, sources);
			try{
				program.build(devices);
			}catch(...){
				for(unsigned i=0;i<devices.size();i++){
					std::cerr<<"Log for device "<<devices[i].getInfo<CL_DEVICE_NAME>()<<":\n\n";
					std::cerr<<program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[i])<<"\n\n";
				}
				throw;
			}
			program.build(devices);

			unsigned n=pInput->n;
			// size_t cbBuffer = 1 * pInput->maxTime * pInput->repeats;
			size_t cbBuffer = sizeof(uint32_t) * n * n;
			// size_t seBuffer = pInput->repeats;
			// cl::Buffer sumBuffer(context, CL_MEM_WRITE_ONLY, cbBuffer);
			// cl::Buffer sumSquaresBuffer(context, CL_MEM_WRITE_ONLY, cbBuffer);
			std::vector<unsigned> prob(pInput->probs);	
			cl::Buffer probBuffer(context, CL_MEM_READ_ONLY, sizeof(unsigned) * pInput->probs.size());
			cl::Buffer seedBuffer(context, CL_MEM_READ_ONLY, cbBuffer);
			cl::Buffer currentBuffer(context, CL_MEM_READ_WRITE, cbBuffer);
			cl::Buffer nextBuffer(context, CL_MEM_READ_WRITE, cbBuffer);


			// kernel
			cl::Kernel kernel(program, "ising_spin");
			cl::Kernel kernel_init(program, "init");
			cl::Kernel kernel_acc(program, "accumulate");
			cl::Kernel kernel_sum(program, "sum");

			//std::vector<int> current(n*n), next(n*n);

			log->LogInfo("Starting steps.");

			std::mt19937 rng(pInput->seed); // Gives the same sequence on all platforms
			std::vector<uint32_t> seeds(pInput->repeats);
			for (uint32_t &seed: seeds)
				seed = rng();
			// kernel.setArg(0, n);
			// kernel.setArg(2, pInput->maxTime);
			// kernel.setArg(3, pInput->repeats);

			// kernel.setArg(1, seedsBuffer);
			// kernel.setArg(4, sumBuffer);
			// kernel.setArg(5, sumSquaresBuffer);
			kernel.setArg(0, seedBuffer);
			kernel.setArg(3, probBuffer);

			cl::CommandQueue queue(context, device);
			// queue.enqueueWriteBuffer(sumBuffer, CL_TRUE, 0, cbBuffer, &sums[0]);
			// queue.enqueueWriteBuffer(sumSquaresBuffer, CL_TRUE, 0, cbBuffer, &sumSquares[0]);
			queue.enqueueWriteBuffer(probBuffer, CL_TRUE, 0, sizeof(unsigned) * prob.size(), &prob[0]);

			cl::Buffer sumsBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * pInput->maxTime);
			cl::Buffer sumSquaresBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * pInput->maxTime);
			kernel_init.setArg(0, sumsBuffer);
			queue.enqueueNDRangeKernel(kernel_init, cl::NullRange, cl::NDRange(pInput->maxTime), cl::NullRange);
			kernel_init.setArg(0, sumSquaresBuffer);
			queue.enqueueNDRangeKernel(kernel_init, cl::NullRange, cl::NDRange(pInput->maxTime), cl::NullRange);

			uint32_t bn = (n + 31) / 32;
			std::vector<uint32_t> sum(bn * n * pInput->maxTime);
			cl::Buffer accBuffer(context, CL_MEM_READ_WRITE, sizeof(uint32_t) * bn * n * pInput->maxTime);

			kernel_sum.setArg(0, accBuffer);
			kernel_sum.setArg(1, sumsBuffer);
			kernel_sum.setArg(2, sumSquaresBuffer);
			kernel_sum.setArg(3, bn * n);
			kernel_acc.setArg(1, accBuffer);

			std::vector<int32_t> current(n*n * pInput->repeats), next(n*n);
			tbb::parallel_for(0u, pInput->repeats, [&](uint32_t i) {
				init(pInput->n, seeds[i], &current[i * n * n]);
			});

#define TH	3
			struct {
				std::vector<uint32_t> s;
				std::thread thread;
			} threads[TH];

			for (unsigned i = 0; i != TH; i++) {
				threads[i].thread = std::thread([&, i, n](uint32_t seed) {
					threads[i].s.resize(n * n * pInput->maxTime);
					for (unsigned t = 0; t != pInput->maxTime; t++) {
						for (unsigned j = 0; j != n * n; j++) {
							threads[i].s[t * n * n + j] = seed;
							seed = lcg(seed);
						}
					}
				}, seeds[i]);
			}

			//std::vector<uint32_t> s(n * n);
			for (unsigned i = 0u; i < pInput->repeats; i++) {
				//log->LogVerbose("  Repeat %u", i);
				//log->LogInfo("  Repeat %u", i);

				//uint32_t seed = seeds[i];
				//init(pInput->n, seed, &current[0]);
				//size_t idx = i * n * n;
				queue.enqueueWriteBuffer(currentBuffer, CL_TRUE, 0, cbBuffer, &current[i * n * n]);

				threads[i % TH].thread.join();

				for(unsigned t=0; t<pInput->maxTime; t++){
          				// Dump the state of spins on high log levels
					//dump(Log_Debug, pInput, &current[0], log);

					/*for(unsigned j = 0u; j != n*n; j++) {
						s[j] = seed;
						seed = lcg(seed);
					}*/
					//queue.enqueueWriteBuffer(seedBuffer, CL_TRUE, 0, cbBuffer, &s[0]);
					queue.enqueueWriteBuffer(seedBuffer, CL_TRUE, 0, cbBuffer, &threads[i % TH].s[t * n * n]);

					kernel.setArg(1, currentBuffer);
					kernel.setArg(2, nextBuffer);
					cl::NDRange offset(0, 0);
					cl::NDRange globalSize(n, n);
					cl::NDRange localSize = cl::NullRange;
					queue.enqueueNDRangeKernel(kernel, offset, globalSize, localSize);
					//queue.enqueueBarrier();
					// Dump the state of spins on high log levels
					//dump(Log_Debug, pInput, &current[0], log);
					
					kernel_acc.setArg(0, nextBuffer);
					kernel_acc.setArg(2, bn * n * t);
					queue.enqueueNDRangeKernel(kernel_acc, cl::NullRange, cl::NDRange(bn, n), cl::NullRange);
						
					std::swap(currentBuffer, nextBuffer);
				}

				if (i + TH < pInput->repeats) {
					threads[i % TH].thread = std::thread([&, i, n](uint32_t seed) {
						//threads[i].s.resize(n * n * pInput->maxTime);
						for (unsigned t = 0; t != pInput->maxTime; t++) {
							for (unsigned j = 0; j != n * n; j++) {
								threads[i % TH].s[t * n * n + j] = seed;
								seed = lcg(seed);
							}
						}
					}, seeds[i + TH]);
				}

				queue.enqueueNDRangeKernel(kernel_sum, cl::NullRange, cl::NDRange(pInput->maxTime), cl::NullRange);
			}
			queue.enqueueReadBuffer(sumsBuffer, CL_TRUE, 0, sizeof(float) * pInput->maxTime, &sums[0]);
			queue.enqueueReadBuffer(sumSquaresBuffer, CL_TRUE, 0, sizeof(float) * pInput->maxTime, &sumSquares[0]);
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

		log->LogInfo("Calculating final statistics");

		pOutput->means.resize(pInput->maxTime);
		pOutput->stddevs.resize(pInput->maxTime);
		parallel_for(0u, pInput->maxTime, [&](unsigned i){
			double sum = sums[i], sumSquare = sumSquares[i];
			/*double sum = 0.f, sumSquare = 0.f;
			double *pSums = &sums[i * pInput->repeats];
			double *pSumSquares = &sumSquares[i * pInput->repeats];
			for (unsigned i = 0; i != pInput->repeats; i++) {
				sum += *pSums++;
				sumSquare += *pSumSquares++;
			}*/
			pOutput->means[i] = sum / pInput->maxTime;
			pOutput->stddevs[i] = sqrt( sumSquare/pInput->maxTime - pOutput->means[i]*pOutput->means[i] );
			log->LogVerbose("  time %u : mean=%8.6f, stddev=%8.4f", i, pOutput->means[i], pOutput->stddevs[i]);
		});

		log->LogInfo("Finished");
	}
private:
	std::map<cl_int, std::string> errmap;

	uint32_t lcg(uint32_t x) const
	{
		return x*1664525+1013904223;
	}

	void init(
		unsigned n,
		uint32_t &seed,
		int32_t *out
	 ) const {
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
