/*******************************************************************************
 * This file is part of SWIFT.
 * Coypright (c) 2015 Matthieu Schaller (matthieu.schaller@durham.ac.uk)
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
#ifndef SWIFT_HYDRO_H
#define SWIFT_HYDRO_H

#include "const.h"
#include "hydro_properties.h"
#include "kernel_hydro.h"
#include "part.h"

/* Import the right functions */
#if defined(MINIMAL_SPH)
#include "./hydro/Minimal/hydro.h"
#include "./hydro/Minimal/hydro_iact.h"
#define SPH_IMPLEMENTATION "Minimal version of SPH (e.g. Price 2010)"
#elif defined(GADGET2_SPH)
#include "./hydro/Gadget2/hydro.h"
#include "./hydro/Gadget2/hydro_iact.h"
#define SPH_IMPLEMENTATION "Gadget-2 version of SPH (Springel 2005)"
#elif defined(DEFAULT_SPH)
#include "./hydro/Default/hydro.h"
#include "./hydro/Default/hydro_iact.h"
#define SPH_IMPLEMENTATION "Default version of SPH"
#else
#error "Invalid choice of SPH variant"
#endif

#endif /* SWIFT_HYDRO_H */
