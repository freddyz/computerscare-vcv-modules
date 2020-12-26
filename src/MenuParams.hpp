#pragma once

using namespace rack;
/*struct AutoParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		std::string disp = Quantity::getDisplayValueString();
		return disp == "0" ? "Auto" : disp;
	}
};*/

/*
MenuParam

a few options:

-continuous
-on/off toggle
-multiselect
-xy


*/
struct MenuParam : MenuEntry {
	ParamWidget* speedParam;

	MenuLabel* johnLabel;

	MenuParam(ParamQuantity* pq,int type) {
		if(type==0) {
			speedParam = new SmallIsoButton();
			//		DisableableParamWidget* button =  createParam<DisableableParamWidget>(Vec(x, y), module, ComputerscareBolyPuttons::TOGGLE + index);

		}
		if(type==1) {
			speedParam = new MediumDotSnapKnob();
		}

		if(type==2) {
			speedParam = new SmoothKnob();
		}



		speedParam->paramQuantity = pq;

		johnLabel = construct<MenuLabel>(&MenuLabel::text, pq->getLabel());
		johnLabel->box.pos = Vec(speedParam->box.size.x+20, 0);

		addChild(speedParam);
		addChild(johnLabel);




	}
};
