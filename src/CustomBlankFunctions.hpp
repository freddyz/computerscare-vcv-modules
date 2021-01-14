#pragma once

inline int mapBlankFrameOffset(float uniformVal, int numFrames) {
	return numFrames > 0 ? ((int) floor(uniformVal * numFrames )) % numFrames : 0;
}