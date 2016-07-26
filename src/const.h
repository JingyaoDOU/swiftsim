/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2012 Pedro Gonnet (ptcedro.gonnet@durham.ac.uk)
 *                    Matthieu Schaller (matthieu.schaller@durham.ac.uk)
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
#ifndef SWIFT_CONST_H
#define SWIFT_CONST_H

/* some math constants */
#define M_PI 3.14159265358979323846

/* SPH Viscosity constants. */
#define const_viscosity_alpha 0.8f
#define const_viscosity_alpha_min \
  0.1f /* Values taken from (Price,2004), not used in legacy gadget mode */
#define const_viscosity_alpha_max \
  2.0f /* Values taken from (Price,2004), not used in legacy gadget mode */
#define const_viscosity_length \
  0.1f /* Values taken from (Price,2004), not used in legacy gadget mode */

/* SPH Thermal conductivity constants. */
#define const_conductivity_alpha \
  1.f /* Value taken from (Price,2008), not used in legacy gadget mode */

/* Time integration constants. */
#define const_max_u_change 0.1f

/* Gravity stuff. */
#define const_theta_max 0.57735f
#define const_G 6.672e-8f     /* Gravitational constant. */
#define const_epsilon 0.0014f /* Gravity blending distance. */

/* Hydrodynamical adiabatic index. */
#define HYDRO_GAMMA_5_3
//#define HYDRO_GAMMA_4_3
//#define HYDRO_GAMMA_2_1

/* Kernel function to use */
#define CUBIC_SPLINE_KERNEL
//#define QUARTIC_SPLINE_KERNEL
//#define QUINTIC_SPLINE_KERNEL
//#define WENDLAND_C2_KERNEL
//#define WENDLAND_C4_KERNEL
//#define WENDLAND_C6_KERNEL

/* SPH variant to use */
//#define MINIMAL_SPH
#define GADGET2_SPH
//#define DEFAULT_SPH

/* Gravity properties */
/* #define EXTERNAL_POTENTIAL_POINTMASS */
//#define EXTERNAL_POTENTIAL_ISOTHERMALPOTENTIAL
#define EXTERNAL_POTENTIAL_DISK_PATCH

/* Are we debugging ? */
//#define SWIFT_DEBUG_CHECKS

#endif /* SWIFT_CONST_H */
