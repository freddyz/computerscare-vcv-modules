#include "ComputerscareMetarbo.hpp"
#include "MetarboIOView.hpp"
#include "MetarboControlsIOView.hpp"
#include "MetarboFullView.hpp"

// View mode switch button - clickable to cycle, also in right-click menu
struct MetarboViewModeSwitch : SvgSwitch {
  ComputerscareMetarbo* metarbo;

  MetarboViewModeSwitch() {
    addFrame(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/iso-3way-1.svg")));
    addFrame(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/iso-3way-2.svg")));
    addFrame(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/iso-3way-3.svg")));
    shadow->opacity = 0.f;
  }
};

struct ComputerscareMetarboWidget : ModuleWidget {
  ComputerscareMetarbo* metarboModule;
  BGPanel* bgPanel;
  MetarboSquareDisplay* squareDisplay = nullptr;
  MetarboViewMode lastViewMode = VIEW_IO_ONLY;
  bool initialized = false;

  ComputerscareMetarboWidget(ComputerscareMetarbo* module) {
    setModule(module);
    metarboModule = module;

    // Start with IO_ONLY width
    float startWidth = metarboViewWidths[VIEW_IO_ONLY] * RACK_GRID_WIDTH;
    box.size = Vec(startWidth, RACK_GRID_HEIGHT);

    // Background panel
    bgPanel = new BGPanel(COLOR_COMPUTERSCARE_LIGHT_GREEN);
    bgPanel->box.size = box.size;
    addChild(bgPanel);

    // Build initial view
    rebuildView(module ? module->viewMode : VIEW_IO_ONLY);
    initialized = true;
  }

  void clearAllPortsAndParams() {
    // Remove all inputs
    for (PortWidget* pw : getInputs()) {
      removeChild(pw);
      delete pw;
    }
    // Remove all outputs
    for (PortWidget* pw : getOutputs()) {
      removeChild(pw);
      delete pw;
    }
    // Remove all params
    for (ParamWidget* pw : getParams()) {
      removeChild(pw);
      delete pw;
    }
    // Remove square display if present
    if (squareDisplay) {
      removeChild(squareDisplay);
      delete squareDisplay;
      squareDisplay = nullptr;
    }
  }

  void rebuildView(MetarboViewMode mode) {
    clearAllPortsAndParams();

    float newWidth;

    // Add view mode switch at top - present in all views
    addParam(createParam<MetarboViewModeSwitch>(
        Vec(4, 16), metarboModule, ComputerscareMetarbo::VIEW_MODE_PARAM));

    switch (mode) {
      case VIEW_IO_ONLY:
        newWidth = MetarboIOView::getWidth();
        MetarboIOView::addPorts(this, metarboModule);
        break;

      case VIEW_CONTROLS_IO:
        newWidth = MetarboControlsIOView::getWidth();
        MetarboControlsIOView::addControls(this, metarboModule);
        MetarboControlsIOView::addPorts(this, metarboModule);
        break;

      case VIEW_FULL: {
        newWidth = MetarboFullView::getWidth();
        MetarboFullView::addControls(this, metarboModule);
        MetarboFullView::addPorts(this, metarboModule);

        // Add square display in the middle
        squareDisplay = new MetarboSquareDisplay(metarboModule);
        float squareX = 30.f + 34.f;  // after controls + inputs column
        float squareY = (RACK_GRID_HEIGHT - 150.f) / 2.f;
        squareDisplay->box.pos = Vec(squareX, squareY);
        addChild(squareDisplay);
        break;
      }

      default:
        newWidth = MetarboIOView::getWidth();
        MetarboIOView::addPorts(this, metarboModule);
        break;
    }

    // Resize module
    box.size.x = newWidth;
    bgPanel->box.size.x = newWidth;

    lastViewMode = mode;
  }

  void step() override {
    if (metarboModule) {
      MetarboViewMode currentMode = metarboModule->viewMode;
      if (currentMode != lastViewMode) {
        rebuildView(currentMode);
      }
    }
    ModuleWidget::step();
  }

  void appendContextMenu(Menu* menu) override {
    ComputerscareMetarbo* mod =
        dynamic_cast<ComputerscareMetarbo*>(this->module);
    if (!mod) return;

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("View Mode"));

    for (int i = 0; i < NUM_VIEW_MODES; i++) {
      struct ViewModeItem : MenuItem {
        ComputerscareMetarbo* module;
        int mode;
        void onAction(const event::Action& e) override {
          module->viewMode = (MetarboViewMode)mode;
          module->params[ComputerscareMetarbo::VIEW_MODE_PARAM].setValue(
              (float)mode);
        }
        void step() override {
          rightText = CHECKMARK((int)module->viewMode == mode);
          MenuItem::step();
        }
      };

      ViewModeItem* item = new ViewModeItem();
      item->text = metarboViewModeNames[i];
      item->module = mod;
      item->mode = i;
      menu->addChild(item);
    }

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("Program"));

    for (int i = 0; i < 4; i++) {
      struct ProgramItem : MenuItem {
        ComputerscareMetarbo* module;
        int program;
        void onAction(const event::Action& e) override {
          module->currentProgram = program;
        }
        void step() override {
          rightText = CHECKMARK(module->currentProgram == program);
          MenuItem::step();
        }
      };

      ProgramItem* item = new ProgramItem();
      item->text = "Program " + std::to_string(i + 1) + " (pass-through)";
      item->module = mod;
      item->program = i;
      menu->addChild(item);
    }
  }
};

Model* modelComputerscareMetarbo =
    createModel<ComputerscareMetarbo, ComputerscareMetarboWidget>(
        "computerscare-metarbo");
