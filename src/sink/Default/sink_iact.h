/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2020 Loic Hausammann (loic.hausammann@epfl.ch)
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
#ifndef SWIFT_DEFAULT_SINKS_IACT_H
#define SWIFT_DEFAULT_SINKS_IACT_H



/**
 * @brief do sink computation after the runner_iact_density (symmetric
 * version)
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param hi Comoving smoothing-length of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param pi First particle.
 * @param pj Second particle.
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void runner_iact_sink(
    const float r2, const float dx[3], const float hi, const float hj,
    struct part *restrict pi, struct part *restrict pj, const float a,
    const float H, const struct sink_props *sink_props) {}


/**
 * @brief do sink computation after the runner_iact_density (non symmetric
 * version)
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param hi Comoving smoothing-length of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param pi First particle.
 * @param pj Second particle (not updated).
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void runner_iact_nonsym_sink(
    const float r2, const float dx[3], const float hi, const float hj,
    struct part *restrict pi, const struct part *restrict pj, const float a,
    const float H, const struct sink_props *sink_props) {}



/**
 * @brief Compute sink swallow interaction between two particles (non-symmetric).
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param ri Comoving cut off radius of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param si First sink particle.
 * @param pj Second particle.
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void
runner_iact_nonsym_sinks_swallow(const float r2, const float *dx,
                                           const float hi, const float hj,
                                           struct sink *restrict si,
                                           const struct part *restrict pj,
                                           const float a, const float H) {

#ifdef DEBUG_INTERACTIONS_SINKS
  /* Update ngb counters */
  if (si->num_ngb_formation < MAX_NUM_OF_NEIGHBOURS_SINKS)
    si->ids_ngbs_formation[si->num_ngb_formation] = pj->id;

  /* Update ngb counters */
  ++si->num_ngb_formation;
#endif
}

/**
 * @brief Swallowing interaction between two sink particles (non-symmetric).
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param ri Comoving cut off radius of particle i.
 * @param rj Comoving cut off radius of particle j.
 * @param si First sink particle.
 * @param sj Second sink particle.
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 *
 * @param Which particle should be removed?
 * Possible value: (sink_do_sink_swallow_remove_none/first/second)
 */
__attribute__((always_inline)) INLINE static enum sink_do_sink_swallow_remove
runner_iact_sym_sinks_do_sink_swallow(const float r2, const float *dx, const float hi,
                             const float hj, struct sink *restrict si,
                             struct sink *restrict sj, const float a,
                             const float H) {

#ifdef DEBUG_INTERACTIONS_SINKS
  /* Update ngb counters */
  if (si->num_ngb_merger < MAX_NUM_OF_NEIGHBOURS_SINKS)
    si->ids_ngbs_merger[si->num_ngb_merger] = sj->id;

  /* Update ngb counters */
  ++si->num_ngb_merger;
#endif

  return sink_do_sink_swallow_remove_none;
}

/**
 * @brief Accretion interaction between two particles (non-symmetric).
 *
 * @param r2 Comoving square distance between the two particles.
 * @param dx Comoving vector separating both particles (pi - pj).
 * @param ri Comoving cut off radius of particle i.
 * @param hj Comoving smoothing-length of particle j.
 * @param si First sink particle.
 * @param pj Second particle.
 * @param a Current scale factor.
 * @param H Current Hubble parameter.
 */
__attribute__((always_inline)) INLINE static void
runner_iact_nonsym_sinks_accretion(const float r2, const float *dx,
                                   const float hi, const float hj,
                                   struct sink *restrict si,
                                   const struct part *restrict pj,
                                   const float a, const float H) {

#ifdef DEBUG_INTERACTIONS_SINKS
  /* Update ngb counters */
  if (si->num_ngb_accretion < MAX_NUM_OF_NEIGHBOURS_SINKS)
    si->ids_ngbs_accretion[si->num_ngb_accretion] = pj->id;

  /* Update ngb counters */
  ++si->num_ngb_accretion;
#endif
}

#endif
