#pragma once

#include "../ModifierMouseControl.hpp"
#include "PortaloofModule.hpp"

struct PortaloofMouseControl {
  using ControlTab = ComputerscareMouseControlTab;

  ComputerscareModifierMouseControl state;

  static engine::ParamQuantity* getParam(ComputerscarePortaloof* module,
                                         int paramId) {
    if (!module || paramId < 0 || paramId >= ComputerscarePortaloof::NUM_PARAMS)
      return nullptr;
    return module->paramQuantities[paramId];
  }

  static float getParamValue(ComputerscarePortaloof* module, int paramId) {
    engine::ParamQuantity* pq = getParam(module, paramId);
    return pq ? pq->getValue() : 0.f;
  }

  bool active(Widget* owner) const { return state.shouldControl(owner); }

  void onHover(const event::Hover& e) { state.onHover(e); }

  void onLeave(const event::Leave& e) { state.onLeave(e); }

  bool onHoverKey(Widget* owner, const event::HoverKey& e) {
    return state.onHoverKey(owner, e);
  }

  bool onButton(Widget* owner, ComputerscarePortaloof* module,
                const event::Button& e, math::Rect controlRect) {
    if (!module) return false;
    if (!controlRect.contains(e.pos)) return false;
    if (state.beginDrag(
            owner, e,
            getParamValue(module, ComputerscarePortaloof::TRANS_X_KNOB),
            getParamValue(module, ComputerscarePortaloof::TRANS_Y_KNOB))) {
      return true;
    }
    return state.endDrag(owner, e);
  }

  bool onDragMove(Widget* owner, ComputerscarePortaloof* module,
                  const event::DragMove& e, math::Vec displaySize) {
    if (!module || !state.editing) return false;
    if (!state.shouldControl(owner)) {
      e.consume(owner);
      return true;
    }
    if (displaySize.x <= 1.f || displaySize.y <= 1.f) {
      e.consume(owner);
      return true;
    }

    float modSpeed = ComputerscareModifierMouseControl::getRackModSpeed();
    float dx = e.mouseDelta.x / displaySize.x * modSpeed;
    float dy = e.mouseDelta.y / displaySize.y * modSpeed;
    engine::ParamQuantity* transX =
        getParam(module, ComputerscarePortaloof::TRANS_X_KNOB);
    engine::ParamQuantity* transY =
        getParam(module, ComputerscarePortaloof::TRANS_Y_KNOB);
    ComputerscareModifierMouseControl::setParamValue(
        transX, transX ? transX->getValue() + dx : 0.f);
    ComputerscareModifierMouseControl::setParamValue(
        transY, transY ? transY->getValue() + dy : 0.f);
    e.consume(owner);
    return true;
  }

  bool onDragEnd(Widget* owner, const event::DragEnd& e) {
    if (!state.editing) return false;
    state.editing = false;
    e.consume(owner);
    return true;
  }

  bool onHoverScroll(Widget* owner, ComputerscarePortaloof* module,
                     const event::HoverScroll& e, math::Rect controlRect) {
    if (!module || e.isConsumed() || !state.shouldControl(owner)) return false;
    if (!controlRect.contains(e.pos)) return false;
    engine::ParamQuantity* scale =
        getParam(module, ComputerscarePortaloof::SCALE_KNOB);
    if (!scale) return false;

    float direction =
        e.scrollDelta.y != 0.f ? e.scrollDelta.y : -e.scrollDelta.x;
    if (direction == 0.f) return false;
    float modSpeed = ComputerscareModifierMouseControl::getRackModSpeed();
    float factor = std::pow(1.08f, direction * modSpeed / 8.f);
    ComputerscareModifierMouseControl::setParamValue(
        scale, scale->getValue() * factor);
    e.consume(owner);
    return true;
  }

  void drawIndicator(const Widget::DrawArgs& args, math::Vec pos,
                     Widget* owner) const {
    ComputerscareModifierMouseControl::drawEditBadge(args, pos);
  }

  void drawPanelHint(const Widget::DrawArgs& args, math::Rect panelRect,
                     Widget* owner) const {
    ComputerscareModifierMouseControl::drawPanelHint(args, panelRect, owner);
  }

  void drawControlTabs(const Widget::DrawArgs& args, float panelRight,
                       const std::vector<ControlTab>& tabs) const {
    ComputerscareModifierMouseControl::drawControlTabs(args, panelRight, tabs);
  }
};
