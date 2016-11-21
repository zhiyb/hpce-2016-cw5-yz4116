#ifndef user_ising_spin_hpp
#define user_ising_spin_hpp

#include <fstream>

#include <tbb/parallel_for.h>
#include "puzzler/puzzles/ising_spin.hpp"

// Work around deprecation warnings
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS 
#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

using namespace tbb;

class IsingSpinProvider : public puzzler::IsingSpinPuzzle
{
public:
	IsingSpinProvider() {}

	virtual void Execute(
			puzzler::ILog *log,
			const IsingSpinInput *pInput,
			puzzler::IsingSpinOutput *pOutput
			) const override {
		std::map<cl_int, std::string> errmap;

#define ERRMAP(e)	errmap[e] = # e
		ERRMAP(CL_SUCCESS);
		ERRMAP(CL_INVALID_PROPERTY);
		ERRMAP(CL_INVALID_VALUE);
		ERRMAP(CL_INVALID_DEVICE);
		ERRMAP(CL_DEVICE_NOT_AVAILABLE);
		ERRMAP(CL_OUT_OF_HOST_MEMORY);
		ERRMAP(CL_INVALID_BINARY);
		ERRMAP(CL_INVALID_BUILD_OPTIONS);
		ERRMAP(CL_INVALID_OPERATION);
		ERRMAP(CL_COMPILER_NOT_AVAILABLE);
		ERRMAP(CL_BUILD_PROGRAM_FAILURE);
		ERRMAP(CL_OUT_OF_RESOURCES);
		ERRMAP(CL_OUT_OF_HOST_MEMORY);

			std::vector<double> sums(pInput->maxTime * pInput->repeats, 0.0);
			std::vector<double> sumSquares(pInput->maxTime * pInput->repeats, 0.0);
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
			size_t cbBuffer = 1 * n * n;
			// size_t seBuffer = pInput->repeats;
			// cl::Buffer sumBuffer(context, CL_MEM_WRITE_ONLY, cbBuffer);
			// cl::Buffer sumSquaresBuffer(context, CL_MEM_WRITE_ONLY, cbBuffer);
			cl::Buffer probBuffer(context, CL_MEM_READ_ONLY, cbBuffer);
			cl::Buffer seedsBuffer(context, CL_MEM_READ_WRITE, pcbBuffer);
			std::vector<unsigned> prob(pInput->probs);	


			// kernel
			cl::Kernel kernel(program, "ising_spin");


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
			kernel.setArg(3, probBuffer);

			cl::CommandQueue queue(context, device);
			// queue.enqueueWriteBuffer(sumBuffer, CL_TRUE, 0, cbBuffer, &sums[0]);
			// queue.enqueueWriteBuffer(sumSquaresBuffer, CL_TRUE, 0, cbBuffer, &sumSquares[0]);
			queue.enqueueWriteBuffer(probBuffer, CL_TRUE, 0, cbBuffer, &prob[0]);

			cl::NDRange offset(0, 0);
			cl::NDRange globalSize(n, n);
			cl::NDRange localSize = cl::NullRange;
			tbb::parallel_for(0u, pInput->repeats, [=, &seeds, &log, &sums, &sumSquares](unsigned i){
					std::vector<int> current(n*n), next(n*n);
					uint32_t seed = seeds[i];

					//log->LogVerbose("  Repeat %u", i);

					//size_t idx = i * n * n;
					init(pInput, seed, &current[0]);

					for(unsigned t=0; t<pInput->maxTime; t++){
					std::vector<uint32_t> s(n*n);
					for(&seed: s)
						seed = lcg(seed);
					kernel.setArg(0, seedBuffer);
					queue.enqueueWriteBuffer(seedsBuffer, CL_TRUE, 0, cbBuffer, &s[0]);
					queue.enqueueNDRangeKernel(kernel, offset, globalSize, localSize);
					queue.enqueueBarrier();
					// Dump the state of spins on high log levels
					//dump(Log_Debug, pInput, &current[0], log);

					// step(pInput, seed, &current[0], &next[0]);
					std::swap(current, next);

					// Track the statistics
					double countPositive=count(pInput, &current[0]);
					sums[i + t * pInput->repeats] = countPositive;
					sumSquares[i + t * pInput->repeats] = countPositive*countPositive;
					}
			});
			/*for(unsigned i = 0; i < pInput->repeats; i++)
			  {
			  kernel_xy(i, n, &seeds[0], pInput->maxTime, pInput->repeats, &sums[0], &sumSquares[0], &prob[0]);
			  }*/
			// queue.enqueueReadBuffer(sumBuffer, CL_TRUE, 0, cbBuffer, &sums[0]);
			// queue.enqueueReadBuffer(sumSquaresBuffer, CL_TRUE, 0, cbBuffer, &sumSquares[0]);
		}catch (const cl::Error &e) {
			std::cerr << "Exception from " << e.what() << ": ";
			return;
		}

		log->LogInfo("Calculating final statistics");

		pOutput->means.resize(pInput->maxTime);
		pOutput->stddevs.resize(pInput->maxTime);
		parallel_for(0u, pInput->maxTime, [&](unsigned i){
				double sum = 0.f, sumSquare = 0.f;
				double *pSums = &sums[i * pInput->repeats];
				double *pSumSquares = &sumSquares[i * pInput->repeats];
				for (unsigned i = 0; i != pInput->repeats; i++) {
				sum += *pSums++;
				sumSquare += *pSumSquares++;
				}
				pOutput->means[i] = sum / pInput->maxTime;
				pOutput->stddevs[i] = sqrt( sumSquare/pInput->maxTime - pOutput->means[i]*pOutput->means[i] );
				log->LogVerbose("  time %u : mean=%8.6f, stddev=%8.4f", i, pOutput->means[i], pOutput->stddevs[i]);
				});

		log->LogInfo("Finished");
	}
private:
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

	int count(
		const IsingSpinInput *pInput,
		const int *in
	 ) const {
		unsigned n=pInput->n;

		return std::accumulate(in, in+n*n, 0);
	}

	uint32_t lcg(uint32_t x) const
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
