#!/bin/bash

# Generate the initial conditions if they are not present.
if [ ! -e multiTypes.hdf5 ]
then
    echo "Generating initial conditions for the multitype box example..."
    python makeIC.py 50 60
fi

../swift -s -g -t 16 multiTypes.yml 2>&1 | tee output.log
