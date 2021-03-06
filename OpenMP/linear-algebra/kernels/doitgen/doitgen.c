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
#include "doitgen.h"


/* Array initialization. */
static
void init_array(int nr, int nq, int np,
		DATA_TYPE POLYBENCH_3D(A,NR,NQ,NP,nr,nq,np),
		DATA_TYPE POLYBENCH_2D(C4,NP,NP,np,np))
{
  int i, j, k;

  for (i = 0; i < nr; i++)
    for (j = 0; j < nq; j++)
      for (k = 0; k < np; k++)
	A[i][j][k] = ((DATA_TYPE) i*j + k) / np;
  for (i = 0; i < np; i++)
    for (j = 0; j < np; j++)
      C4[i][j] = ((DATA_TYPE) i*j) / np;
}


/* DCE code. Must scan the entire live-out data.
   Can be used also to check the correctness of the output. */
static
void print_array(int nr, int nq, int np,
		 DATA_TYPE POLYBENCH_3D(A,NR,NQ,NP,nr,nq,np))
{
  int i, j, k;

  for (i = 0; i < nr; i++)
    for (j = 0; j < nq; j++)
      for (k = 0; k < np; k++) {
	fprintf (stderr, DATA_PRINTF_MODIFIER, A[i][j][k]);
	if (i % 20 == 0) fprintf (stderr, "\n");
      }
  fprintf (stderr, "\n");
}


/* Main computational kernel. The whole function will be timed,
   including the call and return. */
#ifndef OMP_OFFLOAD
static
void kernel_doitgen(int nr, int nq, int np,
		    DATA_TYPE POLYBENCH_3D(A,NR,NQ,NP,nr,nq,np),
		    DATA_TYPE POLYBENCH_2D(C4,NP,NP,np,np),
		    DATA_TYPE POLYBENCH_3D(sum,NR,NQ,NP,nr,nq,np))
{
  int r, q, p, s;
  #pragma scop
  #pragma omp parallel
  {
    #pragma omp for private (q, p, s)
    for (r = 0; r < _PB_NR; r++)
      for (q = 0; q < _PB_NQ; q++)
	{
          for (p = 0; p < _PB_NP; p++)
	    {
              sum[r][q][p] = 0;
	      for (s = 0; s < _PB_NP; s++)
		sum[r][q][p] = sum[r][q][p] + A[r][q][s] * C4[s][p];
            }
	  for (p = 0; p < _PB_NR; p++)
	    A[r][q][p] = sum[r][q][p];
        }
  }
  #pragma endscop
}
#elif POLYBENCH_OFFLOAD1D
static
void kernel_doitgen(int nr, int nq, int np,
		    DATA_TYPE POLYBENCH_3D_1D(A,NR,NQ,NP,nr,nq,np),
		    DATA_TYPE POLYBENCH_2D_1D(C4,NP,NP,np,np),
		    DATA_TYPE POLYBENCH_3D_1D(sum,NR,NQ,NP,nr,nq,np))
{
#define A_IDX(i,j,k) IDX3(A,i,j,k,nr,nq,np)
#define C4_IDX(i,j) IDX2(C4,i,j,np,np)
#define sum_IDX(i,j,k) IDX3(sum,i,j,k,nr,nq,np)
  int r, q, p, s;
  #pragma omp target data map(to: C4[:np*np], sum[:nr*nq*np]) \
    map(tofrom: A[:nr*nq*np])
  {
    #pragma omp target teams distribute parallel for private (q, p, s)
    for (r = 0; r < _PB_NR; r++)
      for (q = 0; q < _PB_NQ; q++)
	{
          for (p = 0; p < _PB_NP; p++)
	    {
              GET_IDX3(sum,r,q,p) = 0;
	      for (s = 0; s < _PB_NP; s++)
		GET_IDX3(sum,r,q,p) = GET_IDX3(sum,r,q,p) + GET_IDX3(A,r,q,s) * GET_IDX2(C4,s,p);
            }
	  for (p = 0; p < _PB_NR; p++)
	    GET_IDX3(A,r,q,p) = GET_IDX3(sum,r,q,p);
        }
  }
}
#else
static
void kernel_doitgen(int nr, int nq, int np,
		    DATA_TYPE POLYBENCH_3D(A,NR,NQ,NP,nr,nq,np),
		    DATA_TYPE POLYBENCH_2D(C4,NP,NP,np,np),
		    DATA_TYPE POLYBENCH_3D(sum,NR,NQ,NP,nr,nq,np))
{
  int r, q, p, s;
#ifdef OMP_DCAT
  #pragma omp target data map(to: C4, sum) map(tofrom: A)
#else
  #pragma omp target data map(to: C4[:np][:np], sum[:nr][:nq][:np]) \
    map(tofrom: A[:nr][:nq][:np])
#endif
  {
    #pragma omp target teams distribute parallel for private (q, p, s)
    for (r = 0; r < _PB_NR; r++)
      for (q = 0; q < _PB_NQ; q++)
	{
          for (p = 0; p < _PB_NP; p++)
	    {
              sum[r][q][p] = 0;
	      for (s = 0; s < _PB_NP; s++)
		sum[r][q][p] = sum[r][q][p] + A[r][q][s] * C4[s][p];
            }
	  for (p = 0; p < _PB_NR; p++)
	    A[r][q][p] = sum[r][q][p];
        }
  }
}
#endif

int main(int argc, char** argv)
{
  /* Retrieve problem size. */
  int nr = NR;
  int nq = NQ;
  int np = NP;

  /* Variable declaration/allocation. */
  DC_BEGIN();
  POLYBENCH_3D_ARRAY_DECL(A,DATA_TYPE,NR,NQ,NP,nr,nq,np);
  DC_END();
  DC_BEGIN();
  POLYBENCH_3D_ARRAY_DECL(sum,DATA_TYPE,NR,NQ,NP,nr,nq,np);
  DC_END();
  DC_BEGIN();
  POLYBENCH_2D_ARRAY_DECL(C4,DATA_TYPE,NP,NP,np,np);
  DC_END();


  /* Initialize array(s). */
  init_array (nr, nq, np,
	      POLYBENCH_ARRAY(A),
	      POLYBENCH_ARRAY(C4));

  /* Start timer. */
  polybench_start_instruments;

  /* Run kernel. */
  kernel_doitgen (nr, nq, np,
		  POLYBENCH_ARRAY(A),
		  POLYBENCH_ARRAY(C4),
		  POLYBENCH_ARRAY(sum));

  /* Stop and print timer. */
  polybench_stop_instruments;
  polybench_print_instruments;

  /* Prevent dead-code elimination. All live-out data must be printed
     by the function call in argument. */
  polybench_prevent_dce(print_array(nr, nq, np,  POLYBENCH_ARRAY(A)));

  /* Be clean. */
  POLYBENCH_FREE_ARRAY(A);
  POLYBENCH_FREE_ARRAY(sum);
  POLYBENCH_FREE_ARRAY(C4);

  return 0;
}
