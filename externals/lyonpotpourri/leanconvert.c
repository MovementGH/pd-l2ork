#include "fftease.h"

void leanconvert( float *S, float *C, int N2 )

{

 int		real, imag,
		amp, phase;
 float		a, b;
  int		i;
	
 for ( i = 0; i <= N2; i++ ) {
   imag = phase = ( real = amp = i<<1 ) + 1;
   a = ( i == N2 ? S[1] : S[real] );
   b = ( i == 0 || i == N2 ? 0. : S[imag] );
   C[amp] = hypot( a, b );
   C[phase] = -atan2( b, a );
 }
}

