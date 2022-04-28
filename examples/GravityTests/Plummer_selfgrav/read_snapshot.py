import h5py

class snapshot:
    def __init__(self,fname,read_particles = True):
        self._f = h5py.File(fname,'r')
        self._read_units()
        self._read_header()
        self._read_parameters()
        if read_particles:
            self._read_particles_DM()
        
    # Internal Units
    def _read_units(self):
        units = self._f['Units'].attrs
        self.UnitMass_in_cgs = units['Unit mass in cgs (U_M)'][0]
        self.UnitLength_in_cgs = units['Unit length in cgs (U_L)'][0]
        self.UnitTemp_in_cgs = units['Unit temperature in cgs (U_T)'][0]
        self.UnitTime_in_cgs = units['Unit time in cgs (U_t)'][0]
        self.UnitVelocity_in_cgs = self.UnitLength_in_cgs / self.UnitTime_in_cgs
    
    # Header
    def _read_header(self):
        header = self._f['Header'].attrs
        self.BoxSize = header['BoxSize']
        self.time = header['Time'][0]
    
    # Run Used Parameters
    def _read_parameters(self):
        params = self._f['Parameters'].attrs
        self.Gravity_max_physical_DM_softening = float(params['Gravity:max_physical_DM_softening'])
        self.Gravity_theta = float(params['Gravity:theta_cr'])
        self.IC_filename = params['InitialConditions:file_name'].decode('utf-8')
        self.IC_periodic = bool(params['InitialConditions:periodic'])
        self.IC_shift = params['InitialConditions:shift']
        self.Snapshots_delta_time = float(params['Snapshots:delta_time'])
        self.time_begin = float(params['TimeIntegration:time_begin'])
        self.time_end = float(params['TimeIntegration:time_end'])
        
    # Particle Data
    def _read_particles_DM(self):
        import numpy as np
        data = self._f['DMParticles']
        self.pos = np.array(data['Coordinates'])
        self.vel = np.array(data['Velocities'])
        self.mass = np.array(data['Masses'])
        self.pot = np.array(data['Potentials'])
        self.IDs = np.array(data['ParticleIDs'])
    
    # Useful Methods
    year_in_sec = 31557600
    def time_Gyr(self):
        return self.time * self.UnitTime_in_cgs / (self.year_in_sec * 1.0e9)

    def time_Myr(self):
        return self.time * self.UnitTime_in_cgs / (self.year_in_sec * 1.0e6)

    def time_begin_Gyr(self):
        return self.time_begin * self.UnitTime_in_cgs / (year_in_sec * 1.0e9)

    def time_end_Gyr(self):
        return self.time_end * self.UnitTime_in_cgs / (year_in_sec * 1.0e9)
