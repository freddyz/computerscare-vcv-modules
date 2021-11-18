//copied from JW Modules https://github.com/jeremywen/JW-Modules/blob/master/src/JWResizableHandle.hpp

/*
BSD 3-Clause License

Copyright (c) 2017, Jeremy Wentworth
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "rack.hpp"

using namespace rack;

struct ComputerscareResizeHandle : OpaqueWidget {
	bool right = false;
	Vec dragPos;
	Rect originalBox;

	ComputerscareResizeHandle() {
		box.size = Vec(RACK_GRID_WIDTH * 1, RACK_GRID_HEIGHT);
	}
	void onEnter(const event::Enter& e) override {
		OpaqueWidget::onEnter(e);
	}
	void onLeave(const event::Leave& e) override {
		OpaqueWidget::onLeave(e);
	}
	void onDragStart(const event::DragStart &e) override {
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;

		dragPos = APP->scene->mousePos;
		ModuleWidget *mw = getAncestorOfType<ModuleWidget>();
		assert(mw);
		originalBox = mw->box;
	}

	void onDragMove(const event::DragMove &e) override {
		ModuleWidget *mw = getAncestorOfType<ModuleWidget>();
		assert(mw);

		Vec newDragPos = APP->scene->mousePos;
		float deltaX = newDragPos.x - dragPos.x;

		Rect newBox = originalBox;
		Rect oldBox = mw->box;
		const float minWidth = 3 * RACK_GRID_WIDTH;
		if (right) {
			newBox.size.x += deltaX;
			newBox.size.x = std::fmax(newBox.size.x, minWidth);
			newBox.size.x = std::round(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
		}
		else {
			newBox.size.x -= deltaX;
			newBox.size.x = std::fmax(newBox.size.x, minWidth);
			newBox.size.x = std::round(newBox.size.x / RACK_GRID_WIDTH) * RACK_GRID_WIDTH;
			newBox.pos.x = originalBox.pos.x + originalBox.size.x - newBox.size.x;
		}

		// Set box and test whether it's valid
		mw->box = newBox;
		if (!APP->scene->rack->requestModulePos(mw, newBox.pos)) {
			mw->box = oldBox;
		}
	}
};