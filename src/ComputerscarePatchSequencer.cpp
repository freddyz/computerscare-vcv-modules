#include "Computerscare.hpp"

#include <string>
#include <sstream>
#include <iomanip>

const int maxSteps = 16;
const int numInputs = 10;
const int numOutputs = 10;

struct ComputerscareDebug;

struct ComputerscarePatchSequencer : Module {
  enum ParamIds {
    STEPS_PARAM,
    MANUAL_CLOCK_PARAM,
    EDIT_PARAM,
    EDIT_PREV_PARAM,
    ENUMS(SWITCHES, 100),
    RESET_PARAM,
    NUM_PARAMS
  };
  enum InputIds {
    TRG_INPUT,
    ENUMS(INPUT_JACKS, 10),
    RANDOMIZE_INPUT,
    RESET_INPUT,
    NUM_INPUTS
  };
  enum OutputIds {
    OUTPUTS,
    NUM_OUTPUTS = OUTPUTS + 10
  };
  enum LightIds {
    SWITCH_LIGHTS,
    NUM_LIGHTS = SWITCH_LIGHTS + 200
  };

  rack::dsp::SchmittTrigger switch_triggers[10][10];

  rack::dsp::SchmittTrigger nextAddressRead;
  rack::dsp::SchmittTrigger nextAddressEdit;
  rack::dsp::SchmittTrigger prevAddressEdit;
  rack::dsp::SchmittTrigger clockTrigger;
  rack::dsp::SchmittTrigger randomizeTrigger;
  rack::dsp::SchmittTrigger resetTriggerInput;
  rack::dsp::SchmittTrigger resetTriggerButton;

  int address = 0;
  int editAddress = 0;
  int addressPlusOne = 1;
  int editAddressPlusOne = 1;
  int counter = 513;

  int numAddresses = 2;
  bool switch_states[maxSteps][10][10] = {};

  bool onlyRandomizeActive = true;

  float input_values[numInputs * 16] = {0.0};
  float sums[numOutputs * 16] = {0.0};

  int randomizationStepEnum = 0; //0: edit step, 1: active step, 2: all steps
  int randomizationOutputBoundsEnum = 1; //0: randomize exactly one per output, 1: randomize exactly one per output, 2: randomize 1 or more, 3: randomize 0 or more

  int channelCount[numOutputs];
  int channelCountEnum = -1;

  ComputerscarePatchSequencer() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(STEPS_PARAM, 1.f, 16.f, 2.0f, "Number of Steps");
    for (int i = 0; i < numOutputs; i++) {
      channelCount[i] = 0;
      configInput(INPUT_JACKS + i, "Row " + std::to_string(i + 1));
      configOutput(OUTPUTS + i, "Column " + std::to_string(i + 1));
    }

    for (int inRow = 0; inRow < numInputs; inRow++) {
      for (int outCol = 0; outCol < numOutputs; outCol++) {
        configButton(SWITCHES + outCol * numInputs + inRow, "Toggle Input Row " + std::to_string(inRow + 1) + ",Output Column " + std::to_string(outCol + 1));
      }
    }
    getParamQuantity(STEPS_PARAM)->randomizeEnabled = false;

    configButton(MANUAL_CLOCK_PARAM, "Manual Scene Advance");
    configButton(RESET_PARAM, "Reset To Scene 1");

    configButton(EDIT_PARAM, "Edit Next Scene");
    configButton(EDIT_PREV_PARAM, "Edit Previous Scene");

    configInput(TRG_INPUT, "Clock");
    configInput(RESET_INPUT, "Reset Trigger");
    configInput(RANDOMIZE_INPUT, "Randomize Trigger");

  }
  void process(const ProcessArgs &args) override;


  void updateChannelCount() {
    int currentMax;

    for (int j = 0; j < numOutputs; j++) {
      if (channelCountEnum == -1) {
        currentMax = 0;
        for (int i = 0; i < numInputs; i++) {
          if (switch_states[address][i][j] && inputs[INPUT_JACKS + i].isConnected()) {
            currentMax = std::max(currentMax, inputs[INPUT_JACKS + i].getChannels());
          }
        }
      }
      else {
        currentMax = channelCountEnum;
      }
      channelCount[j] = currentMax;
      outputs[OUTPUTS + j].setChannels(currentMax);
    }
  }

  int getRandomizationStepEnum() {
    return randomizationStepEnum;
  }

  int getRandomizationOutputBoundsEnum() {
    return randomizationOutputBoundsEnum;
  }

  void setRandomizationStepEnum(int randomizationStep) {
    randomizationStepEnum = randomizationStep;
  }
  void setRandomizationOutputBoundsEnum(int randomizationOutputBounds) {
    randomizationOutputBoundsEnum = randomizationOutputBounds;
  }

  void onRandomize() override {
    randomizePatchMatrix();
  }

  void randomizePatchMatrix()
  {
    if (onlyRandomizeActive) {
      randomizeMatrixOnlyActive();
    }
    else {
      randomizeMatrixIncludingDisconnected();
    }
  };


  // For more advanced Module features, read Rack's engine.hpp header file
  // - toJson, fromJson: serialization of internal data
  // - onSampleRateChange: event triggered by a change of sample rate
  // - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

  void randomizeMatrixOnlyActive() {
    int randomIndex;

    bool connectedInputs[10];
    bool connectedOutputs[10];
    int numConnectedInputs = 0;

    std::vector<int> connectedInputIndices;

    for (int i = 0; i < 10; i++)
    {
      if (inputs[INPUT_JACKS + i].isConnected()) {
        numConnectedInputs++;
        connectedInputIndices.push_back(i);
      }

      connectedInputs[i] = inputs[INPUT_JACKS + i].isConnected();
      connectedOutputs[i] = outputs[OUTPUTS + i].isConnected();
    }
    for (int k = 0; k < maxSteps; k++) {
      if ((randomizationStepEnum == 0 && k == editAddress) || (randomizationStepEnum == 1 && k == address) || randomizationStepEnum == 2) {
        for (int i = 0; i < 10; i++) {
          randomIndex = numConnectedInputs > 0 ? connectedInputIndices[floor(random::uniform() * numConnectedInputs)] : 0;
          if (connectedOutputs[i]) {
            for (int j = 0; j < 10; j++) {
              if (j == randomIndex)
                switch_states[k][j][i] = 1;
              else
                switch_states[k][j][i] = 0;
            }
          }
        }
      }
    }

  }

  void randomizeMatrixIncludingDisconnected() {
    int randomIndex;
    for (int k = 0; k < maxSteps; k++) {
      if ((randomizationStepEnum == 0 && k == editAddress) || (randomizationStepEnum == 1 && k == address) || randomizationStepEnum == 2) {
        for (int i = 0; i < 10; i++)
        {
          randomIndex = floor(random::uniform() * 10);

          for (int j = 0; j < 10; j++)
          {
            if (randomizationOutputBoundsEnum == 3) {
              switch_states[k][j][i] = (j == randomIndex || random::uniform() < 0.2) ? 1 : 0;
            }
            else if (randomizationOutputBoundsEnum == 2) {
              switch_states[k][j][i] = random::uniform() < 0.2 ? 1 : 0;
            }
            else if (randomizationOutputBoundsEnum == 0) {
              switch_states[k][j][i] = (j == randomIndex && random::uniform() < 0.7) ? 1 : 0;
            }
            else {
              switch_states[k][j][i] = j == randomIndex ? 1 : 0;
            }
          }
        }
      }
    }

  }

  void onReset() override
  {
    for (int k = 0; k < maxSteps; k++) {


      for (int i = 0; i < 10; i++)
      {
        for (int j = 0; j < 10; j++)
        {
          switch_states[k][i][j] =  0;
        }
      }
    }
  }; // end randomize()


  void dataFromJson(json_t *rootJ) override {
    // button states
    json_t *button_statesJ = json_object_get(rootJ, "buttons");
    if (button_statesJ)
    {
      for (int k = 0; k < maxSteps; k++) {

        for (int i = 0; i < 10; i++) {
          for (int j = 0; j < 10; j++) {
            json_t *button_stateJ = json_array_get(button_statesJ, k * 100 + i * 10 + j);
            if (button_stateJ)
              switch_states[k][i][j] = !!json_integer_value(button_stateJ);
          }
        }
      }
    }
    json_t *onlyRandomizeActiveJ = json_object_get(rootJ, "onlyRandomizeActive");
    if (onlyRandomizeActiveJ) { onlyRandomizeActive = json_is_true(onlyRandomizeActiveJ); }

    json_t *randomizationStepEnumJ = json_object_get(rootJ, "randomizationStepEnum");
    if (randomizationStepEnumJ) { setRandomizationStepEnum(json_integer_value(randomizationStepEnumJ)); }

    json_t *channelCountEnumJ = json_object_get(rootJ, "channelCountEnum");
    if (channelCountEnumJ) { channelCountEnum = json_integer_value(channelCountEnumJ); }

    json_t *randomizationOutputBoundsEnumJ = json_object_get(rootJ, "randomizationOutputBoundsEnum");
    if (randomizationOutputBoundsEnumJ) { setRandomizationOutputBoundsEnum(json_integer_value(randomizationOutputBoundsEnumJ)); }

  }
  json_t *dataToJson() override
  {

    json_t *rootJ = json_object();
    // button states
    json_t *button_statesJ = json_array();
    for (int k = 0; k < maxSteps; k++) {
      for (int i = 0; i < 10; i++)
      {
        for (int j = 0; j < 10; j++)
        {
          json_t *button_stateJ = json_integer((int) switch_states[k][i][j]);
          json_array_append_new(button_statesJ, button_stateJ);
        }
      }
    }
    json_object_set_new(rootJ, "buttons", button_statesJ);
    json_object_set_new(rootJ, "onlyRandomizeActive", json_boolean(onlyRandomizeActive));
    json_object_set_new(rootJ, "channelCountEnum", json_integer(channelCountEnum));
    json_object_set_new(rootJ, "randomizationStepEnum", json_integer(getRandomizationStepEnum()));
    json_object_set_new(rootJ, "randomizationOutputBoundsEnum", json_integer(getRandomizationOutputBoundsEnum()));
    return rootJ;
  }
};


void ComputerscarePatchSequencer::process(const ProcessArgs &args) {

  int numStepsKnobPosition = (int) clamp(roundf(params[STEPS_PARAM].getValue()), 1.0f, 16.0f);
  //int channels[10] = {0};

  for ( int j = 0 ; j < 10 ; j++)
  {
    //channels[i] = inputs[INPUT_JACKS + i].getChannels();
    for (int c = 0; c < 16; c++) {
      sums[j * 16 + c] = 0.0;
    }


  }

  for (int i = 0 ; i < 10 ; i++)
  {
    for (int j = 0 ; j < 10 ; j++)
    {
      if (switch_triggers[i][j].process(params[SWITCHES + j * 10 + i].getValue()))
      {
        // handle button clicks in the patch matrix
        switch_states[editAddress][i][j] = !switch_states[editAddress][i][j];
      }


    }
  }
  if (counter > 512) {
    updateChannelCount();
    for (int i = 0 ; i < 10 ; i++)
    {
      for (int j = 0 ; j < 10 ; j++)
      {

        // update the green lights (step you are editing) and the red lights (current active step)
        lights[SWITCH_LIGHTS + i + j * 10].value  = (switch_states[editAddress][i][j]) ? 1.0 : 0.0;
        lights[SWITCH_LIGHTS + i + j * 10 + 100].value  = (switch_states[address][i][j]) ? 1.0 : 0.0;
      }
    }
    counter = 0;
  }
  counter++;


  if (numStepsKnobPosition != numAddresses) {
    numAddresses = numStepsKnobPosition;
  }

  if (randomizeTrigger.process(inputs[RANDOMIZE_INPUT].getVoltage() / 2.f)) {
    randomizePatchMatrix();
  }
  if (nextAddressEdit.process(params[EDIT_PARAM].getValue()) ) {
    editAddress = editAddress + 1;
    editAddress = editAddress % maxSteps;
  }
  if (prevAddressEdit.process(params[EDIT_PREV_PARAM].getValue()) ) {
    editAddress = editAddress - 1;
    editAddress = editAddress + maxSteps;
    editAddress = editAddress % maxSteps;
  }

  if (nextAddressRead.process(params[MANUAL_CLOCK_PARAM].getValue()) || clockTrigger.process(inputs[TRG_INPUT].getVoltage() / 2.f)) {
    numAddresses =  (int) clamp(roundf(params[STEPS_PARAM].getValue() /*+ inputs[STEPS_INPUT].getVoltage()*/), 1.0f, 16.0f);

    address = address + 1;
    address = address % numAddresses;
  }

  if (resetTriggerButton.process(params[RESET_PARAM].getValue()) || resetTriggerInput.process(inputs[RESET_INPUT].getVoltage() / 2.f)) {
    numAddresses =  (int) clamp(roundf(params[STEPS_PARAM].getValue()), 1.0f, 16.0f);

    address = 0;
  }

  addressPlusOne = address + 1;
  editAddressPlusOne = editAddress + 1;

  for (int i = 0 ; i < 10 ; i++)
  {
    for (int c = 0; c < 16; c++) {
      input_values[i * 16 + c] = inputs[INPUT_JACKS + i].getVoltage(c);
    }
  }

  for (int i = 0 ; i < 10 ; i++)
  {
    for (int j = 0 ; j < 10 ; j++)
    {
      // todo: toggle for each output of how to combine multiple active signals in a column
      // sum, average, and, or etc
      if (switch_states[address][i][j]) {
        for (int c = 0; c < channelCount[j]; c++) {
          sums[j * 16 + c] += input_values[i * 16 + c];
        }

      }
    }
  }
  /// outputs
  for (int j = 0 ; j < 10 ; j++)
  {
    //outputs[OUTPUTS + j].setChannels(16);
    for (int c = 0; c < channelCount[j]; c++) {
      outputs[OUTPUTS + j].setVoltage(sums[j * 16 + c], c);
    }
  }
}

////////////////////////////////////
struct NumberDisplayWidget3 : TransparentWidget {

  int *value;
  ComputerscarePatchSequencer *module;
  std::string fontPath = "res/digital-7.ttf";

  NumberDisplayWidget3() {

  };

  void draw(const DrawArgs &args) override
  {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x00, 0x00, 0x00);

    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.0, 0.0, box.size.x, box.size.y, 4.0);
    nvgFillColor(args.vg, backgroundColor);
    nvgFill(args.vg);

  }
  void drawLayer(const BGPanel::DrawArgs& args, int layer) override {
    if (layer == 1) {
      drawText(args);
    }
    Widget::drawLayer(args, layer);
  }
  void drawText(const BGPanel::DrawArgs& args) {
    std::shared_ptr<Font> font = APP->window->loadFont(asset::plugin(pluginInstance, fontPath));
    if (font) {
      // text
      nvgFontSize(args.vg, 13);
      nvgFontFaceId(args.vg, font->handle);
      nvgTextLetterSpacing(args.vg, 2.5);

      std::stringstream to_display;
      if (module) {
        to_display << std::setw(3) << *value;
      }
      else {
        to_display << std::setw(3) << "16";
      }

      Vec textPos = Vec(6.0f, 17.0f);
      NVGcolor textColor = nvgRGB(0xC0, 0xE7, 0xDE);
      nvgFillColor(args.vg, textColor);
      nvgText(args.vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
    }
  }
};



struct ComputerscarePatchSequencerWidget : ModuleWidget {

  ComputerscarePatchSequencerWidget(ComputerscarePatchSequencer *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscarePatchSequencerPanel.svg")));

    int top_row = 70;
    int row_spacing = 26;
    int column_spacing = 26;

    int rdx = rand() % 8;
    int rdy  = rand() % 8;

    for (int i = 0 ; i < 10 ; i++)
    {


      for (int j = 0 ; j < 10 ; j++ )
      {
        // the part you click
        addParam(createParam<LEDButton>(Vec(35 + column_spacing * j + 2, top_row + row_spacing * i + 4), module, ComputerscarePatchSequencer::SWITCHES + i + j * 10));



        // green light indicates the state of the matrix that is being edited
        //ModuleLightWidget *bigOne = ModuleLightWidget::create<ComputerscareHugeLight<ComputerscareGreenLight>>(Vec(35 + column_spacing * j +0.4, top_row + row_spacing * i +2.4 ), module, ComputerscarePatchSequencer::SWITCH_LIGHTS  + i + j * 10);
        addChild(createLight<ComputerscareHugeLight<ComputerscareGreenLight>>(Vec(35 + column_spacing * j + 0.4, top_row + row_spacing * i + 2.4 ), module, ComputerscarePatchSequencer::SWITCH_LIGHTS  + i + j * 10));



        //addParam(createParam<LEDButton>(Vec(35 + column_spacing * j+2, top_row + row_spacing * i+4), module, ComputerscarePatchSequencer::SWITCHES + i + j * 10));



        //addChild(bigOne);

        double xpos = 35 + column_spacing * j + 6.3 + rand() % 8 - 4;
        double ypos = top_row + row_spacing * i + 8.3 + rand() % 8 - 4;
        // red light indicates the state of the matrix that is the active step
        //computerscarered
        //addParam(createParam<ComputerscareSmallLight>(Vec(xpos, ypos), module, ComputerscarePatchSequencer::SWITCH_LIGHTS  + i + j * 10 + 100));
        addChild(createLight<ComputerscareSmallLight<ComputerscareRedLight>>(Vec(xpos - rdy, ypos + rdx), module, ComputerscarePatchSequencer::SWITCH_LIGHTS  + i + j * 10 + 100));

        addChild(createLight<ComputerscareSmallLight<ComputerscareRedLight>>(Vec(xpos + rdx, ypos + rdy), module, ComputerscarePatchSequencer::SWITCH_LIGHTS  + i + j * 10 + 100));

      }

      addInput(createInput<InPort>(Vec(3, i * row_spacing + top_row), module, ComputerscarePatchSequencer::INPUT_JACKS + i));

      if (i % 2) {
        addOutput(createOutput<PointingUpPentagonPort>(Vec(33 + i * column_spacing , top_row + 10 * row_spacing), module, ComputerscarePatchSequencer::OUTPUTS + i));
      }
      else {
        addOutput(createOutput<InPort>(Vec(33 + i * column_spacing , top_row + 10 * row_spacing), module, ComputerscarePatchSequencer::OUTPUTS + i));
      }
    }

    //clock input
    addInput(createInput<InPort>(Vec(24, 37), module, ComputerscarePatchSequencer::TRG_INPUT));

    //reset input
    addInput(createInput<InPort>(Vec(24, 3),  module, ComputerscarePatchSequencer::RESET_INPUT));

    //manual clock button
    addParam(createParam<LEDButton>(Vec(7 , 37), module, ComputerscarePatchSequencer::MANUAL_CLOCK_PARAM));

    //reset button
    addParam(createParam<LEDButton>(Vec(7 , 3), module, ComputerscarePatchSequencer::RESET_PARAM));

    //randomize input
    addInput(createInput<InPort>(Vec(270, 0), module, ComputerscarePatchSequencer::RANDOMIZE_INPUT));

    //active step display
    NumberDisplayWidget3 *display = new NumberDisplayWidget3();
    display->box.pos = Vec(56, 40);
    display->box.size = Vec(50, 20);
    display->value = &module->addressPlusOne;
    display->module = module;
    addChild(display);

    // number of steps display
    NumberDisplayWidget3 *stepsDisplay = new NumberDisplayWidget3();
    stepsDisplay->box.pos = Vec(150, 40);
    stepsDisplay->box.size = Vec(50, 20);
    stepsDisplay->module = module;
    stepsDisplay->value = &module->numAddresses;
    addChild(stepsDisplay);

    //number-of-steps dial.   Discrete, 16 positions
    ParamWidget* stepsKnob =  createParam<LrgKnob>(Vec(108, 30), module, ComputerscarePatchSequencer::STEPS_PARAM);
    addParam(stepsKnob);

    //editAddressNext button
    addParam(createParam<LEDButton>(Vec(227 , 41), module, ComputerscarePatchSequencer::EDIT_PARAM));

    //editAddressPrevious button
    addParam(createParam<LEDButton>(Vec(208 , 41), module, ComputerscarePatchSequencer::EDIT_PREV_PARAM));

    // currently editing step #:
    NumberDisplayWidget3 *displayEdit = new NumberDisplayWidget3();
    displayEdit->box.pos = Vec(246, 40);
    displayEdit->box.size = Vec(50, 20);
    displayEdit->module = module;
    displayEdit->value = &module->editAddressPlusOne;
    addChild(displayEdit);
    fatherSon = module;
  }


  /* void fromJson(json_t *rootJ) override
   {
     ModuleWidget::fromJson(rootJ);
     json_t *button_statesJ = json_object_get(rootJ, "buttons");
     if (button_statesJ) {
       //there be legacy JSON
       fatherSon->dataFromJson(rootJ);
     }
   }*/
  void appendContextMenu(Menu *menu) override;

  ComputerscarePatchSequencer *fatherSon;
};
struct OnlyRandomizeActiveMenuItem : MenuItem {
  ComputerscarePatchSequencer *patchSequencer;
  OnlyRandomizeActiveMenuItem() {

  }
  void onAction(const event::Action &e) override {
    patchSequencer->onlyRandomizeActive = !patchSequencer->onlyRandomizeActive;
  }
  void step() override {
    rightText = patchSequencer->onlyRandomizeActive ? "✔" : "";
    MenuItem::step();
  }
};
struct WhichStepToRandomizeItem : MenuItem {
  ComputerscarePatchSequencer *patchSequencer;
  int stepEnum;
  void onAction(const event::Action &e) override {
    patchSequencer->setRandomizationStepEnum(stepEnum);
  }
  void step() override {
    rightText = CHECKMARK(patchSequencer->getRandomizationStepEnum() == stepEnum);
    MenuItem::step();
  }
};

struct WhichRandomizationOutputBoundsItem : MenuItem {
  ComputerscarePatchSequencer *patchSequencer;
  int boundsEnum;
  void onAction(const event::Action &e) override {
    patchSequencer->setRandomizationOutputBoundsEnum(boundsEnum);
  }
  void step() override {
    rightText = CHECKMARK(patchSequencer->getRandomizationOutputBoundsEnum() == boundsEnum);
    MenuItem::step();
  }
};

struct FatherSonChannelItem : MenuItem {
  ComputerscarePatchSequencer *module;
  int channels;
  void onAction(const event::Action &e) override {
    module->channelCountEnum = channels;
  }
};


struct FatherSonChannelsItem : MenuItem {
  ComputerscarePatchSequencer *module;
  Menu *createChildMenu() override {
    Menu *menu = new Menu;
    for (int channels = -1; channels <= 16; channels++) {
      FatherSonChannelItem *item = new FatherSonChannelItem;
      if (channels < 0) {
        item->text = "Automatic";
      }
      else {
        item->text = string::f("%d", channels);
      }
      item->rightText = CHECKMARK(module->channelCountEnum == channels);
      item->module = module;
      item->channels = channels;
      menu->addChild(item);
    }
    return menu;
  }
};

void ComputerscarePatchSequencerWidget::appendContextMenu(Menu *menu)
{
  ComputerscarePatchSequencer *patchSequencer = dynamic_cast<ComputerscarePatchSequencer *>(this->module);

  MenuLabel *spacerLabel = new MenuLabel();
  menu->addChild(spacerLabel);

  FatherSonChannelsItem *channelsItem = new FatherSonChannelsItem;
  channelsItem->text = "Output Polyphony";
  channelsItem->rightText = RIGHT_ARROW;
  channelsItem->module = patchSequencer;
  menu->addChild(channelsItem);

  menu->addChild(new MenuEntry);


  MenuLabel *modeLabel = new MenuLabel();
  modeLabel->text = "Randomization Options";
  menu->addChild(modeLabel);


  OnlyRandomizeActiveMenuItem *onlyRandomizeActiveMenuItem = new OnlyRandomizeActiveMenuItem();
  onlyRandomizeActiveMenuItem->text = "Only Randomize Active Connections";
  onlyRandomizeActiveMenuItem->patchSequencer = patchSequencer;
  menu->addChild(onlyRandomizeActiveMenuItem);

  menu->addChild(construct<MenuLabel>());
  menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Which Step to Randomize"));
  menu->addChild(construct<WhichStepToRandomizeItem>(&MenuItem::text, "Edit step", &WhichStepToRandomizeItem::patchSequencer, patchSequencer, &WhichStepToRandomizeItem::stepEnum, 0));
  menu->addChild(construct<WhichStepToRandomizeItem>(&MenuItem::text, "Active step", &WhichStepToRandomizeItem::patchSequencer, patchSequencer, &WhichStepToRandomizeItem::stepEnum, 1));
  menu->addChild(construct<WhichStepToRandomizeItem>(&MenuItem::text, "All steps", &WhichStepToRandomizeItem::patchSequencer, patchSequencer, &WhichStepToRandomizeItem::stepEnum, 2));


  // randomization output bounds
  menu->addChild(construct<MenuLabel>());
  menu->addChild(construct<MenuLabel>(&MenuLabel::text, "Output Row Randomization Method"));
  menu->addChild(construct<WhichRandomizationOutputBoundsItem>(&MenuItem::text, "One or none", &WhichRandomizationOutputBoundsItem::patchSequencer, patchSequencer, &WhichRandomizationOutputBoundsItem::boundsEnum, 0));
  menu->addChild(construct<WhichRandomizationOutputBoundsItem>(&MenuItem::text, "Exactly one", &WhichRandomizationOutputBoundsItem::patchSequencer, patchSequencer, &WhichRandomizationOutputBoundsItem::boundsEnum, 1));
  menu->addChild(construct<WhichRandomizationOutputBoundsItem>(&MenuItem::text, "Zero or more", &WhichRandomizationOutputBoundsItem::patchSequencer, patchSequencer, &WhichRandomizationOutputBoundsItem::boundsEnum, 2));
  menu->addChild(construct<WhichRandomizationOutputBoundsItem>(&MenuItem::text, "One or more", &WhichRandomizationOutputBoundsItem::patchSequencer, patchSequencer, &WhichRandomizationOutputBoundsItem::boundsEnum, 3));

}

Model *modelComputerscarePatchSequencer = createModel<ComputerscarePatchSequencer, ComputerscarePatchSequencerWidget>("computerscare-fatherandson");

