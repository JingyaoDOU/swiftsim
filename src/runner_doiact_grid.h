//
// Created by yuyttenh on 23/03/22.
//

#ifndef SWIFTSIM_RUNNER_DOIACT_GRID_H
#define SWIFTSIM_RUNNER_DOIACT_GRID_H

/* Local headers. */
#include "active.h"
#include "cell.h"
#include "shadowswift/delaunay.h"
#include "space_getsid.h"
#include "timers.h"

__attribute__((always_inline)) INLINE static void
runner_dopair_grid_construction(struct runner *restrict r,
                                struct cell *restrict ci,
                                struct cell *restrict cj) {
  const struct engine *restrict e = r->e;

#ifdef SWIFT_DEBUG_CHECKS
  assert(ci->hydro.count != 0 && cj->hydro.count != 0);
#endif

  /* Is the cell active and local? */
  assert((cell_is_active_hydro(ci, e) && ci->nodeID == e->nodeID));

  /* Check that cells are drifted. */
  if (!cell_are_part_drifted(ci, e) || !cell_are_part_drifted(cj, e))
    error("Interacting undrifted cells.");

  if (ci == cj) error("Interacting cell with itself!");

  /* Get the sort ID. */
  double shift[3] = {0.0, 0.0, 0.0};
  struct cell *ci_temp = ci;
  struct cell *cj_temp = cj;
  int sid = space_getsid(e->s, &ci_temp, &cj_temp, shift);

  /* Have the cells been sorted? */
  if (!(ci->hydro.sorted & (1 << sid)) ||
      ci->hydro.dx_max_sort_old > space_maxreldx * ci->dmin)
    error("Interacting unsorted cells.");
  if (!(cj->hydro.sorted & (1 << sid)) ||
      cj->hydro.dx_max_sort_old > space_maxreldx * cj->dmin)
    error("Interacting unsorted cells.");

  /* Delaunay already allocated? */
  if (ci->grid.delaunay == NULL) {
    ci->grid.delaunay = delaunay_malloc(ci->loc, ci->width, ci->hydro.count);
  }

  /* We are good to go!*/

  /* Get the cutoff shift. */
  double rshift = 0.0;
  for (int k = 0; k < 3; k++) rshift += shift[k] * runner_shift[sid][k];

  /* Pick-out the sorted lists. */
  const struct sort_entry *restrict sort_i = cell_get_hydro_sorts(ci, sid);
  const struct sort_entry *restrict sort_j = cell_get_hydro_sorts(cj, sid);

  /* Correct sid and shift if the cells have been flipped */
  const int flipped = ci != ci_temp;
  if (flipped) {
    sid = 26 - sid;
  }

  /* Get some other useful values. */
  const int count_i = ci->hydro.count;
  const int count_j = cj->hydro.count;
  struct part *restrict parts_i = ci->hydro.parts;
  struct part *restrict parts_j = cj->hydro.parts;
  const double hi_max = ci->hydro.h_max;
  const float dx_max = (ci->hydro.dx_max_sort + cj->hydro.dx_max_sort);

  /* Mark cell face as inside of simulation volume */
  ci->grid.delaunay->sid_is_inside_face[sid] |= 1;

  if (flipped) {
    /* ci on the right */

    const double di_min = sort_i[0].d;

    /* Loop over the parts in cj (on the left) */
    for (int pjd = count_j - 1;
         pjd >= 0 && sort_j[pjd].d + hi_max + dx_max > di_min; pjd--) {

      /* Recover pj */
      int pj_idx = sort_j[pjd].i;
      struct part *pj = &parts_j[pj_idx];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the left) */
      const double pjx = pj->x[0] - shift[0];
      const double pjy = pj->x[1] - shift[1];
      const double pjz = pj->x[2] - shift[2];

      /* Loop over the parts in ci (on the right) */
      for (int pid = 0;
           pid < count_i && sort_i[pid].d - hi_max - dx_max < sort_j[pjd].d;
           pid++) {

        /* Get a hold of pi. */
        struct part *restrict pi = &parts_i[sort_i[pid].i];

        /* Skip inactive particles */
        if (!part_is_active(pi, e)) {
          /* TODO what should we do here? */
          continue;
        }

        /* Early abort? */
        const float hi = pi->r;
        if (sort_i[pid].d - hi - dx_max >= sort_j[pjd].d) continue;

        /* Compute the pairwise distance. */
        double dx[3] = {pi->x[0] - pjx, pi->x[1] - pjy, pi->x[2] - pjz};
        const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

        /* Hit or miss? */
        if (r2 < hi * hi) {
          delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid,
                                  pj_idx);
        }
      } /* loop over the parts in ci. */
    }   /* loop over the parts in cj. */
  } else {
    /* ci on the left */

    const double di_max = sort_i[count_i - 1].d;

    /* Loop over the parts in cj (on the right) */
    for (int pjd = 0; pjd < count_j && sort_j[pjd].d - hi_max - dx_max < di_max;
         pjd++) {

      /* Recover pj */
      int pj_idx = sort_j[pjd].i;
      struct part *pj = &parts_j[sort_j[pjd].i];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the right) */
      const double pjx = pj->x[0] + shift[0];
      const double pjy = pj->x[1] + shift[1];
      const double pjz = pj->x[2] + shift[2];

      /* Loop over the parts in ci (on the left) */
      for (int pid = count_i - 1;
           pid >= 0 && sort_i[pid].d + hi_max + dx_max > sort_j[pjd].d; pid--) {

        /* Get a hold of pi. */
        struct part *restrict pi = &parts_i[sort_i[pid].i];

        /* Skip inactive particles */
        if (!part_is_active(pi, e)) {
          /* TODO what should we do here? */
          continue;
        }

        /* Early abort? */
        const float hi = pi->r;
        if (sort_i[pid].d + hi + dx_max <= sort_j[pjd].d) continue;

        /* Compute the pairwise distance. */
        double dx[3] = {pi->x[0] - pjx, pi->x[1] - pjy, pi->x[2] - pjz};
        const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

        /* Hit or miss? */
        if (r2 < hi * hi) {
          delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid,
                                  pj_idx);
        }
      } /* loop over the parts in ci. */
    }   /* loop over the parts in cj. */
  }     /* Flipped? */
}

__attribute__((always_inline)) INLINE static void
runner_doself_grid_construction(struct runner *restrict r,
                                struct cell *restrict c) {
  const struct engine *restrict e = r->e;

  /* Anything to do here? */
  if (c->hydro.count == 0) return;

  /* Is the cell active and local? */
  assert((cell_is_active_hydro(c, e) && c->nodeID == e->nodeID));

  /* Check that cells are drifted. */
  if (!cell_are_part_drifted(c, e)) error("Interacting undrifted cell.");

  /* Delaunay already allocated? */
  if (c->grid.delaunay == NULL) {
    c->grid.delaunay = delaunay_malloc(c->loc, c->width, c->hydro.count);
  }

  /* We are good to go!*/

  const int count = c->hydro.count;
  struct part *restrict parts = c->hydro.parts;

#ifdef SHADOWFAX_HILBERT_ORDERING
  /* Update hilbert keys + sort */
  cell_update_hilbert_keys(c);
  for (int i = 0; i < count; i++) {
    c->hydro.hilbert_r_sort[i] = i;
  }
  qsort_r(c->hydro.hilbert_r_sort, count, sizeof(int), sort_h_comp,
          c->hydro.hilbert_keys);
#endif

  /* Loop over the parts in c. */
  for (int i = 0; i < count; i++) {
#ifdef SHADOWFAX_HILBERT_ORDERING
    int idx = c->hydro.hilbert_r_sort[i];
#else
    int idx = i;
#endif
    /* Get a pointer to the idx-th particle. */
    struct part *restrict p = &parts[idx];
    /* TODO skip inactive particles */
    delaunay_add_local_vertex(c->grid.delaunay, idx, p->x[0], p->x[1], p->x[2]);
  }
}

__attribute__((always_inline)) INLINE static void
runner_dopair_subset_grid_construction(struct runner *restrict r,
                                       struct cell *restrict ci,
                                       struct part *restrict parts_i,
                                       int *restrict ind,
                                       double *restrict h_prev, double h_max,
                                       int count, struct cell *restrict cj) {
  const struct engine *restrict e = r->e;

  const int count_j = cj->hydro.count;
  struct part *restrict parts_j = cj->hydro.parts;

  if (!cell_is_active_hydro(ci, e) && cj->nodeID == e->nodeID)
    error("Running construction task for inactive cell!");

  /* Get the sort ID. */
  double shift[3] = {0.0, 0.0, 0.0};
  struct cell *ci_temp = ci;
  struct cell *cj_temp = cj;
  int sid = space_getsid(e->s, &ci_temp, &cj_temp, shift);

  /* Pick-out the sorted lists. */
  const struct sort_entry *sort_i = cell_get_hydro_sorts(ci, sid);
  const struct sort_entry *sort_j = cell_get_hydro_sorts(cj, sid);

  /* Useful variables*/
  const float dx_max = (ci->hydro.dx_max_sort + cj->hydro.dx_max_sort);

  const int flipped = ci != ci_temp;
  if (flipped) {
    /* ci on the right */

    /* Correct sid if the cells have been flipped */
    sid = 26 - sid;

    if (shift[1] != 0) {
      shift[1] = shift[1];
    }

    /* Get the minimal position of any particle of ci along the sorting axis. */
    const float di_min = sort_i[0].d;

    /* Loop over the neighbouring particles parts_j until they are definitely
     * too far to be a candidate ghost particle. */
    for (int pjd = count_j - 1;
         pjd >= 0 && sort_j[pjd].d > di_min - dx_max - h_max; pjd--) {

      /* Get a pointer to the jth particle. */
      int pj_idx = sort_j[pjd].i;
      struct part *restrict pj = &parts_j[pj_idx];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the left) */
      const double pjx = pj->x[0] - shift[0];
      const double pjy = pj->x[1] - shift[1];
      const double pjz = pj->x[2] - shift[2];

      /* Loop over all the unconverged particles in parts_i and check if pj
       * falls within the new search radius, but outside the old search radius
       * of the unconverged particle pi. */
      for (int pid = 0; pid < count; pid++) {

        /* Get a hold of the ith part in ci. */
        struct part *restrict pi = &parts_i[ind[pid]];
        const float hi = pi->r;
        const double hi_prev = h_prev[ind[pid]];

#ifdef SWIFT_DEBUG_CHECKS
        if (!part_is_active(pi, e)) {
          error(
              "Encountered inactive unconverged particle in ghost construction "
              "task!");
        }
#endif

        /* Compute the pairwise distance. */
        const double dx[3] = {pi->x[0] - pjx, pi->x[1] - pjy, pi->x[2] - pjz};
        const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

        /* Hit or miss? */
        if (r2 < hi * hi && r2 >= hi_prev * hi_prev) {
          delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid,
                                  pj_idx);
        }
      } /* Loop over unconverged particles in ci */
    }   /* Loop over particles in cj */
  } else {
    /* ci on the left */

    /* Get the maximal position of any particle of ci along the sorting axis. */
    const float dxi = sort_i[ci->hydro.count - 1].d + ci->hydro.dx_max_sort;

    /* Loop over the neighbouring particles parts_j until they are definitely
     * too far to be a candidate ghost particle. */
    for (int pjd = 0; pjd < count_j && sort_j[pjd].d < dxi + h_max; pjd++) {

      /* Get a pointer to the jth particle. */
      int pj_idx = sort_j[pjd].i;
      struct part *restrict pj = &parts_j[pj_idx];

      /* Skip inhibited particles. */
      if (part_is_inhibited(pj, e)) continue;

      /* Shift pj so that it is in the frame of ci (with cj on the right) */
      const double pjx = pj->x[0] + shift[0];
      const double pjy = pj->x[1] + shift[1];
      const double pjz = pj->x[2] + shift[2];

      /* Loop over all the unconverged particles in parts_i and check if pj
       * falls within the new search radius, but outside the old search radius
       * of the unconverged particle pi. */
      for (int pid = 0; pid < count; pid++) {

        /* Get a hold of the ith part in ci. */
        struct part *restrict pi = &parts_i[ind[pid]];
        const float hi = pi->r;
        const double hi_prev = h_prev[ind[pid]];

#ifdef SWIFT_DEBUG_CHECKS
        if (!part_is_active(pi, e)) {
          error(
              "Encountered inactive unconverged particle in ghost construction "
              "task!");
        }
#endif

        /* Compute the pairwise distance. */
        const double dx[3] = {pi->x[0] - pjx, pi->x[1] - pjy, pi->x[2] - pjz};
        const double r2 = dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2];

        /* Hit or miss? */
        if (r2 < hi * hi && r2 >= hi_prev * hi_prev) {
          delaunay_add_new_vertex(ci->grid.delaunay, pjx, pjy, pjz, sid,
                                  pj_idx);
        }
      } /* Loop over unconverged particles in ci */
    }   /* Loop over particles in cj */
  }     /* Flipped? */
}

__attribute__((always_inline)) INLINE static void
runner_doself_subset_grid_construction(struct runner *restrict r,
                                       struct cell *restrict ci,
                                       struct part *restrict parts_i,
                                       int *restrict ind,
                                       double *restrict h_prev, int count) {
  /* TODO */
}

#endif  // SWIFTSIM_RUNNER_DOIACT_GRID_H
