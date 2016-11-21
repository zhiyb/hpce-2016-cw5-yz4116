__kernel void ising_spin(
	__global const uint *seed,
	__global uint *current, 
	__global uint *next, 
	__global const uint *probs)
{
	uint x = get_global_id(0);
	uint y = get_global_id(1);
	uint n = get_global_size(0);
	int W = x==0 ?    current[y*n+n-1]   : current[y*n+x-1];
	int E = x==n-1 ?  current[y*n+0]     : current[y*n+x+1];
	int N = y==0 ?    current[(n-1)*n+x] : current[(y-1)*n+x];
	int S = y==n-1 ?  current[0*n+x]     : current[(y+1)*n+x];
	int nhood=W+E+N+S;

	int C = current[y*n+x];

	uint index=(nhood+4)/2 + 5*(C+1)/2;
	uint prob=probs[index];

	next[y*n+x] = seed[x*n + y] < prob ? -C : C;
	//s = lcg(s);

}

/*__kernel void init(uint n, uint seed, __global uint out)
{
	uint x = get_global_id(0);
	uint y = get_global_id(1);
	uint n = get_global_size(0);
	for(unsigned x=0; x<n; x++){
		for(unsigned y=0; y<n; y++){
			out[y*n+x] = (seed < 0x80001000ul) ? +1 : -1;
			seed = lcg(seed);
		}
	}
}*/
