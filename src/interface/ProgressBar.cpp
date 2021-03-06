#include "ProgressBar.h"
#include "common/Format.h"
#include "graphics/VideoBuffer.h"

ProgressBar::ProgressBar(Point position, Point size):
	Component(position, size)
{

}

int ProgressBar::GetProgress()
{
	return progress;
}

void ProgressBar::SetProgress(int progress)
{
	if (progress < 0)
		this->progress = 0;
	else if (progress > 100)
		this->progress = 100;
	else
		this->progress = progress;
}

void ProgressBar::OnDraw(VideoBuffer *vid)
{
	// clear area
	vid->FillRect(position.X, position.Y, size.X, size.Y, 0, 0, 0, 255);

	// border
	if (enabled)
		vid->DrawRect(position.X, position.Y, size.X, size.Y, 255, 255, 255, 255);
	else
		vid->DrawRect(position.X, position.Y, size.X, size.Y, (int)(255*.55f), (int)(255*.55f), (int)(255*.55f), 255);

	int width = (size.X-2)*progress/100.0f;
	vid->FillRect(position.X+1, position.Y+1, width, size.Y-2, COLR(color), COLG(color), COLB(color), COLA(color));

	std::string progressStr = Format::NumberToString<int>(progress);
	progressStr += "%";
	width = VideoBuffer::TextSize(progressStr).X;
	if (progress < 53)
		vid->DrawText(position.X + size.X/2 - width/2, position.Y + (size.Y-8)/2, progressStr, 192, 192, 192, 255);
	else
		vid->DrawText(position.X + size.X/2 - width/2, position.Y + (size.Y-8)/2, progressStr, 63, 63, 63, 255);
}
