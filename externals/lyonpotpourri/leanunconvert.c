#include "fftease.h"

/* unconvert essentially undoes what convert does, i.e., it
  turns N2+1 PAIRS of amplitude and frequency values in
  C into N2 PAIR of complex spectrum data (in rfft format)
  in output array S; sampling rate R and interpolation factor
  I are used to recompute phase values from frequencies */

void leanunconvert( float *C, float *S, int N2 )

{
  int		real, imag,
		amp, phase;
  register int		i;
  
  for ( i = 0; i <= N2; i++ ) {
    imag = phase = ( real = amp = i<<1 ) + 1;
    S[real] = *(C+amp) * cos( *(C+phase) );
    if ( i != N2 )
      S[imag] = -*(C+amp) * sin( *(C+phase) );
  }
}

