#ifndef user_julia_hpp
#define user_julia_hpp

#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

#include <tbb/parallel_for.h>
#include "puzzler/puzzles/julia.hpp"

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

#if 0
		// Enumerate available OpenCL platforms
		std::vector<cl::Platform> platforms;

		cl::Platform::get(&platforms);
		if (platforms.size() == 0)
			throw std::runtime_error("No OpenCL platforms found.");

		std::clog << "Found " << platforms.size() << " platforms\n";
		for (unsigned i = 0; i < platforms.size(); i++) {
			std::string vendor = platforms[i].getInfo<CL_PLATFORM_NAME>();
			std::clog << "  Platform " << i << " : " << vendor << "\n";
		}

		// Select an OpenCL platform
		int selectedPlatform = 0;
		if (getenv("HPCE_SELECT_PLATFORM"))
			selectedPlatform = atoi(getenv("HPCE_SELECT_PLATFORM"));
		std::clog << "Choosing platform " << selectedPlatform << "\n";
		cl::Platform platform(platforms.at(selectedPlatform));

		// Enumerate available OpenCL devices
		std::vector<cl::Device> devices;
		platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);	
		if (devices.size() == 0)
			throw std::runtime_error("No opencl devices found.");

		std::clog << "Found " << devices.size() << " devices\n";
		for (unsigned i = 0; i < devices.size(); i++) {
			std::string name = devices[i].getInfo<CL_DEVICE_NAME>();
			std::clog << "  Device " << i << " : " << name << "\n";
		}
#endif

		log->LogInfo("Starting");

		pOutput->pixels.resize(pInput->width*pInput->height);
		juliaFrameRender(
				pInput->width,     //! Number of pixels across
				pInput->height,    //! Number of rows of pixels
				pInput->c,        //! Constant to use in z=z^2+c calculation
				pInput->maxIter,   //! When to give up on a pixel
				//&dest[0]     //! Array of width*height pixels, with pixel (x,y) at pDest[width*y+x]
				&pOutput->pixels[0]
				);

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
		//for(unsigned y=0; y<height; y++){
			//for(unsigned x=0; x<width; x++){
				// Map pixel to z_0
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
				pDest[i] = (iter == maxIter) ? 0 : (1 + iter % 256);
				//pDest[y*width+x] = iter;
			//}
		});
	}
};

#endif
