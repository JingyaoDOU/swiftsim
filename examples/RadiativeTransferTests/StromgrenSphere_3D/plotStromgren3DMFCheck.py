# ----------------------------------------------------
# Stromgren 3D with multifrequency bins
# The test is identical to the test in Section 5.2.2 of Pawlik & Schaye 2011 doi:10.1111/j.1365-2966.2010.18032.x
# The full multifrequency solution is taken from their TT1D result in their Figure 4.
# Plot comparison of simulated neutral fraction and temperature with the solution.
# ----------------------------------------------------

import swiftsimio
from matplotlib import pyplot as plt
import matplotlib as mpl
import numpy as np
import sys
import stromgren_plotting_tools as spt

# Plot parameters
params = {
    "axes.labelsize": 14,
    "axes.titlesize": 14,
    "font.size": 14,
    "legend.fontsize": 14,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12,
    "xtick.direction": "in",
    "ytick.direction": "in",
    "xtick.top": True,
    "ytick.right": True,
    "xtick.major.width": 1.5,
    "ytick.major.width": 1.5,
    "axes.linewidth": 1.5,
    "text.usetex": True,
    "figure.figsize": (10, 4),
    "figure.subplot.left": 0.045,
    "figure.subplot.right": 0.99,
    "figure.subplot.bottom": 0.05,
    "figure.subplot.top": 0.99,
    "figure.subplot.wspace": 0.15,
    "figure.subplot.hspace": 0.12,
    "lines.markersize": 1,
    "lines.linewidth": 2.0,
}
mpl.rcParams.update(params)
mpl.rc("font", **{"family": "sans-serif", "sans-serif": ["Times"]})

scatterplot_kwargs = {
    "alpha": 0.6,
    "s": 4,
    "marker": ".",
    "linewidth": 0.0,
    "facecolor": "blue",
}

# Read in cmdline arg: Are we plotting only one snapshot, or all?
plot_all = False
try:
    snapnr = int(sys.argv[1])
except IndexError:
    plot_all = True
    snapnr = -1

snapshot_base = "output_MF"


def plot_compare(filename):
    # Read in data first
    print("working on", filename)
    data = swiftsimio.load(filename)
    meta = data.metadata
    boxsize = meta.boxsize
    scheme = str(meta.subgrid_scheme["RT Scheme"].decode("utf-8"))
    gamma = meta.hydro_scheme["Adiabatic index"][0]

    xstar = data.stars.coordinates
    xpart = data.gas.coordinates
    dxp = xpart - xstar
    r = np.sqrt(np.sum(dxp ** 2, axis=1))

    imf = spt.get_imf(scheme, data)
    xHI = imf.HI / (imf.HI + imf.HII)

    mu = spt.mean_molecular_weight(imf.HI, imf.HII, imf.HeI, imf.HeII, imf.HeIII)
    data.gas.T = spt.gas_temperature(data.gas.internal_energies, mu, gamma)

    outdict = spt.get_TT1Dsolution()

    fig, ax = plt.subplots(1, 2)

    ax[0].scatter(r, xHI, **scatterplot_kwargs)
    ax[0].plot(
        outdict["rtt1dlist"], outdict["xtt1dlist"], color="k", lw=2.0, label="TT1D"
    )
    ax[0].set_ylabel("Neutral Fraction")
    xlabel_units_str = meta.boxsize.units.latex_representation()
    ax[0].set_xlabel("r [$" + xlabel_units_str + "$]")
    ax[0].set_yscale("log")
    ax[0].set_xlim([0, boxsize[0] / 2.0])

    ax[1].scatter(r, data.gas.T, **scatterplot_kwargs)
    ax[1].plot(
        outdict["rTtt1dlist"], outdict["Ttt1dlist"], color="k", lw=2.0, label="TT1D"
    )
    ax[1].set_ylabel("T [K]")
    ax[1].set_xlabel("r [$" + xlabel_units_str + "$]")
    ax[1].set_yscale("log")
    ax[1].set_xlim([0, boxsize[0] / 2.0])
    ax[1].legend(loc="best")
    
    plt.tight_layout()
    figname = filename[:-5]
    figname += "-Stromgren3DMF.png"
    plt.savefig(figname, dpi=200)
    plt.close()


if __name__ == "__main__":
    snaplist = spt.get_snapshot_list(snapshot_base,plot_all,snapnr)
    for f in snaplist:
        plot_compare(f)
