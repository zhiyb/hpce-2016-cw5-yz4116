#ifndef puzzler_puzzles_julia_hpp
#define puzzler_puzzles_julia_hpp

#include <random>
#include <sstream>
#include <algorithm>
#include <complex>

#include "puzzler/core/puzzle.hpp"

namespace puzzler
{
  typedef std::complex<float> complex_t;
  
  class JuliaPuzzle;
  class JuliaInput;
  class JuliaOutput;

  class JuliaInput
    : public Puzzle::Input
  {
  public:
    unsigned width;
    unsigned height;
    complex_t c; 
    unsigned maxIter; 
  
    JuliaInput(const Puzzle *puzzle, int scale)
      : Puzzle::Input(puzzle, scale)
    {}

    JuliaInput(std::string format, std::string name, PersistContext &ctxt)
      : Puzzle::Input(format, name, ctxt)
    {
      PersistImpl(ctxt);
    }

    virtual void PersistImpl(PersistContext &conn) override final
    {
      conn.SendOrRecv(width);
      conn.SendOrRecv(height);
      conn.SendOrRecv(c);
      conn.SendOrRecv(maxIter);
    }
  };

  class JuliaOutput
    : public Puzzle::Output
  {
  public:
    std::vector<uint8_t> pixels;
  
    JuliaOutput(const Puzzle *puzzle, const Puzzle::Input *input)
      : Puzzle::Output(puzzle, input)
    {}

    JuliaOutput(std::string format, std::string name, PersistContext &ctxt)
      : Puzzle::Output(format, name, ctxt)
    {
      PersistImpl(ctxt);
    }

    virtual void PersistImpl(PersistContext &conn) override
    {
      conn.SendOrRecv(pixels);
    }

    virtual bool Equals(const Output *output) const override
    {
      auto pOutput=As<JuliaOutput>(output);
      return pixels==pOutput->pixels;
    }

  };


  class JuliaPuzzle
    : public PuzzleBase<JuliaInput,JuliaOutput>
  {
    
  private:
    
    complex_t chooseC(float t) const
    {
        return 0.7f*exp(-float(t)*complex_t(0,1));
    }  
  
    void juliaFrameRender_Reference(
        unsigned width,     //! Number of pixels across
        unsigned height,    //! Number of rows of pixels
        complex_t c,        //! Constant to use in z=z^2+c calculation
        unsigned maxIter,   //! When to give up on a pixel
        unsigned *pDest     //! Array of width*height pixels, with pixel (x,y) at pDest[width*y+x]
    ) const
    {
        float dx=3.0f/width, dy=3.0f/height;

        for(unsigned y=0; y<height; y++){
            for(unsigned x=0; x<width; x++){
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
                pDest[y*width+x] = iter;
            }
        }
    }

  protected:
    virtual void Execute(
			 ILog *log,
			 const JuliaInput *input,
			 JuliaOutput *output
			 ) const =0;


    void ReferenceExecute(
			  ILog *log,
			  const JuliaInput *pInput,
			  JuliaOutput *pOutput
			  ) const
    {
      std::vector<unsigned> dest(pInput->width*pInput->height);
      
      log->LogInfo("Starting");
      
      juliaFrameRender_Reference(
          pInput->width,     //! Number of pixels across
          pInput->height,    //! Number of rows of pixels
          pInput->c,        //! Constant to use in z=z^2+c calculation
          pInput->maxIter,   //! When to give up on a pixel
          &dest[0]     //! Array of width*height pixels, with pixel (x,y) at pDest[width*y+x]
      );
      
      log->LogInfo("Mapping");
      
      log->Log(Log_Debug, [&](std::ostream &dst){
        dst<<"\n";
        for(unsigned y=0;y<pInput->height;y++){
          for(unsigned x=0;x<pInput->width;x++){
            unsigned got=dest[y*pInput->width+x];
            dst<<(got%9);
          }
          dst<<"\n";
        }
      });
      log->LogVerbose("  c = %f,%f,  arg=%f\n", pInput->c.real(), pInput->c.imag(), std::arg(pInput->c));
      
      pOutput->pixels.resize(dest.size());
      for(unsigned i=0; i<dest.size(); i++){
        pOutput->pixels[i] = (dest[i]==pInput->maxIter) ? 0 : (1+(dest[i]%256));
      }
      
      log->LogInfo("Finished");
    }

  public:
    virtual std::string Name() const override
    { return "julia"; }

    virtual std::shared_ptr<Input> CreateInput(
					       ILog *,
					       int scale
					       ) const override
    {
      std::mt19937 rnd(time(0));  // Not the best way of seeding...
      std::uniform_real_distribution<> udist;

      auto params=std::make_shared<JuliaInput>(this, scale);

      params->width=scale;
      params->height=(scale*2)/3;
      params->maxIter=scale;
      params->c=chooseC( 1.3+0.6*udist(rnd) );
      

      return params;
    }

  };

};

#endif
