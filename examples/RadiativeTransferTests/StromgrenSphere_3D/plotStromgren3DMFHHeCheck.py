# ----------------------------------------------------
# Stromgren 3D with multifrequency bins
# The test is identical to the test in Section 5.3.2 of Pawlik & Schaye 2011 doi:10.1111/j.1365-2966.2010.18032.x
# The full multifrequency solution is taken from their TT1D result in their Figure 9.
# Plot comparison of simulated neutral fraction and temperature with the solution.
# ----------------------------------------------------

import swiftsimio
from matplotlib import pyplot as plt
import matplotlib as mpl
import matplotlib.lines as mlines
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

scatterplot_kwargs = {"alpha": 0.1, "s": 4, "marker": ".", "linewidth": 0.0}

# Read in cmdline arg: Are we plotting only one snapshot, or all?
plot_all = False
try:
    snapnr = int(sys.argv[1])
except IndexError:
    plot_all = True
    snapnr = -1

snapshot_base = "output_HHe"

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

    mu = spt.mean_molecular_weight(imf.HI, imf.HII, imf.HeI, imf.HeII, imf.HeIII)
    data.gas.T = spt.gas_temperature(data.gas.internal_energies, mu, gamma)

    sA = spt.get_abundances(scheme, data)
    xHI = sA.HI
    xHII = sA.HII
    xHeI = sA.HeI
    xHeII = sA.HeII
    xHeIII = sA.HeIII

    outdict = spt.get_TT1Dsolution_HHe()

    fig, ax = plt.subplots(1, 2)

    ax[0].scatter(r, xHI, **scatterplot_kwargs, facecolor="k")
    ax[0].scatter(r, xHII, **scatterplot_kwargs, facecolor="r")
    ax[0].scatter(r, xHeI, **scatterplot_kwargs, facecolor="b")
    ax[0].scatter(r, xHeII, **scatterplot_kwargs, facecolor="g")
    ax[0].scatter(r, xHeIII, **scatterplot_kwargs, facecolor="y")
    ax[0].plot(
        outdict["rHItt1dlist"],
        outdict["xHItt1dlist"],
        color="k",
        lw=2.0,
        ls="dashed",
        label="TT1D",
    )
    ax[0].plot(
        outdict["rHIItt1dlist"], outdict["xHIItt1dlist"], color="r", lw=2.0, ls="dashed"
    )
    ax[0].plot(
        outdict["rHeItt1dlist"], outdict["xHeItt1dlist"], color="b", lw=2.0, ls="dashed"
    )
    ax[0].plot(
        outdict["rHeIItt1dlist"],
        outdict["xHeIItt1dlist"],
        color="g",
        lw=2.0,
        ls="dashed",
    )
    ax[0].plot(
        outdict["rHeIIItt1dlist"],
        outdict["xHeIIItt1dlist"],
        color="y",
        lw=2.0,
        ls="dashed",
    )
    ax[0].set_ylabel("Abundances")
    xlabel_units_str = meta.boxsize.units.latex_representation()
    ax[0].set_xlabel("r [$" + xlabel_units_str + "$]")
    ax[0].set_yscale("log")
    ax[0].set_xlim([0, 5.4 * 1.3])
    ax[0].set_ylim([1e-5, 1.1])
    ax[0].legend(loc="best", fontsize=12)

    ax[1].scatter(r, data.gas.T, **scatterplot_kwargs)
    ax[1].plot(
        outdict["rTtt1dlist"],
        outdict["Ttt1dlist"],
        color="k",
        lw=2.0,
        label="TT1D",
        ls="dashed",
    )
    ax[1].set_ylabel("T [K]")
    ax[1].set_xlabel("r [$" + xlabel_units_str + "$]")
    ax[1].set_yscale("log")
    ax[1].set_xlim([0, 5.4 * 1.3])
    HI_line = mlines.Line2D([], [], lw=2, ls="dashed", label="HI", color="k")
    HII_line = mlines.Line2D([], [], lw=2, ls="dashed", label="HII", color="r")
    HeI_line = mlines.Line2D([], [], lw=2, ls="dashed", label="HeI", color="b")
    HeII_line = mlines.Line2D([], [], lw=2, ls="dashed", label="HeII", color="g")
    HeIII_line = mlines.Line2D([], [], lw=2, ls="dashed", label="HeIII", color="y")
    first_legend = ax[1].legend(
        handles=[HI_line, HII_line, HeI_line, HeII_line, HeIII_line],
        loc="best",
        fontsize=12,
    )
    ax[1].add_artist(first_legend)

    plt.tight_layout()
    figname = filename[:-5]
    figname += "-Stromgren3DMFHHe.png"
    plt.savefig(figname, dpi=200)
    plt.close()


if __name__ == "__main__":
    snaplist = spt.get_snapshot_list(snapshot_base,plot_all,snapnr)
    for f in snaplist:
        plot_compare(f)
