/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2016 Matthieu Schaller (matthieu.schaller@durham.ac.uk)
 *               2018   Jacob Kegerreis (jacob.kegerreis@durham.ac.uk).
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
#ifndef SWIFT_PLANETARY_HYDRO_PART_H
#define SWIFT_PLANETARY_HYDRO_PART_H

/**
 * @file Planetary/hydro_part.h
 * @brief Minimal conservative implementation of SPH (Particle definition)
 *
 * The thermal variable is the internal energy (u). Simple constant
 * viscosity term with the Balsara (1995) switch (optional).
 * No thermal conduction term is implemented.
 *
 * This corresponds to equations (43), (44), (45), (101), (103)  and (104) with
 * \f$\beta=3\f$ and \f$\alpha_u=0\f$ of Price, D., Journal of Computational
 * Physics, 2012, Volume 231, Issue 3, pp. 759-794.
 */

#include "black_holes_struct.h"
#include "chemistry_struct.h"
#include "cooling_struct.h"
#include "equation_of_state.h"  // For enum material_id
#include "feedback_struct.h"
#include "particle_splitting_struct.h"
#include "rt_struct.h"
#include "star_formation_struct.h"
#include "timestep_limiter_struct.h"
#include "tracers_struct.h"

/**
 * @brief Particle fields not needed during the SPH loops over neighbours.
 *
 * This structure contains the particle fields that are not used in the
 * density or force loops. Quantities should be used in the kick, drift and
 * potentially ghost tasks only.
 */
struct xpart {

  /*! Offset between current position and position at last tree rebuild. */
  float x_diff[3];

  /*! Offset between the current position and position at the last sort. */
  float x_diff_sort[3];

  /*! Velocity at the last full step. */
  float v_full[3];

  /*! Gravitational acceleration at the end of the last step */
  float a_grav[3];

  /*! Internal energy at the last full step. */
  float u_full;

  /*! Additional data used to record particle splits */
  struct particle_splitting_data split_data;

  /*! Additional data used to record cooling information */
  struct cooling_xpart_data cooling_data;

  /*! Additional data used by the tracers */
  struct tracers_xpart_data tracers_data;

  /* Additional data used by the star formation */
  struct star_formation_xpart_data sf_data;

  /* Additional data used by the feedback */
  struct feedback_part_data feedback_data;

} SWIFT_STRUCT_ALIGN;

/**
 * @brief Particle fields for the SPH particles
 *
 * The density and force substructures are used to contain variables only used
 * within the density and force loops over neighbours. All more permanent
 * variables should be declared in the main part of the part structure,
 */
struct part {

  /*! Particle unique ID. */
  long long id;

  /*! Pointer to corresponding gravity part. */
  struct gpart* gpart;

  /*! Particle position. */
  double x[3];

  /*! Particle predicted velocity. */
  float v[3];

  /*! Particle acceleration. */
  float a_hydro[3];

  /*! Particle mass. */
  float mass;

  /*! Particle smoothing length. */
  float h;

  /*! Particle internal energy. */
  float u;

  /*! Time derivative of the internal energy. */
  float u_dt;

  /*! Particle density. */
  float rho;

  /* Correction factors for kernel gradients. Numerator */
  float weighted_wcount;

  /* Correction factors for kernel gradients. Denominator */
  float weighted_neighbour_wcount;

  /* Correction factors for kernel gradients. f = weighted_wcount/(rho*weighted_neighbour_wcount) */
  float f_gdf;
    
  /* Pressure */
  float P;
    
  /* Temperature */
  float T;
    
  /* flag 1 if h=h_max 0 if not */
  int is_h_max;

  /* Store density/force specific stuff. */
  union {

    /**
     * @brief Structure for the variables only used in the density loop over
     * neighbours.
     *
     * Quantities in this sub-structure should only be accessed in the density
     * loop over neighbours and the ghost task.
     */
    struct {

      /*! Neighbour number count. */
      float wcount;

      /*! Derivative of the neighbour number with respect to h. */
      float wcount_dh;

      /*! Derivative of density with respect to h */
      float rho_dh;

      /*! Velocity divergence. */
      float div_v;

      /*! Velocity curl. */
      float rot_v[3];

    } density;

    /**
     * @brief Structure for the variables only used in the force loop over
     * neighbours.
     *
     * Quantities in this sub-structure should only be accessed in the force
     * loop over neighbours and the ghost, drift and kick tasks.
     */
    struct {

      /*! "Grad h" term */
      float f;

      /*! Particle pressure. */
      float pressure;

      /*! Particle soundspeed. */
      float soundspeed;

      /*! Particle signal velocity */
      float v_sig;

      /*! Time derivative of smoothing length  */
      float h_dt;

      /*! Balsara switch */
      float balsara;
        
    } force;
  };

  /*! Chemistry information */
  struct chemistry_part_data chemistry_data;

  /*! Cooling information */
  struct cooling_part_data cooling_data;

  /*! Black holes information (e.g. swallowing ID) */
  struct black_holes_part_data black_holes_data;

  /*! Material identifier flag */
  enum eos_planetary_material_id mat_id;

  /*! Additional Radiative Transfer Data */
  struct rt_part_data rt_data;

  /*! Time-step length */
  timebin_t time_bin;
    
  /*! Tree-depth at which size / 2 <= h * gamma < size */
  char depth_h;

  /*! Time-step limiter information */
  struct timestep_limiter_data limiter_data;

#ifdef SWIFT_DEBUG_CHECKS

  /* Time of the last drift */
  integertime_t ti_drift;

  /* Time of the last kick */
  integertime_t ti_kick;

#endif

#ifdef SWIFT_HYDRO_DENSITY_CHECKS

  /* Integer number of neighbours in the density loop */
  int N_density;

  /* Exact integer number of neighbours in the density loop */
  int N_density_exact;

  /* Integer number of neighbours in the gradient loop */
  int N_gradient;

  /* Exact integer number of neighbours in the gradient loop */
  int N_gradient_exact;

  /* Integer number of neighbours in the force loop */
  int N_force;

  /* Exact integer number of neighbours in the force loop */
  int N_force_exact;

  /*! Exact value of the density field obtained via brute-force loop */
  float rho_exact;

  /*! Weighted numer of neighbours in the density loop */
  float n_density;

  /*! Exact value of the weighted numer of neighbours in the density loop */
  float n_density_exact;

  /*! Weighted numer of neighbours in the gradient loop */
  float n_gradient;

  /*! Exact value of the weighted numer of neighbours in the gradient loop */
  float n_gradient_exact;

  /*! Weighted numer of neighbours in the force loop */
  float n_force;

  /*! Exact value of the weighted numer of neighbours in the force loop */
  float n_force_exact;

  /*! Has this particle interacted with any unhibited neighbour? */
  char inhibited_exact;

  /*! Has this particle been woken up by the limiter? */
  char limited_part;
#endif

#ifdef PLANETARY_FIXED_ENTROPY
  /* Fixed specific entropy */
  float s_fixed;
#endif

#ifdef PLANETARY_IMBALANCE
  /*! Sum of r_ij (used to compute imbalance statistic)*/
  float sum_rij[3];

  /*! Imbalance statistic*/
  float I;

  /*! Weighted mean pressure*/
  float sum_wij_exp_P;

  /*! Weighted mean temperature*/
  float sum_wij_exp_T;

  /*! Sum of W_ij * exp(-Ij) for rho_new */
  float sum_wij_exp;

  /* sum w_ij*/
  float sum_wij;
    
  float last_corrected_rho;
#endif
    
#ifdef PLANETARY_SMOOTHING_CORRECTION
  
  /* Derivative of density w.r.t. smoothing length */  
  float drho_dh;
    
  float smoothing_error;
    
  float P_tilde_numerator;
    
  float P_tilde_denominator;
    
  float S_numerator;
    
  float S_denominator;
    
  float I; 
    
  float max_ngb_sph_rho;
    
  float min_ngb_sph_rho;
    
  float last_corrected_rho;
    
  float sum_f_within_H;
    
  float sum_s_f_within_H;
    
  float last_S_tilde;
  
  float sum_r_w_V[3];
    
  float is_vac_boundary;  
  
#endif
    
#if defined PLANETARY_MATRIX_INVERSION || defined PLANETARY_QUAD_VISC
  /*! Particle C matrix. */
  float C[3][3], Cinv[3][3];

#endif

#ifdef PLANETARY_QUAD_VISC
  /*! Particle D matrix. */
  float Dinv[3][3];
    
  /*! Particle E matrix. i.e. second part of eq 19 in Rosswog 2020*/
  float E[3][3]; 
    
  /*! Particle auxiliary gradient*/
  float dv_aux[3][3];
    
  /*! Particle gradients from eq 18 (without C multiplied)*/
  float dv[3][3];
  float ddv[3][3][3];

  /*! Particle gradients from eq 18 (with C multiplied)*/
  float C_dv[3][3];
  float C_ddv[3][3][3];
    
  /*! Number of particles in grad loop*/
  float N_grad;
#endif


} SWIFT_STRUCT_ALIGN;

#endif /* SWIFT_PLANETARY_HYDRO_PART_H */