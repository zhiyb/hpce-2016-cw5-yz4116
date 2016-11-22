#ifndef user_julia_hpp
#define user_julia_hpp

#include <tbb/parallel_for.h>
#include "puzzler/puzzles/julia.hpp"

#include <fstream>

// Update: this doesn't work in windows - if necessary take it out. It is in
// here because some unix platforms complained if it wasn't heere.
#if !defined(WIN32) && !defined(_WIN32)
#include <alloca.h>
#endif

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS 
#define __CL_ENABLE_EXCEPTIONS 
#include "CL/cl.hpp"

class JuliaProvider : public puzzler::JuliaPuzzle
{
public:
	JuliaProvider() {}

	virtual void Execute(
		puzzler::ILog *log,
		const puzzler::JuliaInput *pInput,
		puzzler::JuliaOutput *pOutput
	) const override {
		//std::vector<unsigned> dest(pInput->width*pInput->height);

		if (std::max(pInput->width, pInput->height) < 600)
			goto cpu;

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

			// Create and compile OpenCL program
			std::string kernelSource = LoadSource("julia.cl");
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

			// Allocate buffers
			size_t cbBuffer = 1 * pInput->width * pInput->height;
			cl::Buffer destBuffer(context, CL_MEM_WRITE_ONLY, cbBuffer);

			// Create kernel
			cl::Kernel kernel(program, "julia");

			// Set kernel parameters
			kernel.setArg(0, sizeof(float) * 2, (void *)&pInput->c);
			kernel.setArg(1, (unsigned)pInput->maxIter);
			kernel.setArg(2, destBuffer);

			// Create command queue
			cl::CommandQueue queue(context, device);

			// Execute the kernel after state buffer copied
			cl::NDRange offset(0, 0);				// Iteration starting offset
			cl::NDRange globalSize(pInput->width, pInput->height);	// Global size
			cl::NDRange localSize = cl::NullRange;			// Local work-groups N/A

			queue.enqueueNDRangeKernel(kernel, offset, globalSize, localSize);

			pOutput->pixels.resize(pInput->width * pInput->height);
			queue.enqueueBarrier();

			queue.enqueueReadBuffer(destBuffer, CL_TRUE, 0, cbBuffer, &pOutput->pixels[0]);
			goto done;
		} catch (const cl::Error &e) {
			std::cerr << "Exception from " << e.what() << ": ";
			return;
		} catch (const std::exception &e) {
			std::cerr<<"Exception: "<<e.what()<<std::endl;
			return;
		}

cpu:
		pOutput->pixels.resize(pInput->width*pInput->height);

		juliaFrameRender(
				pInput->width,     //! Number of pixels across
				pInput->height,    //! Number of rows of pixels
				pInput->c,        //! Constant to use in z=z^2+c calculation
				pInput->maxIter,   //! When to give up on a pixel
				//&dest[0]     //! Array of width*height pixels, with pixel (x,y) at pDest[width*y+x]
				&pOutput->pixels[0]
				);

done:
		log->LogInfo("Mapping");

		log->Log(Log_Debug, [&](std::ostream &dst){
			dst<<"\n";
			for(unsigned y=0;y<pInput->height;y++){
				for(unsigned x=0;x<pInput->width;x++){
					//unsigned got=dest[y*pInput->width+x];
					unsigned got=pOutput->pixels[y*pInput->width+x];
					dst<<(got%9);
				}
				dst<<"\n";
			}
		});
		log->LogVerbose("  c = %f,%f,  arg=%f\n", pInput->c.real(), pInput->c.imag(), std::arg(pInput->c));

		//pOutput->pixels.resize(dest.size());
		//tbb::parallel_for((size_t)0, dest.size(), [&](size_t i){
		//	pOutput->pixels[i] = (dest[i]==pInput->maxIter) ? 0 : (1+(dest[i]%256));
		//});

		log->LogInfo("Finished");
	}

private:
	void juliaFrameRender(
			unsigned width,     //! Number of pixels across
			unsigned height,    //! Number of rows of pixels
			complex_t c,        //! Constant to use in z=z^2+c calculation
			unsigned maxIter,   //! When to give up on a pixel
			//unsigned *pDest     //! Array of width*height pixels, with pixel (x,y) at pDest[width*y+x]
			uint8_t *pDest
			) const
	{
		float dx=3.0f/width, dy=3.0f/height;

		tbb::parallel_for(0u, height * width, [&](unsigned i){
			unsigned y = i / width;
			unsigned x = i % width;
			complex_t z(-1.5f+x*dx, -1.5f+y*dy);

			//Perform a julia iteration starting at the point z_0, for offset c.
			//   z_{i+1} = z_{i}^2 + c
			// The point escapes for the first i where |z_{i}| > 2.

			unsigned iter=0;
			while(iter<maxIter){
				if(abs(z) > 2){
					break;
				}
				// Anybody want to refine/tighten this?
				z = z*z + c;
				++iter;
			}
			//pOutput->pixels[i] = (dest[i]==pInput->maxIter) ? 0 : (1+(dest[i]%256));
			//pDest[i] = (iter == maxIter) ? 0 : (1 + iter % 256);
			pDest[i] = (iter + 1) % (maxIter + 1);
			//pDest[y*width+x] = iter;
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
