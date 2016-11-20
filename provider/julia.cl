__kernel void julia(
	float2 c,        //! Constant to use in z=z^2+c calculation
	unsigned maxIter,   //! When to give up on a pixel
	//unsigned *pDest     //! Array of width*height pixels, with pixel (x,y) at pDest[width*y+x]
	__global unsigned char *pDest
) {
	uint x = get_global_id(0);
	uint y = get_global_id(1);
	uint w = get_global_size(0);
	uint h = get_global_size(1);
	uint i = y * w + x;

	float dx = 3.0f / (float)w, dy = 3.0f / (float)h;

	float2 z = (float2)(-1.5f + (float)x * dx, -1.5f + (float)y * dy);

	//Perform a julia iteration starting at the point z_0, for offset c.
	//   z_{i+1} = z_{i}^2 + c
	// The point escapes for the first i where |z_{i}| > 2.

	unsigned iter = 0;
	while (iter < maxIter) {
		//if (abs(z) > 2.f)
		if (length(z) > 2)
		//if (z.x * z.x + z.y * z.y > 4.f)
			break;
		// Anybody want to refine/tighten this?
		z = (float2)(z.x * z.x - z.y * z.y, z.x * z.y + z.y * z.x) + c;
		++iter;
	}
	//pOutput->pixels[i] = (dest[i]==pInput->maxIter) ? 0 : (1+(dest[i]%256));
	//pDest[i] = (iter == maxIter) ? 0 : (1 + iter % 256);
	pDest[i] = (iter + 1) % (maxIter + 1);
}

