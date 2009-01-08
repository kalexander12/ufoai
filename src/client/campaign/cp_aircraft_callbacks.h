/**
 * @file cp_aircraft_callbacks.h
 * @brief Header file for menu related console command callbacks
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CP_AIRCRAFT_CALLBACKS_H
#define CP_AIRCRAFT_CALLBACKS_H

void AIM_AircraftStart_f(void);
void AIR_NewAircraft_f(void);
void AIM_ResetAircraftCvars_f (void);
void AIM_NextAircraft_f(void);
void AIM_PrevAircraft_f(void);
void AIM_AircraftReturnToBase_f(void);

#endif

