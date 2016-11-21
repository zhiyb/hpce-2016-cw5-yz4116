/* Start from node start, then follow a random walk of length nodes, incrementing
   the count of all the nodes we visit. */
__kernel void random_walk(__global const unsigned *edges, __global uint *count, \
	__global const uint *seed, __global const unsigned *start, \
	unsigned len, unsigned nodesCount, unsigned edgesCount, unsigned offset)
{
	uint i = get_global_id(0);
	count += i * nodesCount;
	i += offset;
	uint rng = *(seed + i);
	unsigned current = *(start + i);
	
	// Initialise output buffer
	__global uint *ptr = count;
	uint s = nodesCount;
	while (s--)
		*ptr++ = 0;

	while (len--) {
		//nodes[current].count++;
		count[current]++;
		current = edges[current * edgesCount + rng % edgesCount];
		rng = rng * 1664525 + 1013904223;
	}
}

// Summarise count buffers
__kernel void random_walk_comb(uint blocks, __global const uint *in, uint offset, __global uint *out)
{
	uint i = get_global_id(0);
	uint nodesCount = get_global_size(0);
	in += i;
	out += offset * nodesCount + i;
	uint count = 0;
	while (blocks--) {
		count += *in;
		in += nodesCount;
	}
	*out = count;
}
