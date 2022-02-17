/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2022 Tsang Keung Chan (chantsangkeung@gmail.com)
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
#ifndef SWIFT_RT_SPHM1RT_COOLING_RATES_H
#define SWIFT_RT_SPHM1RT_COOLING_RATES_H


#include "rt_properties.h"
#include "rt_struct.h"

/* Local includes. */
#include <cvode/cvode.h>
#include <math.h>
#include <nvector/nvector_serial.h>
#include <stdio.h>
#include <stdlib.h>
#include <sundials/sundials_types.h>
#include <sunlinsol/sunlinsol_dense.h>
#include <sunmatrix/sunmatrix_dense.h>
#include <cvode/cvode_direct.h>        /* access to CVDls interface            */
#include <sys/types.h>
#include <time.h>




/**
 * @brief Computes the log_10 of the temperature from the log_10 internal energy u
 *
 * @param k_B_cgs boltzman constant in cgs
 * @param m_H_cgs hydrogen atom mass in cgs 
 * @param X_H hydrogen mass fraction
 * @param log_10_u_cgs Log base 10 of internal energy in cgs.
 * @param abundances    abundances of species in n_i/nH.
 *
 * @return log_10 of the temperature.
 *
 */
__attribute__((always_inline)) INLINE static double convert_u_to_temp(
    const double k_B_cgs, const double m_H_cgs, const double X_H, const double log_10_u_cgs, const double abundances[rt_species_count]) {

  double sumabundances = 0.0;
  for (int j = 0; j < rt_species_count; j++) {
    sumabundances += abundances[j];
  }
  double T_cgs = m_H_cgs / X_H / sumabundances * exp10(log_10_u_cgs) * 2.0 / 3.0 / k_B_cgs;
  return log10(T_cgs);
}


/**
 * @brief Computes the log_10 of the internal energy corresponding to a given
 * temperature, hydrogen neutral fraction
 *
 * @param k_B_cgs boltzman constant in cgs
 * @param m_H_cgs hydrogen atom mass in cgs 
 * @param log_T_cgs Log base 10 of temperature in K
 * @param X_H hydrogen mass fraction
 * @param abundances    abundances of species in n_i/nH.
 *
 * @return log_10 of the internal energy in cgs
 *
 */
__attribute__((always_inline)) INLINE static double convert_temp_to_u(
    const double k_B_cgs, const double m_H_cgs, const double log_T_cgs, const double X_H, const double abundances[rt_species_count]) {
  double sumabundances = 0.0;
  for (int j = 0; j < rt_species_count; j++) {
    sumabundances += abundances[j];
  }
  const double u_cgs = 1.5 * k_B_cgs * exp10(log_T_cgs) * sumabundances * X_H / m_H_cgs;
  return log10(u_cgs);
}


/**************************************/
/* RECOMBINATION COEFFICIENTS         */
/**************************************/

/**
 * @brief Computes the chemistry coefficient (Hui and Gnedin 1997)
 * @param log_T_cgs Log base 10 of temperature in K
 * @param onthespot use on the spot approximation?
 * @return alphalist  coefficients of recomination 
 * @return betalist  coefficients of collisional ionization
 */
INLINE static void compute_alphabeta_cgs(double log_T_cgs, int onthespot, double alphalist[rt_species_count], 
    double betalist[rt_species_count] ) {

  const double T_cgs = exp10(log_T_cgs);

  const double lambdaT = 315614.0 / T_cgs;

  /* Hydrogen coefficient */

  /* Computes the case A recombination coefficient for HII (Hui and Gnedin 1997) */
  double alphaAHII_cgs = 1.269e-13 * pow(lambdaT, 1.503) * pow(1.0 + pow(lambdaT / 0.522, 0.470), -1.923); 

  /* Computes the case B recombination coefficient for HII (Hui and Gnedin 1997) */
  double alphaBHII_cgs = 2.753e-14 * pow(lambdaT, 1.5) * pow(1.0 + pow(lambdaT / 2.740, 0.407), -2.242); 

  /* Computes the collisional ionization rate for HI (Theuns et al. 1998) */
  double betaHI_cgs = 1.17e-10 * sqrt(T_cgs) * exp(-157809.1 / T_cgs) / (1.0 + sqrt(T_cgs/1e5));

  /* Helium coefficient */

  const double lambdaTI = 2.0 * 285335.0 / T_cgs;

  const double lambdaTII = 2.0 * 631515.0 / T_cgs;

 /* Computes the case A recombination coefficient for HeII (Hui and Gnedin 1997) */
  double alphaAHeII_cgs = 3.0e-14 * pow(lambdaTI, 0.654);

  /* Computes the case B recombination coefficient for HeII (Hui and Gnedin 1997) */
  double alphaBHeII_cgs = 1.26e-14 * pow(lambdaTI, 0.750);

  /* Computes the Dielectronic recombination coefficient for HeII (Aldrovandi and Pequignot 1973) */
  double alphaDiHeII_cgs = 1.9e-3 * pow(T_cgs, -1.5) * exp(-4.7e5/T_cgs) * (1.0 + 0.3 * exp(-9.4e4/T_cgs));

 /* Computes the case A recombination coefficient for HeIII (Hui and Gnedin 1997) */
  double alphaAHeIII_cgs = 2.538e-13 * pow(lambdaTII, 1.503) * pow(1.0 + pow(lambdaTII/0.522, 0.470), -1.923);

  /* Computes the case B recombination coefficient for HeIII (Hui and Gnedin 1997) */
  double alphaBHeIII_cgs = 5.506e-14 * pow(lambdaTII, 1.5) * pow(1.0 + pow(lambdaTII/2.740, 0.407), -2.242);

  /* Computes the collisional ionization rate for HeI (Theuns et al. 1998) */
  double betaHeI_cgs = 4.76e-11 * sqrt(T_cgs) * exp(-285335.4 / T_cgs) / (1.0 + sqrt(T_cgs/1e5));


  /* Computes the collisional ionization rate for HeII (Theuns et al. 1998) */
  double betaHeII_cgs = 1.14e-11 * sqrt(T_cgs) * exp(-631515.0 / T_cgs) / (1.0 + sqrt(T_cgs/1e5));

  betalist[rt_sp_elec] = 0.0;
  betalist[rt_sp_HI] = betaHI_cgs;
  betalist[rt_sp_HII] = 0.0;
  betalist[rt_sp_HeI] = betaHeI_cgs;
  betalist[rt_sp_HeII] = betaHeII_cgs;
  betalist[rt_sp_HeIII] = 0.0;
  alphalist[rt_sp_elec] = 0.0;
  alphalist[rt_sp_HI] = 0.0;
  alphalist[rt_sp_HeI] = 0.0; 
  if (onthespot==1) {
    alphalist[rt_sp_HII] = alphaBHII_cgs;
    alphalist[rt_sp_HeII] = alphaBHeII_cgs + alphaDiHeII_cgs;
    alphalist[rt_sp_HeIII] = alphaBHeIII_cgs;
  } else {
    alphalist[rt_sp_HII] = alphaAHII_cgs;
    alphalist[rt_sp_HeII] = alphaAHeII_cgs + alphaDiHeII_cgs;
    alphalist[rt_sp_HeIII] = alphaAHeIII_cgs;
  }

}

/**************************************/
/* COOLING COEFFICIENTS         */
/**************************************/

/**
 * @brief Computes the cooling coefficient (Hui and Gnedin 1997)
 * @param log_T_cgs Log base 10 of temperature in K
 * @param onthespot use on the spot approximation?
 * @return Gammalist cooling coefficients of recomination and collisional ionization (recombination positive)
 */
INLINE static void compute_cooling_gamma_cgs(const double log_T_cgs, const int onthespot, double Gammalist[rt_species_count]) {

  const double T_cgs = exp10(log_T_cgs);

  const double lambdaT = 315614.0 / T_cgs;

  /* Hydrogen coefficient */
  /* Computes the collisional ionization cooling rate (Theuns+98) */
  double Gamma_colion_HI_cgs = 2.54e-21 * pow(T_cgs, 0.5) * exp(-157809.1 / T_cgs) / (1.0 + pow(T_cgs / 1.0e5, 0.5)); 

  /* Computes the collisional excitation cooling rate (Theuns+98) */
  double Gamma_line_HI_cgs  = 7.5e-19 * exp(-118348.0/ T_cgs) / (1.0 + pow(T_cgs / 1.0e5, 0.5)); 


  /* Computes the case A? recombination cooling rate (Hui & Gnedin 1997) */
  double Gamma_recomA_HII_cgs = 1.778e-29 * T_cgs * pow(lambdaT, 1.965) * pow(1.0 + pow(lambdaT/0.541, 0.502), -2.697);

  /* Computes the case B? recombination cooling rate (Hui & Gnedin 1997) */
  double Gamma_recomB_HII_cgs = 3.435e-30 * T_cgs * pow(lambdaT, 1.970) * pow(1.0 + pow(lambdaT/2.250, 0.376), -3.720); 


  /* Computes the Bremsstrahlung cooling rate (Theuns+98) */ 
  double Gamma_ff_HII_cgs  = 1.42e-27 * pow(T_cgs,0.5) * (1.1 + 0.34 * exp(-pow(5.5 - log_T_cgs, 2.0) / 3.0)); 

  /* Helium coefficient */

  const double lambdaTI = 2.0 * 285335.0 / T_cgs;

  const double lambdaTII = 2.0 * 631515.0 / T_cgs;

  /* Computes the collisional ionization cooling rate for HeI (Theuns+98) */
  double Gamma_colion_HeI_cgs = 1.88e-21 * sqrt(T_cgs) * exp(-285335.4 / T_cgs) / (1.0 + pow(T_cgs / 1.0e5, 0.5)); 

  /* Computes the collisional ionization cooling rate for HeI (Theuns+98) */
  double Gamma_colion_HeII_cgs = 9.90e-22 * sqrt(T_cgs) * exp(-631515.0 / T_cgs) / (1.0 + pow(T_cgs / 1.0e5, 0.5)); 

  /* Computes the collisional excitation cooling rate (Theuns+98) */
  double Gamma_line_HeII_cgs  = 5.54e-17 * pow(T_cgs, -0.397) * exp(-473638.0 / T_cgs) / (1.0 + pow(T_cgs / 1.0e5, 0.5));

  /* Computes the case A? recombination cooling rate (Hui & Gnedin 1997) */
  double Gamma_recomA_HeII_cgs = 1.38e-16 * T_cgs * 3.0e-14 * pow(lambdaTI, 0.654);

  /* Computes the case B? recombination cooling rate (Hui & Gnedin 1997) */
  double Gamma_recomB_HeII_cgs = 1.38e-16 * T_cgs * 1.26e-14 * pow(lambdaTI, 0.750);

  /* Computes the dielectric recombination cooling rate (Hui & Gnedin 1997) */
  double Gamma_recomDi_HeII_cgs = 1.24e-13 * pow(T_cgs, -1.5) * exp(-4.7e5/T_cgs) * (1.0 + 0.3 * exp(-9.4e4/T_cgs));;

  /* Computes the case A? recombination cooling rate (Hui & Gnedin 1997) */
  double Gamma_recomA_HeIII_cgs =  1.4224e-28 * T_cgs * pow(lambdaTII, 1.965) * pow(1.0 + pow(lambdaTII/0.541, 0.502), -2.697);

  /* Computes the case B? recombination cooling rate (Hui & Gnedin 1997) */
  double Gamma_recomB_HeIII_cgs = 2.748e-29 * T_cgs * pow(lambdaTII, 1.970) * pow(1.0 + pow(lambdaTII/2.250, 0.376), -3.720);

  /* Computes the Bremsstrahlung cooling rate (Theuns+98) */ 
  double Gamma_ff_HeII_cgs  = 1.42e-27 * pow(T_cgs,0.5) * (1.1 + 0.34 * exp(-pow(5.5 - log_T_cgs, 2.0) / 3.0)); 

  /* Computes the Bremsstrahlung cooling rate (Theuns+98) */ 
  double Gamma_ff_HeIII_cgs  = 5.68e-27 * pow(T_cgs,0.5) * (1.1 + 0.34 * exp(-pow(5.5 - log_T_cgs, 2.0) / 3.0)); 


  Gammalist[rt_sp_elec] = 0.0;
  Gammalist[rt_sp_HI] = Gamma_colion_HI_cgs + Gamma_line_HI_cgs;
  Gammalist[rt_sp_HII] = Gamma_ff_HII_cgs;
  Gammalist[rt_sp_HeI] = Gamma_colion_HeI_cgs;
  Gammalist[rt_sp_HeII] = Gamma_colion_HeII_cgs + Gamma_line_HeII_cgs + Gamma_ff_HeII_cgs;
  Gammalist[rt_sp_HeIII] = Gamma_ff_HeIII_cgs;
  if (onthespot==1) {
    Gammalist[rt_sp_HII] += Gamma_recomB_HII_cgs;
    Gammalist[rt_sp_HeII] += Gamma_recomB_HeII_cgs + Gamma_recomDi_HeII_cgs;
    Gammalist[rt_sp_HeIII] += Gamma_recomB_HeIII_cgs;
  } else {
    Gammalist[rt_sp_HII] += Gamma_recomA_HII_cgs;
    Gammalist[rt_sp_HeII] += Gamma_recomA_HeII_cgs + Gamma_recomDi_HeII_cgs;
    Gammalist[rt_sp_HeIII] += Gamma_recomA_HeIII_cgs;
  }

}



/**************************************/
/* PHOTO-IONIZATION COEFFICIENTS      */
/**************************************/

/**
 * @brief Output the photo-ionization coefficient: assume BB1e5 and Verner+1996 cross-section
 * Note!!!: numbers of frequency bins: has to be three from HI-HeI, HeI-HeII, HeII-infty
 * @return sigmalist  photo-ionization cross section in cm^2
 * @return epsilonlist  averaged thermal energy per ionization in erg
 * @return aindex   use to translate index to species  
 */
INLINE static void compute_photoionization_rate_cgs(double sigmalist[3][3], double epsilonlist[3][3], int aindex[3]) {
  /* the first index denotes frequency bins; the second index denotes species */
  /* HI: index 0 */
  aindex[0] =  rt_sp_HI;   /* use to translate index to species */
  sigmalist[0][0] = 2.99e-18;
  sigmalist[1][0] = 5.66e-19;
  sigmalist[2][0] = 7.84e-20;
  epsilonlist[0][0] = 6.17e-12;
  epsilonlist[1][0] = 2.81e-11;
  epsilonlist[2][0] = 7.77e-11;
  /* HeI: index 1 */
  aindex[1] =  rt_sp_HeI;   /* use to translate index to species */
  sigmalist[0][1] = 0.0;
  sigmalist[1][1] = 4.46e-18;
  sigmalist[2][1] = 1.19e-18;
  epsilonlist[0][1] = 0.0;
  epsilonlist[1][1] = 1.25e-11;
  epsilonlist[2][1] = 6.11e-11;

  /* HeII: index 2 */
  aindex[2] =  rt_sp_HeII;   /* use to translate index to species */
  sigmalist[0][2] = 0.0;
  sigmalist[1][2] = 0.0;
  sigmalist[2][2] = 1.05e-18;
  epsilonlist[0][2] = 0.0;
  epsilonlist[1][2] = 0.0;
  epsilonlist[2][2] = 1.27e-11;

}



/**
 * @brief Computes the chemistry and cooling coefficient
 * @param log_T_cgs Log base 10 of temperature in K
 * @param onthespot use on the spot approximation?
 * @param alphalist combined coefficients of recomination and collisional ionization 
 * @param betalist  coefficients of collisional ionization
 * @param Gammalist cooling coefficients of recomination and collisional ionization 
 * @param sigmalist  photo-ionization cross section in cm^2
 * @param epsilonlist  averaged thermal energy per ionization in erg
 * @param aindex   use to translate index to species  
 */
INLINE static void compute_rate_coefficients(
    const double log_T_cgs, const int onthespot, double alphalist[rt_species_count], double betalist[rt_species_count], 
    double Gammalist[rt_species_count], double sigmalist[3][3], double epsilonlist[3][3],  int aindex[3]) {
   compute_alphabeta_cgs(log_T_cgs,onthespot,alphalist,betalist);
   compute_cooling_gamma_cgs(log_T_cgs,onthespot,Gammalist);
   compute_photoionization_rate_cgs(sigmalist, epsilonlist, aindex);
}


/**
 * @brief function used to calculate chemistry changes.
 * Table indices and offsets for redshift, hydrogen number density and
 * helium fraction are passed it so as to compute them only once per particle.
 *
 * @param log_T_cgs Log base 10 of temperature in K
 * @param n_H_cgs Hydrogen number density in CGS units.
 * @param cred_cgs (reduced) speed of light in cm/s
 * @param abundances species abundance in n_i/nH.
 * @param ngamma_cgs photon density in cm^-3
 * @param alphalist combined coefficients of recomination and collisional ionization 
 * @param betalist  coefficients of collisional ionization
 * @param sigmalist  photo-ionization cross section in cm^2
 * @param aindex   use to translate index to species  
 *
 * @return chemistry_rates The chemistry rate (d n_i / d t in cgs)
 */
INLINE static void compute_chemistry_rate(const double n_H_cgs, const double cred_cgs, const double abundances[rt_species_count], 
    const double ngamma_cgs[3], const double alphalist[rt_species_count], const double betalist[rt_species_count], double sigmalist[3][3], 
    const int aindex[3], double chemistry_rates[rt_species_count]) {

  for (int j = 0; j < rt_species_count; j++) {
    chemistry_rates[j] = 0.0;
  }

  for (int i = 0; i < 3; i++) {
    /* photo-ionization */
    /* HI */
    chemistry_rates[aindex[0]] += -sigmalist[i][0] * cred_cgs * ngamma_cgs[i] * abundances[aindex[0]] * n_H_cgs;
    /* HeI */
    chemistry_rates[aindex[1]] += -sigmalist[i][1] * cred_cgs * ngamma_cgs[i] * abundances[aindex[1]] * n_H_cgs;
    /* HeII */
    chemistry_rates[aindex[2]] += -sigmalist[i][2] * cred_cgs * ngamma_cgs[i] * abundances[aindex[2]] * n_H_cgs;
    /* addition from photo-ionization of a lower level species, i.e. from HeI to HeII */
    chemistry_rates[aindex[2]] += sigmalist[i][1] * cred_cgs * ngamma_cgs[i] * abundances[aindex[1]] * n_H_cgs;
  }


  /* collisional ionization */
  chemistry_rates[rt_sp_HI] += -betalist[rt_sp_HI] * abundances[rt_sp_elec] * abundances[rt_sp_HI] * n_H_cgs * n_H_cgs; 
  chemistry_rates[rt_sp_HeI] += -betalist[rt_sp_HeI] * abundances[rt_sp_elec] * abundances[rt_sp_HeI] * n_H_cgs * n_H_cgs; 
  chemistry_rates[rt_sp_HeII] += -betalist[rt_sp_HeII] * abundances[rt_sp_elec] * abundances[rt_sp_HeII] * n_H_cgs * n_H_cgs; 

  /* collisional ionization from HeI -> HeII */
  chemistry_rates[rt_sp_HeII] += betalist[rt_sp_HeI] * abundances[rt_sp_elec] * abundances[rt_sp_HeI] * n_H_cgs * n_H_cgs; 

  /* recombination */
  chemistry_rates[rt_sp_HI] += alphalist[rt_sp_HII] * abundances[rt_sp_elec] * abundances[rt_sp_HII] * n_H_cgs * n_H_cgs;
  chemistry_rates[rt_sp_HeI] += alphalist[rt_sp_HeII] * abundances[rt_sp_elec] * abundances[rt_sp_HeII] * n_H_cgs * n_H_cgs;
  chemistry_rates[rt_sp_HeII] += alphalist[rt_sp_HeIII] * abundances[rt_sp_elec] * abundances[rt_sp_HeIII] * n_H_cgs * n_H_cgs;

  /* recombination from HeII to HeI */  
  chemistry_rates[rt_sp_HeII] += -alphalist[rt_sp_HeII] * abundances[rt_sp_elec] * abundances[rt_sp_HeII] * n_H_cgs * n_H_cgs;

}


/**
 * @brief function used to calculate radiation absorption rate.
 * Table indices and offsets for redshift, hydrogen number density and
 * helium fraction are passed it so as to compute them only once per particle.
 *
 * @param n_H_cgs Hydrogen number density in CGS units.
 * @param cred_cgs (reduced) speed of light in cm/s in cm/s
 * @param abundances species abundance in n_i/nH.
 * @param ngamma_cgs photon density in cm^-3
 * @param sigmalist  photo-ionization cross section in cm^2
 * @param aindex   use to translate index to species  
 *
 * @return absorption_rate The radiation absorption rate (d n_gamma / d t in cgs) excluded diffuse emission
 */
INLINE static void compute_radiation_rate(const double n_H_cgs, const double cred_cgs, const double abundances[rt_species_count], 
    const double ngamma_cgs[3], double sigmalist[3][3], const int aindex[3], double absorption_rate[3]) {

  absorption_rate[0] = 0.0;
  absorption_rate[1] = 0.0;
  absorption_rate[2] = 0.0;  

  for (int i = 0; i < 3; i++) {
    absorption_rate[0] += sigmalist[0][i] * cred_cgs * ngamma_cgs[0] * abundances[aindex[i]] * n_H_cgs;
    absorption_rate[1] += sigmalist[1][i] * cred_cgs * ngamma_cgs[1] * abundances[aindex[i]] * n_H_cgs;
    absorption_rate[2] += sigmalist[2][i] * cred_cgs * ngamma_cgs[2] * abundances[aindex[i]] * n_H_cgs;
  }

}



/**
 * @brief function used to calculate cooling rate.
 *
 * @param n_H_cgs Hydrogen number density in CGS units.
 * @param cred_cgs (reduced) speed of light in cm/s
 * @param abundances species abundance in n_i/nH.
 * @param ngamma_cgs photon density in cm^-3
 * @param Gammalist cooling coefficient
 * @param sigmalist  photo-ionization cross section in cm^2
 * @param epsilonlist  averaged thermal energy per ionization in erg
 * @param aindex   use to translate index to species  

 *
 * @return The net cooling rate of gas (d energy density / d t in cgs)
 */
INLINE static double compute_cooling_rate(const double n_H_cgs, const double cred_cgs, const double abundances[rt_species_count], 
    const double ngamma_cgs[3], const double Gammalist[rt_species_count], double sigmalist[3][3], double epsilonlist[3][3], 
    int aindex[3]) {
  /* The cooling rate of gas */ 
  double cooling_rate_cgs = 0.0;

  for (int j = 0; j < rt_species_count; j++) {
    cooling_rate_cgs += Gammalist[j] * abundances[rt_sp_elec] * abundances[j];
  }

  cooling_rate_cgs *= n_H_cgs * n_H_cgs; 

  /* The photo-heating rate */
  double photoheating_rate_cgs = 0.0;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      photoheating_rate_cgs +=  epsilonlist[i][j] * sigmalist[i][j] * cred_cgs * ngamma_cgs[i] * abundances[aindex[j]] * n_H_cgs;
    }
  }

  double Lambda_net_cgs = photoheating_rate_cgs - cooling_rate_cgs;

  return Lambda_net_cgs;
}



/**
 * @brief function used to enforce constraint equation.
 * If any of these
 * constraints are not met to within 0.1 per cent, the
 * abundances of all species involved in that constraint
 * are re-scaled accordingly. This routine also enforces
 * that all abundances are non-negative.
 * @param abundances species abundance in n_i/nH.
 * @param metal_mass_fraction metal mass 
 * 
 * @return finish_abundances species abundance in n_i/nH.
 */
INLINE static void enforce_constraint_equations(const double abundances[rt_species_count], const double metal_mass_fraction[rt_chemistry_element_count],
    double finish_abundances[rt_species_count]) {

  double metal_atomic_mass[rt_chemistry_element_count]; /* in unit of hydrogen mass */

  metal_atomic_mass[rt_chemistry_element_H] = 1.0;

  metal_atomic_mass[rt_chemistry_element_He] = 4.0;

  /* Initization */
  for (int j = 0; j < rt_species_count; j++) {
    finish_abundances[j] = max(abundances[j], 0.0);
  }

  /* check whether xHI bigger than one */
  if (finish_abundances[rt_sp_HI] > 1.01)
    error("HI fraction bigger than one");

  /* enforce hydrogen species constraint */
  finish_abundances[rt_sp_HI] = max(finish_abundances[rt_sp_HI], 0.0);
  finish_abundances[rt_sp_HI] = min(finish_abundances[rt_sp_HI], 1.0);
  finish_abundances[rt_sp_HII] = max(1.0 - finish_abundances[rt_sp_HI], 0.0);

  /* enforce helium species constraint */
  double aHe = metal_mass_fraction[rt_chemistry_element_He]/metal_mass_fraction[rt_chemistry_element_H] 
                             * metal_atomic_mass[rt_chemistry_element_H]/metal_atomic_mass[rt_chemistry_element_He]; 

  finish_abundances[rt_sp_HeI] = max(finish_abundances[rt_sp_HeI], 0.0);
  finish_abundances[rt_sp_HeII] = max(finish_abundances[rt_sp_HeII], 0.0);
  finish_abundances[rt_sp_HeIII] = max(aHe - finish_abundances[rt_sp_HeI] - finish_abundances[rt_sp_HeII], 0.0);
  double sumHe = finish_abundances[rt_sp_HeI] + finish_abundances[rt_sp_HeII] + finish_abundances[rt_sp_HeIII];
  if (sumHe > 1.01 * aHe) {
    finish_abundances[rt_sp_HeI] *= aHe / sumHe; 
    finish_abundances[rt_sp_HeII] *= aHe / sumHe; 
    finish_abundances[rt_sp_HeIII] *= aHe / sumHe; 
  }
  /* enforce electron constraint */
  finish_abundances[rt_sp_elec] = finish_abundances[rt_sp_HII] + finish_abundances[rt_sp_HeII] + 2.0 * finish_abundances[rt_sp_HeIII]; 

}


/**
 * @brief function used to enforce constraint equation.
 *
 * @param abundances species abundance in n_i/nH.
 * @param metal_mass_fraction metal mass
 * @param rho_cgs gas density
 * @param u_cgs gas internal energy per mass
 * @param u_min_cgs minimum (floor) gas internal energy per mass
 * @param dt_cgs this timestep
 * @param n_H_cgs Hydrogen number density in CGS units.
 * @param cred_cgs (reduced) speed of light in cm/s
 * @param ngamma_cgs photon density in cm^-3
 * @param sigmalist  photo-ionization cross section in cm^2
 * @param aindex   use to translate index to species 
 * @param onthespot use on the spot approximation?
 * @param alphalist combined coefficients of recomination and collisional ionization 
 * @param betalist  coefficients of collisional ionization
 * @return u_new_cgs  new internal energy per mass
 * @return new_abundances  new species abundances
 * @return new_ngamma_cgs  new photon density in cm^-3
 * @return max_relative_change maximum relative change in all variables 
 */
INLINE static void compute_explicit_solution(const double n_H_cgs, const double cred_cgs, const double dt_cgs, const double rho_cgs, 
    const double u_cgs, const double u_min_cgs, const double abundances[rt_species_count], 
    const double ngamma_cgs[3], const double alphalist[rt_species_count], const double betalist[rt_species_count], const double Gammalist[rt_species_count],
    double sigmalist[3][3], double epsilonlist[3][3], 
    int aindex[3], double *u_new_cgs,
    double new_abundances[rt_species_count], double new_ngamma_cgs[3], double *max_relative_change) {


  double absorption_rate[3], chemistry_rates[rt_species_count];

  compute_radiation_rate(n_H_cgs, cred_cgs, abundances, ngamma_cgs, sigmalist, aindex, absorption_rate);

  compute_chemistry_rate(n_H_cgs, cred_cgs, abundances, ngamma_cgs, alphalist, betalist, sigmalist, aindex, chemistry_rates);

  double Lambda_net_cgs;
  Lambda_net_cgs = compute_cooling_rate(n_H_cgs, cred_cgs, abundances, ngamma_cgs, Gammalist, sigmalist, epsilonlist, aindex);

  /* record for maximum relative change */
  double max_relative_change_value = 0.0;
  double relative_change;

  for (int j = 0; j < rt_species_count; j++) {
    new_abundances[j] = max(abundances[j] + chemistry_rates[j] / n_H_cgs * dt_cgs, 0.0);
    if ((new_abundances[j] > 1e-20) && (abundances[j] > 1e-20)) {
      relative_change = fabs(new_abundances[j] - abundances[j]) / abundances[j];
      max_relative_change_value = max(max_relative_change_value, relative_change);
    }
  }  

  double u_new_cgs_value;
  u_new_cgs_value = max(u_cgs + Lambda_net_cgs * dt_cgs / rho_cgs, u_min_cgs);
  relative_change = fabs(u_new_cgs_value - u_cgs) / u_cgs; 
  max_relative_change_value = max(max_relative_change_value, relative_change);

  for (int i = 0; i < 3; i++) {
    new_ngamma_cgs[i] = max(ngamma_cgs[i] - absorption_rate[i] * dt_cgs, 0.0);
    if ((new_ngamma_cgs[i] > 1e-8 * n_H_cgs) && (ngamma_cgs[i]> 1e-8 * n_H_cgs)) { 
      relative_change = fabs(new_ngamma_cgs[i] - ngamma_cgs[i]) / ngamma_cgs[i];
      max_relative_change_value = max(max_relative_change_value, relative_change);
    }
  } 


  *u_new_cgs = u_new_cgs_value; 
  *max_relative_change = max_relative_change_value;

}




/**
 * @brief function used to initialize species abundance in n_i/nH, assuming collisional ionization equilibrium.
 *
 * @param metal_mass_fraction metal mass
 * @param alphalist combined coefficients of recomination and collisional ionization 
 * @param betalist  coefficients of collisional ionization
 * @param init_abundances species abundances for initial conditions
 *
 * @return The net cooling rate of gas (d energy density / d t in cgs)
 */
INLINE static void initialize_abundances(const double alphalist[rt_species_count], const double betalist[rt_species_count], 
    const double metal_mass_fraction[rt_chemistry_element_count], double init_abundances[rt_species_count]) {

  double metal_atomic_mass[rt_chemistry_element_count]; /* in unit of hydrogen mass */

  metal_atomic_mass[rt_chemistry_element_H] = 1.0;

  metal_atomic_mass[rt_chemistry_element_He] = 4.0;

  init_abundances[rt_sp_HI] = alphalist[rt_sp_HII]/(betalist[rt_sp_HI] + alphalist[rt_sp_HII]);
  init_abundances[rt_sp_HII] = 1.0 - init_abundances[rt_sp_HI];

  double nHe_nH = metal_mass_fraction[rt_chemistry_element_He] / metal_mass_fraction[rt_chemistry_element_H] 
                * metal_atomic_mass[rt_chemistry_element_H] / metal_atomic_mass[rt_chemistry_element_He]; 
  double denoHe = (alphalist[rt_sp_HeIII] * betalist[rt_sp_HeI] + betalist[rt_sp_HeII] * betalist[rt_sp_HeI] 
                             + alphalist[rt_sp_HeII] * alphalist[rt_sp_HeIII]); 
  init_abundances[rt_sp_HeI] = alphalist[rt_sp_HeII] * alphalist[rt_sp_HeIII] * nHe_nH / denoHe;
  init_abundances[rt_sp_HeII] = alphalist[rt_sp_HeIII] * betalist[rt_sp_HeI] * nHe_nH / denoHe;
  init_abundances[rt_sp_HeIII] = betalist[rt_sp_HeI] * betalist[rt_sp_HeII] * nHe_nH / denoHe; 
  init_abundances[rt_sp_elec] = init_abundances[rt_sp_HII] + init_abundances[rt_sp_HeII] + 2.0 * init_abundances[rt_sp_HeIII];
}



/**
 * @brief Defines the right-hand side function.
 *
 * Defines the system of differential equations that make
 * up the right-hand side function, which will be integrated
 * by CVode.
 *
 * @param t Current time.
 * @param y Vector containing the variables to be integrated.
 * @param ydot Vector containing the time derivatives of the variables.
 * @param user_data The #UserData struct containing the input data.
 */
int f(realtype t, N_Vector y, N_Vector ydot, void *user_data) {
  struct UserData *data;

  data = (struct UserData *)user_data;

  /* First, loop through the enum types of all
   * non-eq species. If they are included in
   * the network then their abundance is in
   * the vector y. */
  int icount = 0; /* We use this to keep track of where we are in the vector y */
  int aindex[3];
  for (int i = 0; i < 3; i++) {
    aindex[i] = data->aindex[i];
  }
  for (int i = 0; i < 3; i++) {  
    data->abundances[aindex[i]] = (double)NV_Ith_S(y, icount);
    icount += 1;
  }

  /* Update the species not in the network */
  double finish_abundances[rt_species_count];
  enforce_constraint_equations(data->abundances, data->metal_mass_fraction, finish_abundances);
  for (int j = 0; j < rt_species_count; j++) {
    data->abundances[j] = finish_abundances[j];
  }

  /* If Thermal Evolution is switched on, the element in the
   * vector y is the internal energy (per unit volume). Use this
   * to update the temperature, and also the rates that depend on T */
  double u_cgs; 
  if (data->coolingon==1) {
    u_cgs = (double)NV_Ith_S(y,icount);
    icount += 1;
  } else {
    u_cgs = data->u_cgs;
  }

  /* the final element in the
   * vector y is the photon density.
   * */
  double ngamma_cgs[3]; 
  if (data->fixphotondensity==0) {
    for (int i = 0; i < 3; i++) {  
      ngamma_cgs[i] = (double)NV_Ith_S(y, icount);
      icount += 1;
    }
  } else {
    for (int i = 0; i < 3; i++) {  
      ngamma_cgs[i] = data->ngamma_cgs[i];
    }
  }



  double log_u_cgs = log10(u_cgs);
  double log_T_cgs = convert_u_to_temp(data->k_B_cgs, data->m_H_cgs, data->metal_mass_fraction[rt_chemistry_element_H], log_u_cgs, data->abundances);

  // Update rates
  double alphalist[rt_species_count], betalist[rt_species_count], Gammalist[rt_species_count], sigmalist[3][3], epsilonlist[3][3];

  compute_rate_coefficients(log_T_cgs, data->onthespot, alphalist, betalist, Gammalist, sigmalist, epsilonlist, aindex);

  if (data->useparams == 1) {
    betalist[rt_sp_elec] = 0.0;
    betalist[rt_sp_HI] = data->beta_cgs_H;
    betalist[rt_sp_HII] = 0.0;
    betalist[rt_sp_HeI] = 0.0;
    betalist[rt_sp_HeII] = 0.0;
    betalist[rt_sp_HeIII] = 0.0;
    alphalist[rt_sp_elec] = 0.0;
    alphalist[rt_sp_HI] = 0.0;
    alphalist[rt_sp_HeI] = 0.0; 
    if (data->onthespot==1) {
      alphalist[rt_sp_HII] = data->alphaB_cgs_H;
      alphalist[rt_sp_HeII] = 0.0;
      alphalist[rt_sp_HeIII] = 0.0;
    } else {
      alphalist[rt_sp_HII] = data->alphaA_cgs_H;
      alphalist[rt_sp_HeII] = 0.0;
      alphalist[rt_sp_HeIII] = 0.0;
    }

    sigmalist[0][0] = data->sigma_cross_cgs_H[0];
    sigmalist[1][0] = data->sigma_cross_cgs_H[1];
    sigmalist[2][0] = data->sigma_cross_cgs_H[2];
    sigmalist[0][1] = 0.0;
    sigmalist[1][1] = 0.0;
    sigmalist[2][1] = 0.0;
    sigmalist[0][2] = 0.0;
    sigmalist[1][2] = 0.0;
    sigmalist[2][2] = 0.0;    
  }

  // Compute creation and destruction rates
  double absorption_rate[3], chemistry_rates[rt_species_count];

  compute_radiation_rate(data->n_H_cgs, data->cred_cgs, data->abundances, ngamma_cgs, sigmalist, aindex, absorption_rate);

  compute_chemistry_rate(data->n_H_cgs, data->cred_cgs, data->abundances, ngamma_cgs, alphalist, betalist, sigmalist, aindex, chemistry_rates);

  double Lambda_net_cgs;
  Lambda_net_cgs = compute_cooling_rate(data->n_H_cgs, data->cred_cgs, data->abundances, ngamma_cgs, Gammalist, sigmalist, epsilonlist, aindex);

  int jcount = 0;
  /* Now set the output ydot vector for the chemical abundances */
  for (int i = 0; i < 3; i++) {  
    NV_Ith_S(ydot, jcount) = (realtype)(chemistry_rates[aindex[i]] / data->n_H_cgs);
    jcount += 1;
  }
  /* Now set the output ydot vector for the internal energy */
  if (data->coolingon == 1) {
    NV_Ith_S(ydot, jcount) = (realtype)(Lambda_net_cgs / data->rho_cgs);
    jcount += 1;
  }

  /* Now set the output ydot vector for the radiation density */
  if (data->fixphotondensity==0) {
    for (int i = 0; i < 3; i++) { 
      NV_Ith_S(ydot, jcount) = (realtype) (-absorption_rate[i]);
      jcount += 1;
    }
  }
  return(0);
}





#endif /* SWIFT_RT_SPHM1RT_COOLING_RATES_H */
