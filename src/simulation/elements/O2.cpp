/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "simulation/ElementsCommon.h"

int O2_update(UPDATE_FUNC_ARGS)
{
	int r,rx,ry;
	for (rx=-2; rx<3; rx++)
		for (ry=-2; ry<3; ry++)
			if (BOUNDS_CHECK && (rx || ry))
			{
				r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				if(parts[r>>8].tmp&1&&(r&0xFF)==PT_PLSM)
					continue;

				if ((r&0xFF)==PT_FIRE)
				{
					parts[r>>8].temp+=(rand()/(RAND_MAX/100));
					if(parts[r>>8].tmp&0x01)
					parts[r>>8].temp=3473;
					parts[r>>8].tmp |= 2;
				}
				if ((r&0xFF)==PT_FIRE || ((r&0xFF)==PT_PLSM && !(parts[r>>8].tmp&4)))
				{
					sim->part_create(i,x,y,PT_FIRE);
					parts[i].temp+=(rand()/(RAND_MAX/100));
					parts[i].tmp |= 2;
				}
			}

	if (parts[i].temp > 9973.15 && pv[y/CELL][x/CELL] > 250.0f && fabsf(gravx[((y/CELL)*(XRES/CELL))+(x/CELL)]) + fabsf(gravy[((y/CELL)*(XRES/CELL))+(x/CELL)]) > 20)
	{
		if (rand()%5 < 1)
		{
			int j;
			sim->part_create(i,x,y,PT_BRMT);

			j = sim->part_create(-3,x+rand()%3-1,y+rand()%3-1,PT_NEUT);
			if (j != -1)
				parts[j].temp = 15000;
			j = sim->part_create(-3,x+rand()%3-1,y+rand()%3-1,PT_PHOT);
			if (j != -1)
				parts[j].temp = 15000;
			j = sim->part_create(-3,x+rand()%3-1,y+rand()%3-1,PT_PLSM);
			if (j != -1)
			{
				parts[j].temp = 15000;
				parts[j].tmp |= 4;
			}

			parts[i].temp = 15000;
			pv[y/CELL][x/CELL] += 300;
		}
	}
	return 0;
}

void O2_init_element(ELEMENT_INIT_FUNC_ARGS)
{
	elem->Identifier = "DEFAULT_PT_O2";
	elem->Name = "OXYG";
	elem->Colour = COLPACK(0x80A0FF);
	elem->MenuVisible = 1;
	elem->MenuSection = SC_GAS;
	elem->Enabled = 1;

	elem->Advection = 2.0f;
	elem->AirDrag = 0.00f * CFDS;
	elem->AirLoss = 0.99f;
	elem->Loss = 0.30f;
	elem->Collision = -0.1f;
	elem->Gravity = 0.0f;
	elem->Diffusion = 3.0f;
	elem->PressureAdd_NoAmbHeat = 0.000f	* CFDS;
	elem->Falldown = 0;

	elem->Flammable = 0;
	elem->Explosive = 0;
	elem->Meltable = 0;
	elem->Hardness = 0;

	elem->Weight = 1;

	elem->DefaultProperties.temp = R_TEMP+0.0f	+273.15f;
	elem->HeatConduct = 70;
	elem->Latent = 0;
	elem->Description = "Oxygen gas. Ignites easily.";

	elem->State = ST_GAS;
	elem->Properties = TYPE_GAS;

	elem->LowPressureTransitionThreshold = IPL;
	elem->LowPressureTransitionElement = NT;
	elem->HighPressureTransitionThreshold = IPH;
	elem->HighPressureTransitionElement = NT;
	elem->LowTemperatureTransitionThreshold = 90.0f;
	elem->LowTemperatureTransitionElement = PT_LO2;
	elem->HighTemperatureTransitionThreshold = ITH;
	elem->HighTemperatureTransitionElement = NT;

	elem->Update = &O2_update;
	elem->Graphics = NULL;
	elem->Init = &O2_init_element;
}