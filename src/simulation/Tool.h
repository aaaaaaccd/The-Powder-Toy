/**
 * Powder Toy - Tool (header)
 *
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
#ifndef TOOL_H
#define TOOL_H

#include <iostream>
#include "defines.h"


enum { ELEMENT_TOOL, WALL_TOOL, TOOL_TOOL, DECO_TOOL, GOL_TOOL, INVALID_TOOL, DECO_PRESET, FAV_MENU_BUTTON, HUD_MENU_BUTTON };

struct Point;
class Brush;
class Simulation;
class Tool
{
	std::string identifier;
	std::string description;
protected:
	int type;
	int toolID;
public:
	Tool(int toolID, std::string toolIdentifier, std::string description);
	Tool(int toolType, int toolID, std::string toolIdentifier, std::string description);
	virtual ~Tool() {}

	int GetType() { return type; }
	int GetID() { return toolID; }
	std::string GetIdentifier() { return identifier; }
	std::string GetDescription() { return description; }

	virtual int DrawPoint(Simulation *sim, Brush *brush, Point position, float toolStrength);
	virtual void DrawLine(Simulation *sim, Brush *brush, Point startPos, Point endPos, bool held, float toolStrength);
	virtual void DrawRect(Simulation *sim, Brush *brush, Point startPos, Point endPos);
	virtual int FloodFill(Simulation *sim, Brush *brush, Point position);
	virtual void Click(Simulation *sim, Point position);
	virtual Tool* Sample(Simulation *sim, Point position);
};

class ElementTool : public Tool
{
public:
	ElementTool(Simulation * sim, int elementID);
	int GetID();
};

class PlopTool : public ElementTool
{
public:
	PlopTool(Simulation * sim, int elementID);

	virtual int DrawPoint(Simulation *sim, Brush *brush, Point position, float toolStrength);
	virtual void DrawLine(Simulation *sim, Brush *brush, Point startPos, Point endPos, bool held, float toolStrength);
	virtual void DrawRect(Simulation *sim, Brush *brush, Point startPos, Point endPos);
	virtual int FloodFill(Simulation *sim, Brush *brush, Point position);
	virtual void Click(Simulation *sim, Point position);
};

class GolTool : public Tool
{
public:
	GolTool(int golID);
	int GetID();

	virtual int DrawPoint(Simulation *sim, Brush *brush, Point position, float toolStrength);
	virtual void DrawLine(Simulation *sim, Brush *brush, Point startPos, Point endPos, bool held, float toolStrength);
	virtual void DrawRect(Simulation *sim, Brush *brush, Point startPos, Point endPos);
	virtual int FloodFill(Simulation *sim, Brush *brush, Point position);
};

class WallTool : public Tool
{
public:
	WallTool(int wallID);
	int GetID() { if (type == WALL_TOOL) return toolID; else return -1; }

	virtual int DrawPoint(Simulation *sim, Brush *brush, Point position, float toolStrength);
	virtual void DrawLine(Simulation *sim, Brush *brush, Point startPos, Point endPos, bool held, float toolStrength);
	virtual void DrawRect(Simulation *sim, Brush *brush, Point startPos, Point endPos);
	virtual int FloodFill(Simulation *sim, Brush *brush, Point position);
};

class StreamlineTool : public WallTool
{
public:
	StreamlineTool();

	virtual int DrawPoint(Simulation *sim, Brush *brush, Point position, float toolStrength);
	virtual void DrawLine(Simulation *sim, Brush *brush, Point startPos, Point endPos, bool held, float toolStrength);
	virtual int FloodFill(Simulation *sim, Brush *brush, Point position);
};

class ToolTool : public Tool
{
public:
	ToolTool(int toolID);
	int GetID() { if (type == TOOL_TOOL) return toolID; else return -1; }

	virtual int DrawPoint(Simulation *sim, Brush *brush, Point position, float toolStrength);
	virtual void DrawLine(Simulation *sim, Brush *brush, Point startPos, Point endPos, bool held, float toolStrength);
	virtual void DrawRect(Simulation *sim, Brush *brush, Point startPos, Point endPos);
	virtual int FloodFill(Simulation *sim, Brush *brush, Point position);
	virtual void Click(Simulation *sim, Point position);
};

class PropTool : public ToolTool
{
public:
	PropTool();
	~PropTool() {}

	virtual int DrawPoint(Simulation *sim, Brush *brush, Point position, float toolStrength);
	virtual void DrawLine(Simulation *sim, Brush *brush, Point startPos, Point endPos, bool held, float toolStrength);
	virtual void DrawRect(Simulation *sim, Brush *brush, Point startPos, Point endPos);
	virtual int FloodFill(Simulation *sim, Brush *brush, Point position);

	PropertyType propType;
	PropertyValue propValue;
	size_t propOffset;
};

class DecoTool : public Tool
{
public:
	DecoTool(int decoID);
	int GetID() { if (type == DECO_TOOL) return toolID; else return -1; }

	virtual int DrawPoint(Simulation *sim, Brush *brush, Point position, float toolStrength);
	virtual void DrawLine(Simulation *sim, Brush *brush, Point startPos, Point endPos, bool held, float toolStrength);
	virtual void DrawRect(Simulation *sim, Brush *brush, Point startPos, Point endPos);
	virtual int FloodFill(Simulation *sim, Brush *brush, Point position);
	virtual Tool* Sample(Simulation *sim, Point position);
};

class InvalidTool : public Tool
{
protected:
	InvalidTool(int type, int ID, std::string identifier, std::string description);
public:
	int GetID() { return -1; }

	virtual int DrawPoint(Simulation *sim, Brush *brush, Point position, float toolStrength) final;
	virtual void DrawLine(Simulation *sim, Brush *brush, Point startPos, Point endPos, bool held, float toolStrength) final;
	virtual void DrawRect(Simulation *sim, Brush *brush, Point startPos, Point endPos) final;
	virtual int FloodFill(Simulation *sim, Brush *brush, Point position) final;
	virtual Tool* Sample(Simulation *sim, Point position) final;
};

class DecoPresetTool : public InvalidTool
{
public:
	DecoPresetTool(int presetID);
	int GetID() { if (type == DECO_PRESET) return toolID; else return -1; }
};

class FavTool : public InvalidTool
{
public:
	FavTool(int favID);
	int GetID() { if (type == FAV_MENU_BUTTON) return toolID; else return -1; }
};

class HudTool : public InvalidTool
{
public:
	HudTool(int hudID);
	int GetID() { if (type == HUD_MENU_BUTTON) return toolID; else return -1; }
};

#endif
