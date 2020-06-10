/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2016 Matthieu Schaller (matthieu.schaller@durham.ac.uk)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

/* Config parameters. */
#include "../config.h"

#ifdef HAVE_FFTW
#include <fftw3.h>
#if defined(WITH_MPI) && defined(HAVE_MPI_FFTW)
#include <fftw3-mpi.h>
#endif
#endif

#include <math.h>

/* This object's header. */
#include "mesh_gravity.h"

/* Local includes. */
#include "accumulate.h"
#include "active.h"
#include "debug.h"
#include "engine.h"
#include "error.h"
#include "gravity_properties.h"
#include "kernel_long_gravity.h"
#include "part.h"
#include "restart.h"
#include "runner.h"
#include "space.h"
#include "threadpool.h"

#include "hashmap.h"
#include "row_major_id.h"
#include "mesh_gravity_mpi.h"

#ifdef HAVE_FFTW
/**
 * @brief Interpolate values from a the mesh using CIC.
 *
 * @param mesh The mesh to read from.
 * @param i The index of the cell along x
 * @param j The index of the cell along y
 * @param k The index of the cell along z
 * @param tx First CIC coefficient along x
 * @param ty First CIC coefficient along y
 * @param tz First CIC coefficient along z
 * @param dx Second CIC coefficient along x
 * @param dy Second CIC coefficient along y
 * @param dz Second CIC coefficient along z
 */
__attribute__((always_inline, const)) INLINE static double CIC_get(
    double mesh[6][6][6], const int i, const int j, const int k,
    const double tx, const double ty, const double tz, const double dx,
    const double dy, const double dz) {

  double temp;
  temp = mesh[i + 0][j + 0][k + 0] * tx * ty * tz;
  temp += mesh[i + 0][j + 0][k + 1] * tx * ty * dz;
  temp += mesh[i + 0][j + 1][k + 0] * tx * dy * tz;
  temp += mesh[i + 0][j + 1][k + 1] * tx * dy * dz;
  temp += mesh[i + 1][j + 0][k + 0] * dx * ty * tz;
  temp += mesh[i + 1][j + 0][k + 1] * dx * ty * dz;
  temp += mesh[i + 1][j + 1][k + 0] * dx * dy * tz;
  temp += mesh[i + 1][j + 1][k + 1] * dx * dy * dz;

  return temp;
}

/**
 * @brief Interpolate a value to a mesh using CIC.
 *
 * @param mesh The mesh to write to
 * @param N The side-length of the mesh
 * @param i The index of the cell along x
 * @param j The index of the cell along y
 * @param k The index of the cell along z
 * @param tx First CIC coefficient along x
 * @param ty First CIC coefficient along y
 * @param tz First CIC coefficient along z
 * @param dx Second CIC coefficient along x
 * @param dy Second CIC coefficient along y
 * @param dz Second CIC coefficient along z
 * @param value The value to interpolate.
 */
__attribute__((always_inline)) INLINE static void CIC_set(
    double* mesh, const int N, const int i, const int j, const int k,
    const double tx, const double ty, const double tz, const double dx,
    const double dy, const double dz, const double value) {

  /* Classic CIC interpolation */
  atomic_add_d(&mesh[row_major_id_periodic(i + 0, j + 0, k + 0, N)],
               value * tx * ty * tz);
  atomic_add_d(&mesh[row_major_id_periodic(i + 0, j + 0, k + 1, N)],
               value * tx * ty * dz);
  atomic_add_d(&mesh[row_major_id_periodic(i + 0, j + 1, k + 0, N)],
               value * tx * dy * tz);
  atomic_add_d(&mesh[row_major_id_periodic(i + 0, j + 1, k + 1, N)],
               value * tx * dy * dz);
  atomic_add_d(&mesh[row_major_id_periodic(i + 1, j + 0, k + 0, N)],
               value * dx * ty * tz);
  atomic_add_d(&mesh[row_major_id_periodic(i + 1, j + 0, k + 1, N)],
               value * dx * ty * dz);
  atomic_add_d(&mesh[row_major_id_periodic(i + 1, j + 1, k + 0, N)],
               value * dx * dy * tz);
  atomic_add_d(&mesh[row_major_id_periodic(i + 1, j + 1, k + 1, N)],
               value * dx * dy * dz);
}

/**
 * @brief Assigns a given #gpart to a density mesh using the CIC method.
 *
 * @param gp The #gpart.
 * @param rho The density mesh.
 * @param N the size of the mesh along one axis.
 * @param fac The width of a mesh cell.
 * @param dim The dimensions of the simulation box.
 */
INLINE static void gpart_to_mesh_CIC(const struct gpart* gp, double* rho,
                                     const int N, const double fac,
                                     const double dim[3]) {

  /* Box wrap the multipole's position */
  const double pos_x = box_wrap(gp->x[0], 0., dim[0]);
  const double pos_y = box_wrap(gp->x[1], 0., dim[1]);
  const double pos_z = box_wrap(gp->x[2], 0., dim[2]);

  /* Workout the CIC coefficients */
  int i = (int)(fac * pos_x);
  if (i >= N) i = N - 1;
  const double dx = fac * pos_x - i;
  const double tx = 1. - dx;

  int j = (int)(fac * pos_y);
  if (j >= N) j = N - 1;
  const double dy = fac * pos_y - j;
  const double ty = 1. - dy;

  int k = (int)(fac * pos_z);
  if (k >= N) k = N - 1;
  const double dz = fac * pos_z - k;
  const double tz = 1. - dz;

#ifdef SWIFT_DEBUG_CHECKS
  if (i < 0 || i >= N) error("Invalid gpart position in x");
  if (j < 0 || j >= N) error("Invalid gpart position in y");
  if (k < 0 || k >= N) error("Invalid gpart position in z");
#endif

  const double mass = gp->mass;

  /* CIC ! */
  CIC_set(rho, N, i, j, k, tx, ty, tz, dx, dy, dz, mass);
}

/**
 * @brief Assigns all the #gpart of a #cell to a density mesh using the CIC
 * method.
 *
 * @param c The #cell.
 * @param rho The density mesh.
 * @param N the size of the mesh along one axis.
 * @param fac The width of a mesh cell.
 * @param dim The dimensions of the simulation box.
 */
void cell_gpart_to_mesh_CIC(const struct cell* c, double* rho, const int N,
                            const double fac, const double dim[3]) {

  const int gcount = c->grav.count;
  const struct gpart* gparts = c->grav.parts;

  /* Assign all the gpart of that cell to the mesh */
  for (int i = 0; i < gcount; ++i)
    gpart_to_mesh_CIC(&gparts[i], rho, N, fac, dim);
}

/**
 * @brief Shared information about the mesh to be used by all the threads in the
 * pool.
 */
struct cic_mapper_data {
  const struct cell* cells;
  double* rho;
  int N;
  double fac;
  double dim[3];
};

/**
 * @brief Threadpool mapper function for the mesh CIC assignment of a cell.
 *
 * @param map_data A chunk of the list of local cells.
 * @param num The number of cells in the chunk.
 * @param extra The information about the mesh and cells.
 */
void cell_gpart_to_mesh_CIC_mapper(void* map_data, int num, void* extra) {

  /* Unpack the shared information */
  const struct cic_mapper_data* data = (struct cic_mapper_data*)extra;
  const struct cell* cells = data->cells;
  double* rho = data->rho;
  const int N = data->N;
  const double fac = data->fac;
  const double dim[3] = {data->dim[0], data->dim[1], data->dim[2]};

  /* Pointer to the chunk to be processed */
  int* local_cells = (int*)map_data;

  // MATTHIEU: This could in principle be improved by creating a local mesh
  //           with just the extent required for the cell. Assignment can
  //           then be done without atomics. That local mesh is then added
  //           atomically to the global one.

  /* Loop over the elements assigned to this thread */
  for (int i = 0; i < num; ++i) {

    /* Pointer to local cell */
    const struct cell* c = &cells[local_cells[i]];

    /* Assign this cell's content to the mesh */
    cell_gpart_to_mesh_CIC(c, rho, N, fac, dim);
  }
}

/**
 * @brief Computes the potential on a gpart from a given mesh using the CIC
 * method.
 *
 * Debugging routine.
 *
 * @param gp The #gpart.
 * @param pot The potential mesh.
 * @param N the size of the mesh along one axis.
 * @param fac width of a mesh cell.
 * @param dim The dimensions of the simulation box.
 */
void mesh_to_gparts_CIC(struct gpart* gp, const double* pot, const int N,
                        const double fac, const double dim[3]) {

  /* Box wrap the gpart's position */
  const double pos_x = box_wrap(gp->x[0], 0., dim[0]);
  const double pos_y = box_wrap(gp->x[1], 0., dim[1]);
  const double pos_z = box_wrap(gp->x[2], 0., dim[2]);

  int i = (int)(fac * pos_x);
  if (i >= N) i = N - 1;
  const double dx = fac * pos_x - i;
  const double tx = 1. - dx;

  int j = (int)(fac * pos_y);
  if (j >= N) j = N - 1;
  const double dy = fac * pos_y - j;
  const double ty = 1. - dy;

  int k = (int)(fac * pos_z);
  if (k >= N) k = N - 1;
  const double dz = fac * pos_z - k;
  const double tz = 1. - dz;

#ifdef SWIFT_DEBUG_CHECKS
  if (i < 0 || i >= N) error("Invalid gpart position in x");
  if (j < 0 || j >= N) error("Invalid gpart position in y");
  if (k < 0 || k >= N) error("Invalid gpart position in z");
#endif

#ifdef SWIFT_GRAVITY_FORCE_CHECKS
  if (gp->a_grav_PM[0] != 0. || gp->potential_PM != 0.)
    error("Particle with non-initalised stuff");
#endif

  /* First, copy the necessary part of the mesh for stencil operations */
  /* This includes box-wrapping in all 3 dimensions. */
  double phi[6][6][6];
  for (int iii = -2; iii <= 3; ++iii) {
    for (int jjj = -2; jjj <= 3; ++jjj) {
      for (int kkk = -2; kkk <= 3; ++kkk) {
        phi[iii + 2][jjj + 2][kkk + 2] =
            pot[row_major_id_periodic(i + iii, j + jjj, k + kkk, N)];
      }
    }
  }

  /* Some local accumulators */
  double p = 0.;
  double a[3] = {0.};

  /* Indices of (i,j,k) in the local copy of the mesh */
  const int ii = 2, jj = 2, kk = 2;

  /* Simple CIC for the potential itself */
  p += CIC_get(phi, ii, jj, kk, tx, ty, tz, dx, dy, dz);

  /* ---- */

  /* 5-point stencil along each axis for the accelerations */
  a[0] += (1. / 12.) * CIC_get(phi, ii + 2, jj, kk, tx, ty, tz, dx, dy, dz);
  a[0] -= (2. / 3.) * CIC_get(phi, ii + 1, jj, kk, tx, ty, tz, dx, dy, dz);
  a[0] += (2. / 3.) * CIC_get(phi, ii - 1, jj, kk, tx, ty, tz, dx, dy, dz);
  a[0] -= (1. / 12.) * CIC_get(phi, ii - 2, jj, kk, tx, ty, tz, dx, dy, dz);

  a[1] += (1. / 12.) * CIC_get(phi, ii, jj + 2, kk, tx, ty, tz, dx, dy, dz);
  a[1] -= (2. / 3.) * CIC_get(phi, ii, jj + 1, kk, tx, ty, tz, dx, dy, dz);
  a[1] += (2. / 3.) * CIC_get(phi, ii, jj - 1, kk, tx, ty, tz, dx, dy, dz);
  a[1] -= (1. / 12.) * CIC_get(phi, ii, jj - 2, kk, tx, ty, tz, dx, dy, dz);

  a[2] += (1. / 12.) * CIC_get(phi, ii, jj, kk + 2, tx, ty, tz, dx, dy, dz);
  a[2] -= (2. / 3.) * CIC_get(phi, ii, jj, kk + 1, tx, ty, tz, dx, dy, dz);
  a[2] += (2. / 3.) * CIC_get(phi, ii, jj, kk - 1, tx, ty, tz, dx, dy, dz);
  a[2] -= (1. / 12.) * CIC_get(phi, ii, jj, kk - 2, tx, ty, tz, dx, dy, dz);

  /* ---- */

  /* Store things back */
  accumulate_add_f(&gp->a_grav[0], fac * a[0]);
  accumulate_add_f(&gp->a_grav[1], fac * a[1]);
  accumulate_add_f(&gp->a_grav[2], fac * a[2]);
  gravity_add_comoving_potential(gp, p);
#ifdef SWIFT_GRAVITY_FORCE_CHECKS
  gp->potential_PM = p;
  gp->a_grav_PM[0] = fac * a[0];
  gp->a_grav_PM[1] = fac * a[1];
  gp->a_grav_PM[2] = fac * a[2];
#endif
}

/**
 * @brief Shared information about the Green function to be used by all the
 * threads in the pool.
 */
struct Green_function_data {

  int N;
  fftw_complex* frho;
  double green_fac;
  double a_smooth2;
  double k_fac;
  int slice_offset;
  int slice_width;
};

/**
 * @brief Mapper function for the application of the Green function.
 *
 * @param map_data The array of the density field Fourier transform.
 * @param num The number of elements to iterate on (along the x-axis).
 * @param extra The properties of the Green function.
 */
void mesh_apply_Green_function_mapper(void* map_data, const int num,
                                      void* extra) {

  struct Green_function_data* data = (struct Green_function_data*)extra;

  /* Unpack the array */
  fftw_complex* const frho = data->frho;
  const int N = data->N;
  const int N_half = N / 2;

  /* Unpack the Green function properties */
  const double green_fac = data->green_fac;
  const double a_smooth2 = data->a_smooth2;
  const double k_fac = data->k_fac;

  /* Find what slice of the full mesh is stored on this MPI rank */
  const int slice_offset = data->slice_offset;

  /* Range of x coordinates in the full mesh handled by this call */
  const int i_start = ((fftw_complex*)map_data - frho) + slice_offset;
  const int i_end = i_start + num;

  /* Loop over the x range corresponding to this thread */
  for (int i = i_start; i < i_end; ++i) {

    /* kx component of vector in Fourier space and 1/sinc(kx) */
    const int kx = (i > N_half ? i - N : i);
    const double kx_d = (double)kx;
    const double fx = k_fac * kx_d;
    const double sinc_kx_inv = (kx != 0) ? fx / sin(fx) : 1.;

    for (int j = 0; j < N; ++j) {

      /* ky component of vector in Fourier space and 1/sinc(ky) */
      const int ky = (j > N_half ? j - N : j);
      const double ky_d = (double)ky;
      const double fy = k_fac * ky_d;
      const double sinc_ky_inv = (ky != 0) ? fy / sin(fy) : 1.;

      for (int k = 0; k < N_half + 1; ++k) {

        /* kz component of vector in Fourier space and 1/sinc(kz) */
        const int kz = (k > N_half ? k - N : k);
        const double kz_d = (double)kz;
        const double fz = k_fac * kz_d;
        const double sinc_kz_inv = (kz != 0) ? fz / (sin(fz) + FLT_MIN) : 1.;

        /* Norm of vector in Fourier space */
        const double k2 = (kx_d * kx_d + ky_d * ky_d + kz_d * kz_d);

        /* Avoid FPEs... */
        if (k2 == 0.) continue;

        /* Green function */
        double W = 1.;
        fourier_kernel_long_grav_eval(k2 * a_smooth2, &W);
        const double green_cor = green_fac * W / (k2 + FLT_MIN);

        /* Deconvolution of CIC */
        const double CIC_cor = sinc_kx_inv * sinc_ky_inv * sinc_kz_inv;
        const double CIC_cor2 = CIC_cor * CIC_cor;
        const double CIC_cor4 = CIC_cor2 * CIC_cor2;

        /* Combined correction */
        const double total_cor = green_cor * CIC_cor4;

        /* Apply to the mesh */
        const int index = N * (N_half + 1) * (i - slice_offset) + (N_half + 1) * j + k;
        frho[index][0] *= total_cor;
        frho[index][1] *= total_cor;
      }
    }
  }
}

/**
 * @brief Apply the Green function in Fourier space to the density
 * array to get the potential.
 *
 * Also deconvolves the CIC kernel.
 *
 * @param tp The threadpool.
 * @param frho The NxNx(N/2) complex array of the Fourier transform of the
 * density field.
 * @param slice_offset The x coordinate of the start of the slice on this MPI rank
 * @param slice_width The width of the local slice on this MPI rank
 * @param N The dimension of the array.
 * @param r_s The Green function smoothing scale.
 * @param box_size The physical size of the simulation box.
 */
void mesh_apply_Green_function(struct threadpool* tp, fftw_complex* frho,
                               const int slice_offset, const int slice_width,
                               const int N, const double r_s,
                               const double box_size) {

  /* Some common factors */
  struct Green_function_data data;
  data.frho = frho;
  data.N = N;
  data.green_fac = -1. / (M_PI * box_size);
  data.a_smooth2 = 4. * M_PI * M_PI * r_s * r_s / (box_size * box_size);
  data.k_fac = M_PI / (double)N;
  data.slice_offset = slice_offset;
  data.slice_width = slice_width;

  /* Parallelize the Green function application using the threadpool
     to split the x-axis loop over the threads.
     The array is N x N x (N/2). We use the thread to each deal with
     a range [i_min, i_max[ x N x (N/2) */
  threadpool_map(tp, mesh_apply_Green_function_mapper, frho, slice_width,
                 sizeof(fftw_complex), threadpool_auto_chunk_size, &data);

  /* Correct singularity at (0,0,0), if it's in our local slice */
  if(slice_offset==0 && slice_width > 0) {
    frho[0][0] = 0.;
    frho[0][1] = 0.;
  }
}

#endif


/**
 * @brief Compute the potential, including periodic correction on the mesh.
 *
 * Interpolates the top-level multipoles on-to a mesh, move to Fourier space,
 * compute the potential including short-range correction and move back
 * to real space. We use CIC for the interpolation.
 *
 * Note that there is no multiplication by G_newton at this stage.
 *
 * The output from this version is a hashmap containing the potential
 * in mesh cells which will be needed on this MPI rank. This is stored
 * in mesh->potential_local. The FFTW MPI library is used to do the FFTs.
 *
 * @param mesh The #pm_mesh used to store the potential.
 * @param s The #space containing the particles.
 * @param tp The #threadpool object used for parallelisation.
 * @param verbose Are we talkative?
 */
void compute_potential_distributed(struct pm_mesh* mesh, const struct space* s,
                                   struct threadpool* tp, const int verbose) {

#if defined(WITH_MPI) && defined(HAVE_MPI_FFTW)

  const double r_s = mesh->r_s;
  const double box_size = s->dim[0];
  const double dim[3] = {s->dim[0], s->dim[1], s->dim[2]};

  if (r_s <= 0.) error("Invalid value of a_smooth");
  if (mesh->dim[0] != dim[0] || mesh->dim[1] != dim[1] ||
      mesh->dim[2] != dim[2])
    error("Domain size does not match the value stored in the space.");

  /* Some useful constants */
  const int N = mesh->N;
  const double cell_fac = N / box_size;

  MPI_Barrier(MPI_COMM_WORLD);
  ticks tic = getticks();

  /* Calculate contributions to density field on this MPI rank */
  hashmap_t rho_map;
  hashmap_init(&rho_map);
  mpi_mesh_accumulate_gparts_to_hashmap(tp, N, cell_fac, s, &rho_map);
  if(verbose)message("Accumulating mass to hashmap took %.3f %s.",
                     clocks_from_ticks(getticks() - tic), clocks_getunit());

  /* Ask FFTW what slice of the density field we need to store on this task.
     Note that fftw_mpi_local_size_3d works in terms of the size of the complex
     output. The last dimension of the real input is padded to 2*(N/2+1). */
  ptrdiff_t local_n0;
  ptrdiff_t local_0_start;
  ptrdiff_t nalloc =
    fftw_mpi_local_size_3d((ptrdiff_t)N, (ptrdiff_t)N, (ptrdiff_t) (N/2+1),
                           MPI_COMM_WORLD, &local_n0, &local_0_start);
  if(verbose)message("Local density field slice has thickness %d.", (int)local_n0);
  if(verbose)message("Hashmap size = %d, local cells = %d", (int)hashmap_size(&rho_map), (int) (local_n0*N*N));

  /* Allocate storage for mesh slices. nalloc is the number of *complex* values. */
  double* rho_slice = (double*)fftw_malloc(2 * nalloc * sizeof(double));
  for (int i = 0; i < 2*nalloc; i += 1) rho_slice[i] = 0.0;

  /* Allocate storage for the slices of the FFT of the density mesh */
  fftw_complex* frho_slice = (fftw_complex*) fftw_malloc(nalloc * sizeof(fftw_complex));

  tic = getticks();

  /* Construct density field slices from contributions stored in hashmaps */
  mpi_mesh_hashmaps_to_slices(N, (int)local_n0, &rho_map, rho_slice);
  if (verbose)
    message("Assembling mesh slices took %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());
  hashmap_free(&rho_map);

  tic = getticks();

  /* Carry out the MPI Fourier transform. We can save a bit of time
   * if we allow FFTW to transpose the first two dimensions of the output.
   *
   * Layout of the MPI FFTW input and output:
   *
   * Input mesh contains N*N*N reals, padded to N*N*(2*(N/2+1)).
   * Output Fourier transform is N*N*(N/2+1) complex values.
   *
   * The first two dimensions of the transform are transposed in
   * the output. Each MPI rank has slice of thickness local_n0
   * starting at local_0_start in the first dimension.
   */
  fftw_plan mpi_plan = fftw_mpi_plan_dft_r2c_3d(N, N, N, rho_slice, frho_slice, MPI_COMM_WORLD,
                                                FFTW_ESTIMATE | FFTW_MPI_TRANSPOSED_OUT | FFTW_DESTROY_INPUT);
  fftw_execute(mpi_plan);
  fftw_destroy_plan(mpi_plan);
  if (verbose)
    message("MPI forward Fourier transform took %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

  tic = getticks();

  /* Apply Green function to local slice of the MPI mesh */
  mesh_apply_Green_function(tp, frho_slice, local_0_start, local_n0, N, r_s, box_size);
  if (verbose)
    message("Applying Green function to MPI mesh took %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

  tic = getticks();

  /* Carry out the reverse MPI Fourier transform */
  fftw_plan mpi_inverse_plan = fftw_mpi_plan_dft_c2r_3d(N, N, N, frho_slice, rho_slice, MPI_COMM_WORLD,
                                                        FFTW_ESTIMATE | FFTW_MPI_TRANSPOSED_IN | FFTW_DESTROY_INPUT);
  fftw_execute(mpi_inverse_plan);
  fftw_destroy_plan(mpi_inverse_plan);  

  if (verbose)
    message("MPI reverse Fourier transform took %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

  /* Clear the potential hashmap */
  hashmap_free(mesh->potential_local);
  hashmap_init(mesh->potential_local);

  /* Fetch MPI mesh entries we need on this rank from other ranks */
  mpi_mesh_fetch_potential(N, cell_fac, s, local_0_start, local_n0,
                           rho_slice, mesh->potential_local);
  
  /* Discard per-task mesh slices */
  fftw_free(rho_slice);
  fftw_free(frho_slice);
#else
  error("No FFTW MPI library available. Cannot compute distributed mesh.");
#endif
}


/**
 * @brief Compute the potential, including periodic correction on the mesh.
 *
 * Interpolates the top-level multipoles on-to a mesh, move to Fourier space,
 * compute the potential including short-range correction and move back
 * to real space. We use CIC for the interpolation.
 *
 * Note that there is no multiplication by G_newton at this stage.
 *
 * This version stores the full N*N*N mesh on each MPI rank and uses the
 * non-MPI version of FFTW.
 *
 * @param mesh The #pm_mesh used to store the potential.
 * @param s The #space containing the particles.
 * @param tp The #threadpool object used for parallelisation.
 * @param verbose Are we talkative?
 */
void compute_potential_global(struct pm_mesh* mesh, const struct space* s,
                              struct threadpool* tp, const int verbose) {

#ifdef HAVE_FFTW

  const double r_s = mesh->r_s;
  const double box_size = s->dim[0];
  const double dim[3] = {s->dim[0], s->dim[1], s->dim[2]};
  const int* local_cells = s->local_cells_top;
  const int nr_local_cells = s->nr_local_cells;

  if (r_s <= 0.) error("Invalid value of a_smooth");
  if (mesh->dim[0] != dim[0] || mesh->dim[1] != dim[1] ||
      mesh->dim[2] != dim[2])
    error("Domain size does not match the value stored in the space.");

  /* Some useful constants */
  const int N = mesh->N;
  const int N_half = N / 2;
  const double cell_fac = N / box_size;

  /* Use the memory allocated for the potential to temporarily store rho */
  double* restrict rho = mesh->potential_global;
  if (rho == NULL) error("Error allocating memory for density mesh");
  bzero(rho, N * N * N * sizeof(double));

  /* Allocates some memory for the mesh in Fourier space */
  fftw_complex* restrict frho =
      (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * N * N * (N_half + 1));
  if (frho == NULL)
    error("Error allocating memory for transform of density mesh");
  memuse_log_allocation("fftw_frho", frho, 1,
                        sizeof(fftw_complex) * N * N * (N_half + 1));

  /* Prepare the FFT library */
  fftw_plan forward_plan = fftw_plan_dft_r2c_3d(
      N, N, N, rho, frho, FFTW_ESTIMATE | FFTW_DESTROY_INPUT);
  fftw_plan inverse_plan = fftw_plan_dft_c2r_3d(
      N, N, N, frho, rho, FFTW_ESTIMATE | FFTW_DESTROY_INPUT);

  ticks tic = getticks();

  /* Zero everything */
  bzero(rho, N * N * N * sizeof(double));

  /* Gather the mesh shared information to be used by the threads */
  struct cic_mapper_data data;
  data.cells = s->cells_top;
  data.rho = rho;
  data.N = N;
  data.fac = cell_fac;
  data.dim[0] = dim[0];
  data.dim[1] = dim[1];
  data.dim[2] = dim[2];

  /* Do a parallel CIC mesh assignment of the gparts but only using
     the local top-level cells */
  threadpool_map(tp, cell_gpart_to_mesh_CIC_mapper, (void*)local_cells,
                 nr_local_cells, sizeof(int), threadpool_auto_chunk_size,
                 (void*)&data);

  if (verbose)
    message("Gpart assignment took %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

#ifdef WITH_MPI

  MPI_Barrier(MPI_COMM_WORLD);
  tic = getticks();

  /* Merge everybody's share of the density mesh */
  MPI_Allreduce(MPI_IN_PLACE, rho, N * N * N, MPI_DOUBLE, MPI_SUM,
                MPI_COMM_WORLD);

  if (verbose)
    message("Mesh communication took %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());
#endif

  /* message("\n\n\n DENSITY"); */
  /* print_array(rho, N); */

  tic = getticks();

  /* Fourier transform to go to magic-land */
  fftw_execute(forward_plan);

  if (verbose)
    message("Forward Fourier transform took %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

  /* frho now contains the Fourier transform of the density field */
  /* frho contains NxNx(N/2+1) complex numbers */

  tic = getticks();

  /* Now de-convolve the CIC kernel and apply the Green function */
  mesh_apply_Green_function(tp, frho, 0, N, N, r_s, box_size);

  if (verbose)
    message("Applying Green function took %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());

  tic = getticks();

  /* Fourier transform to come back from magic-land */
  fftw_execute(inverse_plan);

  if (verbose)
    message("Backwards Fourier transform took %.3f %s.",
            clocks_from_ticks(getticks() - tic), clocks_getunit());
  
  /* rho now contains the potential */
  /* This array is now again NxNxN real numbers */

  /* Let's store it in the structure */
  mesh->potential_global = rho;

  /* message("\n\n\n POTENTIAL"); */
  /* print_array(mesh->potential_global, N); */

  /* Clean-up the mess */
  fftw_destroy_plan(forward_plan);
  fftw_destroy_plan(inverse_plan);
  memuse_log_allocation("fftw_frho", frho, 0, 0);
  fftw_free(frho);

#else
  error("No FFTW library found. Cannot compute periodic long-range forces.");
#endif
}


/**
 * @brief Compute the potential, including periodic correction on the mesh.
 *
 * Interpolates the top-level multipoles on-to a mesh, move to Fourier space,
 * compute the potential including short-range correction and move back
 * to real space. We use CIC for the interpolation.
 *
 * Note that there is no multiplication by G_newton at this stage.
 *
 * @param mesh The #pm_mesh used to store the potential.
 * @param s The #space containing the particles.
 * @param tp The #threadpool object used for parallelisation.
 * @param verbose Are we talkative?
 */
void pm_mesh_compute_potential(struct pm_mesh* mesh, const struct space* s,
                               struct threadpool* tp, const int verbose) {
  if(mesh->distributed_mesh) {
    compute_potential_distributed(mesh, s, tp, verbose);
  } else {
    compute_potential_global(mesh, s, tp, verbose);    
  }
}


/**
 * @brief Interpolate the forces and potential from the mesh to the #gpart.
 *
 * We use CIC interpolation. The resulting accelerations and potential must
 * be multiplied by G_newton.
 *
 * @param mesh The #pm_mesh (containing the potential) to interpolate from.
 * @param e The #engine (to check active status).
 * @param gparts The #gpart to interpolate to.
 * @param gcount The number of #gpart.
 */
void interpolate_forces(const struct pm_mesh* mesh,
                        const struct engine* e, struct gpart* gparts,
                        int gcount) {
#ifdef HAVE_FFTW

  const int N = mesh->N;
  const double cell_fac = mesh->cell_fac;
  const double* potential = mesh->potential_global;
  const double dim[3] = {e->s->dim[0], e->s->dim[1], e->s->dim[2]};

  /* Get the potential from the mesh to the active gparts using CIC */
  for (int i = 0; i < gcount; ++i) {
    struct gpart* gp = &gparts[i];

    if (gpart_is_active(gp, e)) {

#ifdef SWIFT_DEBUG_CHECKS
      /* Check that particles have been drifted to the current time */
      if (gp->ti_drift != e->ti_current)
        error("gpart not drifted to current time");

      /* Check that the particle was initialised */
      if (gp->initialised == 0)
        error("Adding forces to an un-initialised gpart.");
#endif

      mesh_to_gparts_CIC(gp, potential, N, cell_fac, dim);
    }
  }
#else
  error("No FFTW library found. Cannot compute periodic long-range forces.");
#endif
}


/**
 * @brief Interpolate the forces and potential from the mesh to the #gpart.
 *
 * We use CIC interpolation. The resulting accelerations and potential must
 * be multiplied by G_newton.
 *
 * @param mesh The #pm_mesh (containing the potential) to interpolate from.
 * @param e The #engine (to check active status).
 * @param gparts The #gpart to interpolate to.
 * @param gcount The number of #gpart.
 */
void pm_mesh_interpolate_forces(const struct pm_mesh* mesh,
                                const struct engine* e,
                                const struct cell *cell) {
  if(mesh->distributed_mesh) {
    mpi_mesh_interpolate_forces(mesh->potential_local, mesh->N, mesh->cell_fac, e, cell);
  } else {
    interpolate_forces(mesh, e, cell->grav.parts, cell->grav.count);
  }
}

/**
 * @bried Allocates the potential grid to be ready for an FFT calculation
 *
 * @param mesh The #pm_mesh structure.
 */
void pm_mesh_allocate(struct pm_mesh* mesh) {

#ifdef HAVE_FFTW

  if(mesh->distributed_mesh) {
    if (mesh->potential_local != NULL) error("Mesh already allocated!");
    mesh->potential_local = malloc(sizeof(hashmap_t));
    hashmap_init(mesh->potential_local);
  } else {
    const int N = mesh->N;

    /* Allocate the memory for the combined density and potential array */
    mesh->potential_global = (double*)fftw_malloc(sizeof(double) * N * N * N);
    if (mesh->potential_global == NULL)
      error("Error allocating memory for the long-range gravity mesh.");
    memuse_log_allocation("fftw_mesh.potential", mesh->potential_global, 1,
                          sizeof(double) * N * N * N);
  }

#else
  error("No FFTW library found. Cannot compute periodic long-range forces.");
#endif
}

/**
 * @brief Frees the potential grid.
 *
 * @param mesh The #pm_mesh structure.
 */
void pm_mesh_free(struct pm_mesh* mesh) {
  
#ifdef HAVE_FFTW
  if(mesh->distributed_mesh && mesh->potential_local) {
    hashmap_free(mesh->potential_local);
    free(mesh->potential_local);
    mesh->potential_local = NULL;
  }
  if(!mesh->distributed_mesh && mesh->potential_global) {
    memuse_log_allocation("fftw_mesh.potential", mesh->potential_global, 0, 0);
    free(mesh->potential_global);
    mesh->potential_global = NULL;
  }
#else
  error("No FFTW library found. Cannot compute periodic long-range forces.");
#endif
}

/**
 * @brief Initialises FFTW for MPI and thread usage as necessary
 *
 * @param N The size of the FFT mesh
 */
void initialise_fftw(int N, int nr_threads) {

#ifdef HAVE_THREADED_FFTW
  /* Initialise the thread-parallel FFTW version */
  if (N >= 64) fftw_init_threads();
#endif
#if defined(WITH_MPI) && defined(HAVE_MPI_FFTW)
  /* Initialize FFTW MPI support - must be called after fftw_init_threads() */
  fftw_mpi_init();
#endif
#ifdef HAVE_THREADED_FFTW
  /* Set  number of threads to use */
  if (N >= 64) fftw_plan_with_nthreads(nr_threads);
#endif

}


/**
 * @brief Initialises the mesh used for the long-range periodic forces
 *
 * @param mesh The #pm_mesh to initialise.
 * @param props The propoerties of the gravity scheme.
 * @param dim The (comoving) side-lengths of the simulation volume.
 * @param nr_threads The number of threads on this MPI rank.
 */
void pm_mesh_init(struct pm_mesh* mesh, const struct gravity_props* props,
                  double dim[3], int nr_threads) {

#ifdef HAVE_FFTW

  if (dim[0] != dim[1] || dim[0] != dim[2])
    error("Doing mesh-gravity on a non-cubic domain");

  const int N = props->mesh_size;
  const double box_size = dim[0];

  mesh->nr_threads = nr_threads;
  mesh->periodic = 1;
  mesh->N = N;
  mesh->distributed_mesh = props->distributed_mesh;
  mesh->dim[0] = dim[0];
  mesh->dim[1] = dim[1];
  mesh->dim[2] = dim[2];
  mesh->cell_fac = N / box_size;
  mesh->r_s = props->a_smooth * box_size / N;
  mesh->r_s_inv = 1. / mesh->r_s;
  mesh->r_cut_max = mesh->r_s * props->r_cut_max_ratio;
  mesh->r_cut_min = mesh->r_s * props->r_cut_min_ratio;
  mesh->potential_local = NULL;
  mesh->potential_global = NULL;

  if(!mesh->distributed_mesh && mesh->N > 1290)
    error("Mesh too big. The number of cells is larger than 2^31. "
          "Use a mesh side-length <= 1290.");

  if (2. * mesh->r_cut_max > box_size)
    error("Mesh too small or r_cut_max too big for this box size");

  initialise_fftw(N, mesh->nr_threads);
  pm_mesh_allocate(mesh);

#else
  error("No FFTW library found. Cannot compute periodic long-range forces.");
#endif
}

/**
 * @brief Initialises the mesh for the case where we don't do mesh gravity
 * calculations
 *
 * Crucially this set the 'periodic' propoerty to 0 and all the relevant values
 * to a
 * state where all calculations will default to pure non-periodic Newtonian.
 *
 * @param mesh The #pm_mesh to initialise.
 * @param dim The (comoving) side-lengths of the simulation volume.
 */
void pm_mesh_init_no_mesh(struct pm_mesh* mesh, double dim[3]) {

  bzero(mesh, sizeof(struct pm_mesh));

  /* Fill in non-zero properties */
  mesh->dim[0] = dim[0];
  mesh->dim[1] = dim[1];
  mesh->dim[2] = dim[2];
  mesh->r_s = FLT_MAX;
  mesh->r_cut_min = FLT_MAX;
  mesh->r_cut_max = FLT_MAX;
}

/**
 * @brief Frees the memory allocated for the long-range mesh.
 */
void pm_mesh_clean(struct pm_mesh* mesh) {

#ifdef HAVE_THREADED_FFTW
  fftw_cleanup_threads();
#endif
#if defined(WITH_MPI) && defined(HAVE_MPI_FFTW)
  fftw_mpi_cleanup();
#endif

  pm_mesh_free(mesh);
}

/**
 * @brief Write a #pm_mesh struct to the given FILE as a stream of bytes.
 *
 * @param mesh the struct
 * @param stream the file stream
 */
void pm_mesh_struct_dump(const struct pm_mesh* mesh, FILE* stream) {
  restart_write_blocks((void*)mesh, sizeof(struct pm_mesh), 1, stream,
                       "gravity", "gravity props");
}

/**
 * @brief Restore a #pm_mesh struct from the given FILE as a stream of
 * bytes.
 *
 * @param mesh the struct
 * @param stream the file stream
 */
void pm_mesh_struct_restore(struct pm_mesh* mesh, FILE* stream) {

  restart_read_blocks((void*)mesh, sizeof(struct pm_mesh), 1, stream, NULL,
                      "gravity props");

  if (mesh->periodic) {

#ifdef HAVE_FFTW
    const int N = mesh->N;
    initialise_fftw(N, mesh->nr_threads);

    if(mesh->distributed_mesh) {
      /* Allocate hashmap to store local part of density and potential array */
      mesh->potential_local = malloc(sizeof(hashmap_t));
      hashmap_init(mesh->potential_local);
    } else {
      /* Allocate the memory for the combined density and potential array */
      mesh->potential_global = (double*)fftw_malloc(sizeof(double) * N * N * N);
      if (mesh->potential_global == NULL)
        error("Error allocating memory for the long-range gravity mesh.");
      memuse_log_allocation("fftw_mesh.potential", mesh->potential_global, 1,
        sizeof(double) * N * N * N);
    }
#else
    error("No FFTW library found. Cannot compute periodic long-range forces.");
#endif
  }
}
