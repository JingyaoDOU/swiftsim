#!/bin/bash
export LANG=C:UTF-8
if [ ! -e plummer.hdf5 ]
then
    echo "Generating initial conditions for Plummer example..."
    python3 plummerIC.py
fi

../../swift --self-gravity --threads=6 params.yml 2>&1 | tee output.log

echo "Plotting results..."
# If params.yml is left at default values, should produce 100 snapshots -> Plot 0,10,20,...100
python3 plotdensity.py output_00*0.hdf5
