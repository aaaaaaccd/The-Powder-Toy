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

int STKM_graphics(GRAPHICS_FUNC_ARGS);
void STKM_init_legs(playerst* playerp, int i);

int STKM2_update(UPDATE_FUNC_ARGS)
{
	run_stickman(&player2, UPDATE_FUNC_SUBCALL_ARGS);
	return 0;
}

int STKM2_create_override(ELEMENT_CREATE_OVERRIDE_FUNC_ARGS)
{
	if (player2.spwn || (p == -2 && ISSPAWN2))
		return -1;
	else
		return -4;
}

void STKM2_create(ELEMENT_CREATE_FUNC_ARGS)
{
	STKM_init_legs(&player2, i);
	player2.rocketBoots = 0;
	player2.spwn = 1;
	sim->part_create(-3, x, y, PT_SPAWN2);
}

void STKM2_init_element(ELEMENT_INIT_FUNC_ARGS)
{
	elem->Identifier = "DEFAULT_PT_STKM2";
	elem->Name = "STK2";
	elem->Colour = COLPACK(0x6464FF);
	elem->MenuVisible = 1;
	elem->MenuSection = SC_SPECIAL;
	elem->Enabled = 1;

	elem->Advection = 0.5f;
	elem->AirDrag = 0.00f * CFDS;
	elem->AirLoss = 0.2f;
	elem->Loss = 1.0f;
	elem->Collision = 0.0f;
	elem->Gravity = 0.0f;
	elem->Diffusion = 0.0f;
	elem->PressureAdd_NoAmbHeat = 0.00f	* CFDS;
	elem->Falldown = 0;

	elem->Flammable = 0;
	elem->Explosive = 0;
	elem->Meltable = 0;
	elem->Hardness = 0;

	elem->Weight = 50;

	elem->DefaultProperties.temp = R_TEMP+14.6f+273.15f;
	elem->HeatConduct = 0;
	elem->Latent = 0;
	elem->Description = "second stickman. Don't kill him! Control with wasd.";

	elem->State = ST_NONE;
	elem->Properties = PROP_NOCTYPEDRAW;

	elem->LowPressureTransitionThreshold = IPL;
	elem->LowPressureTransitionElement = NT;
	elem->HighPressureTransitionThreshold = IPH;
	elem->HighPressureTransitionElement = NT;
	elem->LowTemperatureTransitionThreshold = ITL;
	elem->LowTemperatureTransitionElement = NT;
	elem->HighTemperatureTransitionThreshold = 620.0f;
	elem->HighTemperatureTransitionElement = PT_FIRE;

	elem->Update = &STKM2_update;
	elem->Graphics = &STKM_graphics;
	elem->Func_Create_Override = &STKM2_create_override;
	elem->Func_Create = &STKM2_create;
	elem->Init = &STKM2_init_element;
}