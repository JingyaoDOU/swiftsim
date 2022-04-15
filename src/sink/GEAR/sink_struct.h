/*******************************************************************************
 * This file is part of SWIFT.
 * Copyright (c) 2022 Yves Revaz (yves.revaz@epfl.ch)
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
#ifndef SWIFT_SINK_STRUCT_DEFAULT_H
#define SWIFT_SINK_STRUCT_DEFAULT_H

/**
 * @brief Sink-related fields carried by each *gas* particle.
 */
struct sink_part_data {

  /*! ID of the sink that will swallow this #part. */
  long long swallow_id;

  /*! Gravitational potential of the particle */
  float potential;
  
  /*! Gravitational potential of the particle */
  uint8_t can_form_sink;
    
};

#endif /* SWIFT_SINK_STRUCT_DEFAULT_H */