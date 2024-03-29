#include <string>
#include <sstream>
#include <iomanip>

#include "Computerscare.hpp"
#include "dtpulse.hpp"


struct ComputerscareColdRide;
struct LaundryTextField;
struct LaundryTF2;
struct LaundrySmallDisplay;
struct ComputerscareColdRideWidget;

const int numFields = 6;

std::string randomColdRideFormula() {
  std::string mainlookup = "111111111111111111111111112222333333344444444444444445556667778888888888888999";
  std::string string = "";
  std::string randchar = "";
  std::vector<std::string> atLookup = {"4", "8", "12", "16", "24", "32", "36", "48", "60", "64", "120", "128"};
  int length = 0;

  length = floor(random::uniform() * 12) + 1;
  string = "";
  for (int j = 0; j < length; j++) {
    randchar = mainlookup[floor(random::uniform() * mainlookup.size())];
    string = string + randchar;
    if (random::uniform() < 0.2) {
      string += "?";
    }
  }
  if (random::uniform() < 0.5) {
    if (random::uniform() < 0.8) {
      string = "[" + string.insert(floor(random::uniform() * (string.size() - 1) + 1), ",") + "]";
    }
    string += "@" + atLookup[floor(random::uniform() * atLookup.size())];
  }
  return string;
}

struct ComputerscareColdRide : Module {


  enum ParamIds {
    MANUAL_CLOCK_PARAM,
    MANUAL_RESET_PARAM,
    INDIVIDUAL_RESET_PARAM,
    NUM_PARAMS = INDIVIDUAL_RESET_PARAM + numFields
  };
  enum InputIds {
    GLOBAL_CLOCK_INPUT,
    GLOBAL_RESET_INPUT,
    CLOCK_INPUT,
    RESET_INPUT = CLOCK_INPUT + numFields,
    NUM_INPUTS = RESET_INPUT + numFields
  };
  enum OutputIds {
    TRG_OUTPUT,
    FIRST_STEP_OUTPUT = TRG_OUTPUT + numFields,
    NUM_OUTPUTS = FIRST_STEP_OUTPUT + numFields
  };
  enum LightIds {
    SWITCH_LIGHTS,
    NUM_LIGHTS
  };

  constexpr static size_t HISTORY_SIZE = 1 << 21;
  dsp::DoubleRingBuffer<float, HISTORY_SIZE> historyBuffer;


  rack::dsp::SchmittTrigger globalClockTrigger;
  rack::dsp::SchmittTrigger globalResetTriggerInput;

  rack::dsp::SchmittTrigger globalManualClockTrigger;
  rack::dsp::SchmittTrigger globalManualResetTrigger;

  rack::dsp::SchmittTrigger clockTriggers[numFields];
  rack::dsp::SchmittTrigger resetTriggers[numFields];

  LaundryTF2* textFields[numFields];
  LaundrySmallDisplay* smallLetterDisplays[numFields];

  std::string currentFormula[numFields];
  std::string currentTextFieldValue[numFields];
  std::string upcomingFormula[numFields];

  rack::dsp::SchmittTrigger manualResetTriggers[numFields];

  LaundryPoly laundryPoly[numFields];

  int checkCounter = 0;
  int checkCounterLimit = 10000;

  int channelCountEnum[numFields];
  int channelCount[numFields];

  bool activePolyStep[numFields][16] = {{false}};


  bool shouldChange[numFields] = {false};
  bool changeImminent[numFields] = {false};
  bool manualSet[numFields];
  bool inError[numFields];

  bool jsonLoaded = false;
  bool laundryInitialized = false;
  int sampleCounter = 0;

 


  ComputerscareColdRide() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    for (int i = 0; i < numFields; i++) {
      manualSet[i] = false;
      inError[i] = false;

      setNextAbsoluteSequence(i);
      checkIfShouldChange(i);
      resetOneOfThem(i);

      std::string rowi = std::to_string(i + 1);

      configButton(INDIVIDUAL_RESET_PARAM + i, "Reset Row " + rowi );

      configInput(CLOCK_INPUT + i, "Row " + rowi + " Clock");
      configInput(RESET_INPUT + i, "Row " + rowi + " Reset");

      configOutput(TRG_OUTPUT + i, "Row " + rowi + " Trigger");
      configOutput(FIRST_STEP_OUTPUT + i, "Row " + rowi + " End of Cycle");

      LaundryPoly lp = LaundryPoly(currentFormula[i]);
      laundryPoly[i] = lp;
      channelCountEnum[i] = -1;
      channelCount[i] = 1;
    }

    configButton(MANUAL_CLOCK_PARAM, "Manual Clock Advance");
    configButton(MANUAL_RESET_PARAM, "Manual Reset");

    configInput(GLOBAL_CLOCK_INPUT, "Global Clock");
    configInput(GLOBAL_RESET_INPUT, "Global Reset");
  }
  json_t *dataToJson() override {
    json_t *rootJ = json_object();

    json_t *sequencesJ = json_array();
    json_t *channelCountJ = json_array();
    for (int i = 0; i < numFields; i++) {
      json_t *sequenceJ = json_string(currentTextFieldValue[i].c_str());
      json_array_append_new(sequencesJ, sequenceJ);
      json_t *channelJ = json_integer(channelCountEnum[i]);
      json_array_append_new(channelCountJ, channelJ);
    }
    json_object_set_new(rootJ, "sequences", sequencesJ);
    json_object_set_new(rootJ, "channelCount", channelCountJ);

    return rootJ;
  }

  void dataFromJson(json_t *rootJ) override {
    std::string val;
    int count;
    json_t *sequencesJ = json_object_get(rootJ, "sequences");
    if (sequencesJ) {
      for (int i = 0; i < numFields; i++) {

        json_t *sequenceJ = json_array_get(sequencesJ, i);
        if (sequenceJ)
          val = json_string_value(sequenceJ);
        // currentFormula[i] = val;
        //currentTextFieldValue[i] = val;
        currentTextFieldValue[i] = val;
        // upcomingFormula[i]=val;
        manualSet[i] = true;
      }
    }
    else {
      json_t *textJLegacy = json_object_get(rootJ, "data");
      if (textJLegacy) {
        json_t *seqJLegacy = json_object_get(textJLegacy, "sequences");

        if (seqJLegacy) {
          for (int i = 0; i < numFields; i++) {
            json_t *sequenceJ = json_array_get(seqJLegacy, i);
            if (sequenceJ)
              val = json_string_value(sequenceJ);
            // currentFormula[i] = val;
            //lastValue[i] = val;
            currentTextFieldValue[i] = val;
            //upcomingFormula[i]=val;
            manualSet[i] = true;

          }
        }
      }
    }
    json_t *channelCountEnumJ = json_object_get(rootJ, "channelCount");
    if (channelCountEnumJ) {
      for (int i = 0; i < numFields; i++) {
        json_t *countJ = json_array_get(channelCountEnumJ, i);
        if (countJ) {
          count = json_integer_value(countJ);
          channelCountEnum[i] = count;
        }
      }
    }
    jsonLoaded = true;

  }

  int counter = 0;
  float fifo[32] =  {0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f};

  int last_out_a = 0;


  void process(const ProcessArgs &args) override{
    int numSteps = 21;
    counter++;
    counter = counter % numSteps;
    

    float dry = inputs[GLOBAL_CLOCK_INPUT].getVoltage();

    fifo[counter] = dry;

    int bufferIndex = (counter + numSteps +1) % numSteps;
    bufferIndex = 0;
    //DEBUG("bufferIndex %i",bufferIndex);

    outputs[TRG_OUTPUT ].setVoltage(fifo[bufferIndex]);
   // outputs[TRG_OUTPUT ].setVoltage(fifo[0][0]);


  }

  void onAdd() override {
  }

  void checkTextField(int channel) {
    std::string textFieldValue = currentTextFieldValue[channel];

    if (textFieldValue != currentFormula[channel] && textFieldValue != upcomingFormula[channel]) {

      LaundryPoly lp = LaundryPoly(textFieldValue);
      if (!lp.inError && matchParens(textFieldValue)) {
        upcomingFormula[channel] = textFieldValue;
        setNextAbsoluteSequence(channel);
        inError[channel] = false;
      }
      else {
        DEBUG("Channel %i in error", channel);
        inError[channel] = true;
      }
    }

  }

  void onRandomize() override {
    randomizeAllFields();
  }
  void randomizeAllFields() {
    for (int i = 0; i < numFields; i++) {
      randomizeAField(i);
    }
  }

  void randomizeAField(int i) {
    currentTextFieldValue[i] = randomColdRideFormula();
    manualSet[i] = true;
    setNextAbsoluteSequence(i);


  }

  void setNextAbsoluteSequence(int index) {
    shouldChange[index] = true;
  }
  void checkChannelCount(int index) {
    if (channelCountEnum[index] == -1) {
      if (currentFormula[index].find("#") != std::string::npos) {
        channelCount[index] = 16;
      } else {
        size_t n = std::count(currentFormula[index].begin(), currentFormula[index].end(), ';');
        channelCount[index] = std::min((int)n + 1, 16);
      }
    } else {
      channelCount[index] = channelCountEnum[index];
    }
  }
  void setAbsoluteSequenceFromQueue(int index) {
    laundryPoly[index] = LaundryPoly(upcomingFormula[index]);
    currentFormula[index] = upcomingFormula[index];
    checkChannelCount(index);
    if (laundryPoly[index].inError) {
      DEBUG("ERROR ch:%i", index);
    }
  }
  void checkIfShouldChange(int index) {
    if (shouldChange[index]) {
      setAbsoluteSequenceFromQueue(index);
      shouldChange[index] = false;
    }
  }

  /*
  lets say the sequence "332" is entered in the 0th (first)
  numSteps[0] would then be 8 (3 + 3 + 2)
  absoluteSequences[0] would be the vector (1,0,0,1,0,0,1,0)
  absoluteStep[0] would run from 0 to 7

  332-4 (332 offset by 4)

  */
  void incrementInternalStep(int i) {
    for (int ch = 0; ch < 16; ch++) {
      laundryPoly[i].lss[ch].incrementAndCheck();
      if (laundryPoly[i].lss[laundryPoly[i].maxIndex].readHead == 0) {
        this->setChangeImminent(i, false);
      }
    }
  }
  std::string getDisplayString(int index) {
    LaundryPoly loopy = this->laundryPoly[index];

    std::string lhs = std::to_string(loopy.lss[loopy.maxIndex].readHead + 1);
    std::string rhs =  std::to_string(loopy.lss[loopy.maxIndex].numSteps);

    padTo(lhs, 3, ' ');
    padTo(rhs, 3, ' ');

    std::string val = lhs + "\n" + rhs;

    return val;
  }
  void setChangeImminent(int i, bool value) {
    changeImminent[i] = value;
  }
  void resetOneOfThem(int i) {
    for (int ch = 0; ch < 16; ch++) {
      laundryPoly[i].lss[ch].readHead = -1;
    }
  }
};




struct LaundryTF2 : ComputerscareTextField
{
  ComputerscareColdRide *module;
  //int fontSize = 16;
  int rowIndex = 0;

  LaundryTF2(int i)
  {
    rowIndex = i;
    ComputerscareTextField();
  };


  void draw(const DrawArgs &args) override
  {
    if (module)
    {
      if (module->manualSet[rowIndex]) {

        text = module->currentTextFieldValue[rowIndex];
        module->manualSet[rowIndex] = false;
      }
      std::string value = text.c_str();
      module->currentTextFieldValue[rowIndex] = value;
      inError = module->inError[rowIndex];
      /* if (value != module->currentFormula[rowIndex] )
       {
         module->currentTextFieldValue[rowIndex] = value;
         inError = true;
       }
       else {
         inError = false;
       }*/
    }
    else {
      text = randomColdRideFormula();
    }

    ComputerscareTextField::draw(args);
  }


  //void draw(const DrawArgs &args) override;
  //int getTextPosition(math::Vec mousePos) override;
};

struct LaundrySmallDisplay : SmallLetterDisplay
{
  ComputerscareColdRide *module;
  int type;
  int index;
  LaundrySmallDisplay(int i)
  {
    index = i;
    SmallLetterDisplay();
  };
  void draw(const DrawArgs &args)
  {
    //this->setNumDivisionsString();
    if (module)
    {
      value = module->getDisplayString(index);
      blink = module->shouldChange[index];
      doubleblink = module->changeImminent[index];
      SmallLetterDisplay::draw(args);
    }
  }

};

struct LaundryChannelItem : MenuItem {
  ComputerscareColdRide *module;
  int channels;
  int row;
  void onAction(const event::Action &e) override {
    if (row > -1) {
      module->channelCountEnum[row] = channels;
    } else {
      for (int i = 0; i < numFields; i++) {
        module->channelCountEnum[i] = channels;
      }
    }
  }
};


struct LaundryChannelsItem : MenuItem {
  ComputerscareColdRide *module;
  int row;
  Menu *createChildMenu() override {
    Menu *menu = new Menu;
    for (int channels = -1; channels <= 16; channels++) {
      LaundryChannelItem *item = new LaundryChannelItem;
      item->row = row;
      if (channels < 0)
        item->text = "Automatic";
      else
        item->text = string::f("%d", channels);
      if (row > -1) {
        item->rightText = CHECKMARK(module->channelCountEnum[row] == channels);
      }
      item->module = module;
      item->channels = channels;
      menu->addChild(item);
    }
    return menu;
  }
};


struct ComputerscareColdRideWidget : ModuleWidget {

  double verticalSpacing = 18.4;
  int verticalStart = 22;
  ComputerscareColdRideWidget(ComputerscareColdRide *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ComputerscareColdRidePanel.svg")));

    //global clock input
    addInput(createInput<InPort>(mm2px(Vec(2 , 0)),  module, ComputerscareColdRide::GLOBAL_CLOCK_INPUT));

    //global reset input
    addInput(createInput<InPort>(mm2px(Vec(12 , 0)),  module, ComputerscareColdRide::GLOBAL_RESET_INPUT));

    //momentary clock and reset buttons
    addParam(createParam<ComputerscareClockButton>(mm2px(Vec(2 , 8)), module, ComputerscareColdRide::MANUAL_CLOCK_PARAM));
    addParam(createParam<ComputerscareResetButton>(mm2px(Vec(12 , 8)), module, ComputerscareColdRide::MANUAL_RESET_PARAM));

    for (int i = 0; i < numFields; i++) {
      //first-step output
      addOutput(createOutput<OutPort>(mm2px(Vec(42 , verticalStart + verticalSpacing * i - 11)),  module, ComputerscareColdRide::FIRST_STEP_OUTPUT + i));

      //individual output
      addOutput(createOutput<OutPort>(mm2px(Vec(54 , verticalStart + verticalSpacing * i - 11)),  module, ComputerscareColdRide::TRG_OUTPUT + i));

      //individual clock input
      addInput(createInput<InPort>(mm2px(Vec(2, verticalStart + verticalSpacing * i - 10)),  module, ComputerscareColdRide::CLOCK_INPUT + i));

      //individual reset input
      addInput(createInput<InPort>(mm2px(Vec(12, verticalStart + verticalSpacing * i - 10)),  module, ComputerscareColdRide::RESET_INPUT + i));

      textFieldTemp = new LaundryTF2(i);
      textFieldTemp->box.pos = mm2px(Vec(1, verticalStart + verticalSpacing * i));
      textFieldTemp->module = module;
      textFieldTemp->box.size = mm2px(Vec(64, 7));
      textFieldTemp->multiline = false;
      textFieldTemp->color = nvgRGB(0xC0, 0xE7, 0xDE);
      textFieldTemp->text = "";

      laundryTextFields[i] = textFieldTemp;
      addChild(textFieldTemp);


      // // active / total steps display
      // smallLetterDisplay = new LaundrySmallDisplay(i);
      // smallLetterDisplay->box.pos = mm2px(Vec(22, verticalStart - 9.2 + verticalSpacing * i));
      // smallLetterDisplay->box.size = Vec(60, 30);
      // smallLetterDisplay->value = std::to_string(3);
      // smallLetterDisplay->baseColor = COLOR_COMPUTERSCARE_LIGHT_GREEN;
      // smallLetterDisplay->module = module;
      // addChild(smallLetterDisplay);
      // laundrySmallDisplays[i] = smallLetterDisplay;

      addParam(createParam<ComputerscareInvisibleButton>(mm2px(Vec(20, verticalStart - 9.2 + verticalSpacing * i)), module, ComputerscareColdRide::INDIVIDUAL_RESET_PARAM + i));

    }
    laundry = module;
  }

  void appendContextMenu(Menu *menu) override {
    ComputerscareColdRide *module = dynamic_cast<ComputerscareColdRide*>(this->laundry);

    menu->addChild(new MenuEntry);



    for (int i = -1; i < numFields; i++) {
      LaundryChannelsItem *channelsItem = new LaundryChannelsItem;
      channelsItem->text = i == -1 ? "Set All Channels Polyphony" : string::f("Channel %d Polyphony", i + 1);;
      channelsItem->rightText = RIGHT_ARROW;
      channelsItem->module = module;
      channelsItem->row = i;
      menu->addChild(channelsItem);

      if (i == -1) {
        MenuLabel *spacerLabel = new MenuLabel();
        menu->addChild(spacerLabel);
      }
    }
  }
  /*This is a deprecated method, but since I used ModuleWidget::toJson to save the custom sequences,
      old patches have "sequences" at the root of the JSON serialization.  Module::dataFromJSON does not provide
      the root object, just the "data" key, so this is the only way to get the sequences from patches prior to v1.2

      */
  /* void fromJson(json_t *rootJ) override
   {

     std::string val;
     ModuleWidget::fromJson(rootJ);

     json_t *seqJLegacy = json_object_get(rootJ, "sequences");
     if (seqJLegacy) {
       for (int i = 0; i < numFields; i++) {
         json_t *sequenceJ = json_array_get(seqJLegacy, i);
         if (sequenceJ) {
           val = json_string_value(sequenceJ);
           laundry->currentTextFieldValue[i] = val;
           laundry->manualSet[i] = true;
         }

       }
       laundry->jsonLoaded = true;
     }


   }*/
  ComputerscareColdRide *laundry;

  LaundryTF2 *textFieldTemp;
  LaundryTF2 *laundryTextFields[numFields];
  LaundrySmallDisplay* smallLetterDisplay;
  LaundrySmallDisplay* laundrySmallDisplays[numFields];

};

Model *modelComputerscareColdRide = createModel<ComputerscareColdRide, ComputerscareColdRideWidget>("computerscare-cold-ride");
