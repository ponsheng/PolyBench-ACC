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

// FIXME
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Include polybench common header. */
#include <polybench.h>

/* Include benchmark-specific header. */
/* Default data type is double, default size is 4000. */
#include "atax.h"


/* Array initialization. */
static
void init_array (int nx, int ny,
		 DATA_TYPE POLYBENCH_2D(A,NX,NY,nx,ny),
		 DATA_TYPE POLYBENCH_1D(x,NY,ny))
{
  int i, j;

  for (i = 0; i < ny; i++)
      x[i] = i * M_PI;
  for (i = 0; i < nx; i++)
    for (j = 0; j < ny; j++)
      A[i][j] = ((DATA_TYPE) i*(j+1)) / nx;
}


/* DCE code. Must scan the entire live-out data.
   Can be used also to check the correctness of the output. */
static
void print_array(int nx,
		 DATA_TYPE POLYBENCH_1D(y,NX,nx))

{
  int i;

  for (i = 0; i < nx; i++) {
    fprintf (stderr, DATA_PRINTF_MODIFIER, y[i]);
    if (i % 20 == 0) fprintf (stderr, "\n");
  }
  fprintf (stderr, "\n");
}


/* Main computational kernel. The whole function will be timed,
   including the call and return. */
#ifndef OMP_OFFLOAD
static
void kernel_atax(int nx, int ny,
		 DATA_TYPE POLYBENCH_2D(A,NX,NY,nx,ny),
		 DATA_TYPE POLYBENCH_1D(x,NY,ny),
		 DATA_TYPE POLYBENCH_1D(y,NY,ny),
		 DATA_TYPE POLYBENCH_1D(tmp,NX,nx))
{
  int i, j;
  #pragma scop
  #pragma omp parallel
  {
    #pragma omp for
    for (i = 0; i < _PB_NY; i++)
      y[i] = 0;
    #pragma omp for private (j)
    for (i = 0; i < _PB_NX; i++) {
	    tmp[i] = 0;
        for (j = 0; j < _PB_NY; j++)
          tmp[i] = tmp[i] + A[i][j] * x[j];
        for (j = 0; j < _PB_NY; j++) {
            DATA_TYPE temp = A[i][j] * tmp[i];
            #pragma omp atomic
            y[j] += temp;
        }
    }
  }
  #pragma endscop
}
#elif defined POLYBENCH_OFFLOAD1D
static
void kernel_atax(int nx, int ny,
		 DATA_TYPE POLYBENCH_2D_1D(A,NX,NY,nx,ny),
		 DATA_TYPE POLYBENCH_1D(x,NY,ny),
		 DATA_TYPE POLYBENCH_1D(y,NY,ny),
		 DATA_TYPE POLYBENCH_1D(tmp,NX,nx))
{
  int i, j;
#define A_IDX(i,j) IDX2(A,i,j,nx,ny)
  {
    #pragma omp for
    for (i = 0; i < _PB_NY; i++)
      y[i] = 0;
    #pragma omp target data map(to:A[:NX*NY], x[:NY], tmp[:NX]) map(tofrom: y[:NY])
    #pragma omp target teams distribute parallel for private (j)
    for (i = 0; i < _PB_NX; i++) {
	    tmp[i] = 0;
        for (j = 0; j < _PB_NY; j++)
          tmp[i] = tmp[i] + GET_IDX2(A,i,j) * x[j];
        for (j = 0; j < _PB_NY; j++) {
            DATA_TYPE temp = GET_IDX2(A,i,j) * tmp[i];
            #pragma omp atomic
            y[j] += temp;
        }
    }
  }
}
#else
static
void kernel_atax(int nx, int ny,
		 DATA_TYPE POLYBENCH_2D(A,NX,NY,nx,ny),
		 DATA_TYPE POLYBENCH_1D(x,NY,ny),
		 DATA_TYPE POLYBENCH_1D(y,NY,ny),
		 DATA_TYPE POLYBENCH_1D(tmp,NX,nx))
{
  int i, j;
  {
    #pragma omp for
    for (i = 0; i < _PB_NY; i++)
      y[i] = 0;
#ifdef OMP_DCAT
    #pragma omp target data map(to:A, x[:NY], tmp[:NX]) map(tofrom: y[:NY])
#else
    #pragma omp target data map(to:A[:NX][:NY], x[:NY], tmp[:NX]) map(tofrom: y[:NY])
#endif
    #pragma omp target teams distribute parallel for private (j)
    for (i = 0; i < _PB_NX; i++) {
	    tmp[i] = 0;
        for (j = 0; j < _PB_NY; j++)
          tmp[i] = tmp[i] + A[i][j] * x[j];
        for (j = 0; j < _PB_NY; j++) {
            DATA_TYPE temp = A[i][j] * tmp[i];
            #pragma omp atomic
            y[j] += temp;
        }
    }
  }
}
#endif


int main(int argc, char** argv)
{
  /* Retrieve problem size. */
  int nx = NX;
  int ny = NY;

  /* Variable declaration/allocation. */
  DC_BEGIN();
  POLYBENCH_2D_ARRAY_DECL(A, DATA_TYPE, NX, NY, nx, ny);
  DC_END();
  POLYBENCH_1D_ARRAY_DECL(x, DATA_TYPE, NY, ny);
  POLYBENCH_1D_ARRAY_DECL(y, DATA_TYPE, NY, ny);
  POLYBENCH_1D_ARRAY_DECL(tmp, DATA_TYPE, NX, nx);

  /* Initialize array(s). */
  init_array (nx, ny, POLYBENCH_ARRAY(A), POLYBENCH_ARRAY(x));

  /* Start timer. */
  polybench_start_instruments;

  /* Run kernel. */
  kernel_atax (nx, ny,
	       POLYBENCH_ARRAY(A),
	       POLYBENCH_ARRAY(x),
	       POLYBENCH_ARRAY(y),
	       POLYBENCH_ARRAY(tmp));

  /* Stop and print timer. */
  polybench_stop_instruments;
  polybench_print_instruments;

  /* Prevent dead-code elimination. All live-out data must be printed
     by the function call in argument. */
  polybench_prevent_dce(print_array(nx, POLYBENCH_ARRAY(y)));

  /* Be clean. */
  POLYBENCH_FREE_ARRAY(A);
  POLYBENCH_FREE_ARRAY(x);
  POLYBENCH_FREE_ARRAY(y);
  POLYBENCH_FREE_ARRAY(tmp);

  return 0;
}
