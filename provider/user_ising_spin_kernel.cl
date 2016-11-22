__kernel void ising_spin(
	__global const uint *seed,
	__global int *current, 
	__global int *next, 
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

__kernel void accumulate(
	__global const int *in, __global int *out,
	uint offset
)
{
	const uint bs = 32;
	uint x = get_global_id(0);
	uint y = get_global_id(1);
	uint bn = get_global_size(0);
	uint n = get_global_size(1);
	uint end = min(n, bn * bs);
	end = min(bs, end - x * bs);
	in += y * n + x * bs;
	out += offset + y * bn + x;
	int s = 0;
	while (end--)
		s += *in++;
	*out = s;
}

__kernel void init(__global float *out)
{
	uint i = get_global_id(0);
	*(out + i) = 0;
}

__kernel void sum(
	__global const int *in, __global float *sums, __global float *sumSquares,
	uint bs
)
{
	uint i = get_global_id(0);
	in += i * bs;
	sums += i;
	sumSquares += i;

	int s = 0;
	while (bs--)
		s += *in++;

	*sums += (float)s;
	*sumSquares += ((float)s * (float)s);
}
