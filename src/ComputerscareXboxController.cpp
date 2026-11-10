#include "Controller/Widget.hpp"
struct ComputerscareXboxController : ComputerscareController {
  ComputerscareXboxController() : ComputerscareController(cc::Xbox) {}
};
struct ComputerscareXboxControllerWidget : ComputerscareControllerWidget {
  ComputerscareXboxControllerWidget(ComputerscareXboxController* m)
      : ComputerscareControllerWidget(m, cc::Xbox) {}
};
Model* modelComputerscareXboxController =
    createModel<ComputerscareXboxController, ComputerscareXboxControllerWidget>(
        "computerscare-controller-xbox");
