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

int EXOT_update(UPDATE_FUNC_ARGS)
{
	int r, rt, rx, ry, nb, rrx, rry, trade, tym, t;
	t = parts[i].type;
	for (rx=-2; rx<=2; rx++)
		for (ry=-2; ry<=2; ry++)
			if (BOUNDS_CHECK && (rx || ry))
			{
				r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				if ((r&0xFF) == PT_WARP)
				{
					if (parts[r>>8].tmp2>2000)
						if (1>rand()%100)
						{
							parts[i].tmp2 += 100;
						}
				}
				else if ((r&0xFF) == PT_EXOT)
				{
					if (parts[r>>8].ctype == PT_PROT)
						parts[i].ctype = PT_PROT;
					if (parts[r>>8].life == 1500 && !(rand()%1000))
						parts[i].life = 1500;
				}
				else if ((r&0xFF) == PT_LAVA)
				{
					//turn molten TTAN or molten GOLD to molten VIBR 
					if (parts[r>>8].ctype == PT_TTAN || parts[r>>8].ctype == PT_TTAN)
					{
						if (!(rand()%10))
						{
							parts[r>>8].ctype = PT_VIBR;
							kill_part(i);
							return 1;
						}
					}
					//molten VIBR will kill the leftover EXOT though, so the VIBR isn't killed later
					else if (parts[r>>8].ctype == PT_VIBR)
					{
						if (!(rand()%1000))
						{
							kill_part(i);
							return 1;
						}
					}
				}
				if (parts[i].tmp>245 && parts[i].life>1000)
					if ((r&0xFF)!=PT_EXOT && (r&0xFF)!=PT_BREL && !(ptypes[r&0xFF].properties&PROP_INDESTRUCTIBLE) && (r&0xFF)!=PT_PRTI && (r&0xFF)!=PT_PRTO && (r&0xFF)!=PT_PHOT && (r&0xFF)!=PT_VOID && (r&0xFF)!=PT_NBHL && (r&0xFF)!=PT_WARP && (r&0xFF)!=PT_NEUT)
					{
						sim->part_create(i, x, y, parts[r>>8].type);
						return 1;
					}
			}
	parts[i].tmp--;
	parts[i].tmp2--;
	//reset tmp every 250 frames, gives EXOT it's slow flashing effect 
	if (parts[i].tmp<1 || parts[i].tmp>250)
		parts[i].tmp = 250;
	if (parts[i].tmp2 < 1)
		parts[i].tmp2 = 1;
	else if (parts[i].tmp2 > 6000)
	{
		parts[i].tmp2 = 10000;
		if (parts[i].life < 1001)
		{
			part_change_type(i, x, y, PT_WARP);
			return 0;
		}
	}
	else if (parts[i].life < 1001)
		pv[y/CELL][x/CELL] += (parts[i].tmp2*CFDS)/160000;

	if (pv[y/CELL][x/CELL]>200 && parts[i].temp>9000 && parts[i].tmp2>200)
	{
		parts[i].tmp2 = 6000;
		part_change_type(i, x, y, PT_WARP);
		return 0;
	}		
	if (parts[i].tmp2 > 100)
	{
		for (trade = 0; trade<9; trade++)
		{
			rx = rand()%5-2;
			ry = rand()%5-2;
			if (BOUNDS_CHECK && (rx || ry))
			{
				r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				if ((r&0xFF)==t && (parts[i].tmp2>parts[r>>8].tmp2) && parts[r>>8].tmp2>=0)//diffusion
				{
					tym = parts[i].tmp2 - parts[r>>8].tmp2;
					if (tym == 1)
					{
						parts[r>>8].tmp2++;
						parts[i].tmp2--;
						break;
					}
					if (tym > 0)
					{
						parts[r>>8].tmp2 += tym/2;
						parts[i].tmp2 -= tym/2;
						break;
					}
				}
			}
		}
	}
	if (parts[i].ctype == PT_PROT)
	{
		if (parts[i].temp < 50.0f)
		{
			sim->part_create(i, x, y, PT_HFLM);
			return 1;
		}
		else
			parts[i].temp -= 1.0f;
	}
	else if (parts[i].temp<273.15f)
	{
		parts[i].vx = 0;
		parts[i].vy = 0;
		pv[y/CELL][x/CELL] -= 0.01f;
		parts[i].tmp--;
	}
	return 0;

}

int EXOT_graphics(GRAPHICS_FUNC_ARGS)
{
	int q = (int)cpart->temp;
	int b = cpart->tmp;
	int c = cpart->tmp2;	
	if (cpart->life < 1001)
	{
		if ((cpart->tmp2 - 1)>rand()%1000)
		{	
			float frequency = 0.04045f;
			*colr = (int)(sinf(frequency*c + 4) * 127 + 150);
			*colg = (int)(sinf(frequency*c + 6) * 127 + 150);
			*colb = (int)(sinf(frequency*c + 8) * 127 + 150);
			*firea = 100;
			*firer = 0;
			*fireg = 0;
			*fireb = 0;
			*pixel_mode |= PMODE_FLAT;
			*pixel_mode |= PMODE_FLARE;
		}
		else
		{
			float frequency = 0.00045f;	
			*colr = (int)(sinf(frequency*q + 4) * 127 + (b/1.7));
			*colg = (int)(sinf(frequency*q + 6) * 127 + (b/1.7));
			*colb = (int)(sinf(frequency*q + 8) * 127 + (b/1.7));
			*cola = cpart->tmp / 6;	
			*firea = *cola;
			*firer = *colr;
			*fireg = *colg;
			*fireb = *colb;
			*pixel_mode |= FIRE_ADD;
			*pixel_mode |= PMODE_BLUR;
		}
	}
	else
	{
		float frequency = 0.01300f;	
		*colr = (int)(sinf(frequency*q + 6.00) * 127 + ((b/2.9) + 80));
		*colg = (int)(sinf(frequency*q + 6.00) * 127 + ((b/2.9) + 80));
		*colb = (int)(sinf(frequency*q + 6.00) * 127 + ((b/2.9) + 80));
		*cola = cpart->tmp / 6;	
		*firea = *cola;
		*firer = *colr;
		*fireg = *colg;
		*fireb = *colb;
		*pixel_mode |= FIRE_ADD;
		*pixel_mode |= PMODE_BLUR;
	}
	return 0;
}

void EXOT_init_element(ELEMENT_INIT_FUNC_ARGS)
{
	elem->Identifier = "DEFAULT_PT_EXOT";
	elem->Name = "EXOT";
	elem->Colour = COLPACK(0x404040);
	elem->MenuVisible = 1;
	elem->MenuSection = SC_NUCLEAR;
	elem->Enabled = 1;

	elem->Advection = 0.3f;
	elem->AirDrag = 0.02f * CFDS;
	elem->AirLoss = 0.95f;
	elem->Loss = 0.80f;
	elem->Collision = 0.0f;
	elem->Gravity = 0.15f;
	elem->Diffusion = 0.00f;
	elem->PressureAdd_NoAmbHeat = 0.0003f	* CFDS;
	elem->Falldown = 2;

	elem->Flammable = 0;
	elem->Explosive = 0;
	elem->Meltable = 0;
	elem->Hardness = 2;

	elem->Weight = 46;

	elem->DefaultProperties.temp = R_TEMP-2.0f	+273.15f;
	elem->HeatConduct = 250;
	elem->Latent = 0;
	elem->Description = "Exotic matter. Explodes with excess exposure to electrons. Has many other odd reactions.";

	elem->State = ST_LIQUID;
	elem->Properties = TYPE_LIQUID|PROP_NEUTPASS;

	elem->LowPressureTransitionThreshold = IPL;
	elem->LowPressureTransitionElement = NT;
	elem->HighPressureTransitionThreshold = IPH;
	elem->HighPressureTransitionElement = NT;
	elem->LowTemperatureTransitionThreshold = ITL;
	elem->LowTemperatureTransitionElement = NT;
	elem->HighTemperatureTransitionThreshold = ITH;
	elem->HighTemperatureTransitionElement = NT;

	elem->DefaultProperties.life = 1000;
	elem->DefaultProperties.tmp = 244;

	elem->Update = &EXOT_update;
	elem->Graphics = &EXOT_graphics;
	elem->Init = &EXOT_init_element;
}