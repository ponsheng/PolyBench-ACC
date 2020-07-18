/* POLYBENCH/GPU-OPENMP
 *
 * This file is a part of the Polybench/GPU-OpenMP suite
 *
 * Contact:
 * William Killian <killian@udel.edu>
 *
 * Copyright 2013, The University of Delaware
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

/* Include polybench common header. */
#include <polybench.h>

/* Include benchmark-specific header. */
/* Default data type is double, default size is 4000. */
#include "2mm.h"


/* Array initialization. */
static
void init_array(int ni, int nj, int nk, int nl,
		DATA_TYPE *alpha,
		DATA_TYPE *beta,
		DATA_TYPE POLYBENCH_2D(A,NI,NK,ni,nl),
		DATA_TYPE POLYBENCH_2D(B,NK,NJ,nk,nj),
		DATA_TYPE POLYBENCH_2D(C,NL,NJ,nl,nj),
		DATA_TYPE POLYBENCH_2D(D,NI,NL,ni,nl))
{
  int i, j;

  *alpha = 32412;
  *beta = 2123;
  for (i = 0; i < ni; i++)
    for (j = 0; j < nk; j++)
      A[i][j] = ((DATA_TYPE) i*j) / ni;
  for (i = 0; i < nk; i++)
    for (j = 0; j < nj; j++)
      B[i][j] = ((DATA_TYPE) i*(j+1)) / nj;
  for (i = 0; i < nl; i++)
    for (j = 0; j < nj; j++)
      C[i][j] = ((DATA_TYPE) i*(j+3)) / nl;
  for (i = 0; i < ni; i++)
    for (j = 0; j < nl; j++)
      D[i][j] = ((DATA_TYPE) i*(j+2)) / nk;
}


/* DCE code. Must scan the entire live-out data.
   Can be used also to check the correctness of the output. */
static
void print_array(int ni, int nl,
		 DATA_TYPE POLYBENCH_2D(D,NI,NL,ni,nl))
{
  int i, j;

  for (i = 0; i < ni; i++)
    for (j = 0; j < nl; j++) {
	fprintf (stderr, DATA_PRINTF_MODIFIER, D[i][j]);
	if ((i * ni + j) % 20 == 0) fprintf (stderr, "\n");
    }
  fprintf (stderr, "\n");
}


/* Main computational kernel. The whole function will be timed,
   including the call and return. */
#ifndef OMP_OFFLOAD
static
void kernel_2mm(int ni, int nj, int nk, int nl,
		DATA_TYPE alpha,
		DATA_TYPE beta,
		DATA_TYPE POLYBENCH_2D(tmp,NI,NJ,ni,nj),
		DATA_TYPE POLYBENCH_2D(A,NI,NK,ni,nk),
		DATA_TYPE POLYBENCH_2D(B,NK,NJ,nk,nj),
		DATA_TYPE POLYBENCH_2D(C,NL,NJ,nl,nj),
		DATA_TYPE POLYBENCH_2D(D,NI,NL,ni,nl))
{
  int i, j, k;
  #pragma scop
  /* D := alpha*A*B*C + beta*D */
  #pragma omp parallel
  {
    #pragma omp for private (j, k)
    for (i = 0; i < _PB_NI; i++)
      for (j = 0; j < _PB_NJ; j++)
  	{
    	  tmp[i][j] = 0;
  	  for (k = 0; k < _PB_NK; ++k)
	    tmp[i][j] += alpha * A[i][k] * B[k][j];
        }
    #pragma omp for private (j, k)
    for (i = 0; i < _PB_NI; i++)
      for (j = 0; j < _PB_NL; j++)
        {
	  D[i][j] *= beta;
	  for (k = 0; k < _PB_NJ; ++k)
	    D[i][j] += tmp[i][k] * C[k][j];
	}
  }
  #pragma endscop
}
#elif defined POLYBENCH_OFFLOAD1D
static void kernel_2mm(int ni, int nj, int nk, int nl,
		DATA_TYPE alpha,
		DATA_TYPE beta,
		DATA_TYPE POLYBENCH_2D_1D(tmp,NI,NJ,ni,nj),
		DATA_TYPE POLYBENCH_2D_1D(A,NI,NK,ni,nk),
		DATA_TYPE POLYBENCH_2D_1D(B,NK,NJ,nk,nj),
		DATA_TYPE POLYBENCH_2D_1D(C,NL,NJ,nl,nj),
		DATA_TYPE POLYBENCH_2D_1D(D,NI,NL,ni,nl))
{
  int i, j, k;
  #pragma omp target data map(to:tmp[:NI*NJ], A[:NI*NK], B[:NK*NJ], C[:NL*NJ]) \
    map(tofrom: D[:NI*NL])
  {
#define tmp_IDX(i, j) IDX2(tmp, i, j, ni, nj)
#define A_IDX(i, j) IDX2(A, i, j, ni, nk)
#define B_IDX(i, j) IDX2(B, i, j, nk, nj)
#define C_IDX(i, j) IDX2(C, i, j, nl, nj)
#define D_IDX(i, j) IDX2(D, i, j, ni, nl)
    #pragma omp target teams distribute parallel for private (j, k)
    for (i = 0; i < _PB_NI; i++)
      for (j = 0; j < _PB_NJ; j++) {
        tmp_IDX(i,j) = 0;
          for (k = 0; k < _PB_NK; ++k)
            tmp_IDX(i,j) += alpha * A_IDX(i,k) * B_IDX(k,j);
      }

    #pragma omp target teams distribute parallel for private (j, k)
    for (i = 0; i < _PB_NI; i++)
      for (j = 0; j < _PB_NL; j++) {
	    D_IDX(i,j) *= beta;
	    for (k = 0; k < _PB_NJ; ++k)
	      D_IDX(i,j) += tmp_IDX(i,k) * C_IDX(k,j);
	}
  }
}
#else
static void kernel_2mm(int ni, int nj, int nk, int nl,
		DATA_TYPE alpha,
		DATA_TYPE beta,
		DATA_TYPE POLYBENCH_2D(tmp,NI,NJ,ni,nj),
		DATA_TYPE POLYBENCH_2D(A,NI,NK,ni,nk),
		DATA_TYPE POLYBENCH_2D(B,NK,NJ,nk,nj),
		DATA_TYPE POLYBENCH_2D(C,NL,NJ,nl,nj),
		DATA_TYPE POLYBENCH_2D(D,NI,NL,ni,nl))
{
  int i, j, k;
  #pragma omp target data map(to:tmp[:NI][:NJ], A[:NI][:NK], B[:NK][:NJ], \
          C[:NL][:NJ]) map(tofrom: D[:NI][:NL])
  {
    #pragma omp target teams distribute parallel for private (j, k)
    for (i = 0; i < _PB_NI; i++)
      for (j = 0; j < _PB_NJ; j++) {
        tmp[i][j] = 0;
          for (k = 0; k < _PB_NK; ++k)
            tmp[i][j] += alpha * A[i][k] * B[k][j];
      }
    #pragma omp target teams distribute parallel for private (j, k)
    for (i = 0; i < _PB_NI; i++)
      for (j = 0; j < _PB_NL; j++) {
	    D[i][j] *= beta;
	    for (k = 0; k < _PB_NJ; ++k)
	      D[i][j] += tmp[i][k] * C[k][j];
	}
  }
}
#endif


int main(int argc, char** argv)
{
  /* Retrieve problem size. */
  int ni = NI;
  int nj = NJ;
  int nk = NK;
  int nl = NL;

  /* Variable declaration/allocation. */
  DATA_TYPE alpha;
  DATA_TYPE beta;
  POLYBENCH_2D_ARRAY_DECL(tmp,DATA_TYPE,NI,NJ,ni,nj);
  POLYBENCH_2D_ARRAY_DECL(A,DATA_TYPE,NI,NK,ni,nk);
  POLYBENCH_2D_ARRAY_DECL(B,DATA_TYPE,NK,NJ,nk,nj);
  POLYBENCH_2D_ARRAY_DECL(C,DATA_TYPE,NL,NJ,nl,nj);
  POLYBENCH_2D_ARRAY_DECL(D,DATA_TYPE,NI,NL,ni,nl);

  /* Initialize array(s). */
  init_array (ni, nj, nk, nl, &alpha, &beta,
	      POLYBENCH_ARRAY(A),
	      POLYBENCH_ARRAY(B),
	      POLYBENCH_ARRAY(C),
	      POLYBENCH_ARRAY(D));

  /* Start timer. */
  polybench_start_instruments;


  /* Run kernel. */
  kernel_2mm (ni, nj, nk, nl,
	      alpha, beta,
	      POLYBENCH_ARRAY(tmp),
	      POLYBENCH_ARRAY(A),
	      POLYBENCH_ARRAY(B),
	      POLYBENCH_ARRAY(C),
	      POLYBENCH_ARRAY(D));

  /* Stop and print timer. */
  polybench_stop_instruments;
  polybench_print_instruments;

  /* Prevent dead-code elimination. All live-out data must be printed
     by the function call in argument. */
  polybench_prevent_dce(print_array(ni, nl,  POLYBENCH_ARRAY(D)));

  /* Be clean. */
  POLYBENCH_FREE_ARRAY(tmp);
  POLYBENCH_FREE_ARRAY(A);
  POLYBENCH_FREE_ARRAY(B);
  POLYBENCH_FREE_ARRAY(C);
  POLYBENCH_FREE_ARRAY(D);

  return 0;
}
