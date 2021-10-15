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
struct ParamAndType {
	ParamQuantity* param;
	int type;
	ParamAndType(ParamQuantity* p, int t) {
		param = p;
		type = t;
	}
};

struct SubMenuAndKnob : MenuItem {
	SubMenuAndKnob() {
		MenuItem();
	}
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Option 1"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Option 2"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Option 3"));

		return menu;
	}

};
/*

KeyboardControlChildMenu *kbMenu = new KeyboardControlChildMenu();
		kbMenu->text = "Keyboard Controls";
		kbMenu->rightText = RIGHT_ARROW;
		kbMenu->blank = blank;
		menu->addChild(kbMenu);*/

struct SmoothSlider : ui::Slider {
	SmoothSlider(ParamQuantity* paramQ) {
		box.size.x = 180.0f;
		quantity = paramQ;
	}
};

struct MomentaryButton : ui::Button {
	MomentaryButton(ParamQuantity* paramQ) {
		box.size.x = 32.f;
		quantity = paramQ;
	}
};

struct MenuToggle : MenuItem
{
	ParamQuantity *myParamQuantity;
	MenuToggle(ParamQuantity* pq)
	{
		myParamQuantity = pq;
		text = pq->getLabel();
	}

	void onAction(const event::Action &e) override
	{
		myParamQuantity->setValue( myParamQuantity->getValue() == 0 ? 1 : 0);
	}
	void step() override {
		rightText = myParamQuantity->getValue() == 1 ? "✔" : "";
		MenuItem::step();
	}
};

struct MenuParam : MenuEntry {
	ParamWidget* pWidget;

	SmoothSlider* slider;
	MenuToggle *toggle;

	MenuLabel* johnLabel;
	MenuLabel* displayString;
	SubMenuAndKnob *submenu;
	float controlRightMargin = 6;

	MenuParam(ParamQuantity* param, int type) {
		if (type == 0) {
			toggle = new MenuToggle(param);

			//toggle->box.pos = Vec(controlRightMargin, 0);
			addChild(toggle);
			//johnLabel = construct<MenuLabel>(&MenuLabel::text, param->getLabel());
			//johnLabel->box.pos = Vec(button->box.size.x + controlRightMargin * 2, 0);

			//addChild(johnLabel);
		}
		else if (type == 1) {
			/*pWidget = new MediumDotSnapKnob();
			pWidget->paramQuantity = param;
			pWidget->box.pos = Vec(controlRightMargin, 0);

			addChild(pWidget);
			johnLabel = construct<MenuLabel>(&MenuLabel::text, param->getLabel());
			johnLabel->box.pos = Vec((type == 2 ? slider->box.size.x : pWidget->box.size.x) + controlRightMargin * 2, 0);
			addChild(johnLabel);*/
		}
		else if (type == 2) {
			slider = new SmoothSlider(param);
			slider->box.pos = Vec(controlRightMargin, 0);
			addChild(slider);

		}
		box.size.y = 32;


		//if(type==1) {addChild(submenu);}
	}
	/*Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Option 1"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Option 2"));
		menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Option 3"));
		menu->box.pos.x=10;
		return menu;
	}*/
};


/*
	0: boolean
		-button
		-menu checkbox
		-gate or toggle input

		-keyboard toggle w/ schmitt trigger

	1: discrete
		-snap knob
		-dropdown menu
		-forward/back buttons
		-cv input
		-forward/back input

		-keyboard forward/back

	2: continuous
		-smooth knob
		-slider
		-cv input

*/



struct MultiselectParamQuantity;

struct ComputerscareMenuParamModule : ComputerscarePolyModule {
	std::vector<ParamAndType*> paramList;
	std::map<int, ParamAndType*> pidMap;
	ParamQuantity* pq;
	void configMenuParam(int paramId, float minValue, float maxValue, float defaultValue, std::string label = "", int controlType = 2, std::string unit = "", float displayBase = 0.f, float displayMultiplier = 1.f, float displayOffset = 0.f) {

		configParam(paramId, minValue, maxValue, defaultValue, label, unit, displayBase, displayMultiplier, displayOffset);
		pq = paramQuantities[paramId];
		ParamAndType* pt = new ParamAndType(pq, controlType);
		paramList.push_back(pt);

	}
	void configMenuParam(int paramId, float defaultValue, std::string label = "", std::vector<std::string> options = {}) {
		int size = (int) options.size();
		configParam<MultiselectParamQuantity>(paramId, 0, size - 1, defaultValue, label);
		pq = paramQuantities[paramId];
		ParamAndType* pt = new ParamAndType(pq, 2);
		paramList.push_back(pt);
		pidMap.insert({paramId, pt});
	}
	std::vector<ParamAndType*> getParamList() {
		return paramList;
	}
	std::string getOptionValue(int paramId, int index) {
		//std::vector<std::string> *options = pidMap.find(paramId);
		//return pidMap.find(paramId).at(index);
		return "charles";
	}
};

struct MultiselectParamQuantity : ParamQuantity {
	ComputerscareMenuParamModule* module;
	std::string getDisplayValueString() override {
		int index = Quantity::getValue();
		return module->getOptionValue(paramId, index);
	}
};
struct MenuParamModuleWidget : ModuleWidget {
	MenuParamModuleWidget() {
		//DEBUG("Im inside MenuParamModuleWidget dontcha know");
	}
	/*std::vector<ParamAndType> *myListOParams;
	MenuParamModuleWidget(std::vector<ParamAndType> *paramList) {
		myListOParams = paramList;
	}
	void initMyStuff(widget::OpaqueWidget* parent) {
		for (unsigned int i = 0; i < myListOParams.size(); i++) {
			MenuParam* speedParam = new MenuParam(*myListOParams[i]);
			parent->addChild(speedParam);
		}
	}*/
};

struct ParamSettingItem : MenuItem {
	int myVal;
	Param* myParam;
	ParamSettingItem(int val, Param *param) {
		myVal = val;
		myParam = param;
	}
	void onAction(const event::Action &e) override {
		myParam->setValue(myVal);
	}
	void step() override {
		rightText = CHECKMARK(myParam->getValue() == myVal);
		MenuItem::step();
	}
};





