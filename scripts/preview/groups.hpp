#pragma once
#include <app/CableWidget.hpp>
#include <app/RailWidget.hpp>
#include <map>
#include <set>

// Offscreen groups own their plugs instead of attaching them to the live Rack.
struct GroupCableWidget : app::CableWidget {
  void onAdd(const AddEvent& e) override { widget::Widget::onAdd(e); }
  void onRemove(const RemoveEvent& e) override { widget::Widget::onRemove(e); }
};

inline void renderModuleGroups(rack::plugin::Plugin& plugin,
                               const std::string& output, float scale) {
  using M = rack::plugin::Model;
  auto advanced = [](M* m) {
    return m == modelComputerscareVolyPector ||
           m == modelComputerscareMelyPorge || m == modelComputerscareSlolyPit;
  };
  auto basic = [](M* m) {
    return m == modelComputerscareKnolyPobs ||
           m == modelComputerscareBolyPuttons ||
           m == modelComputerscareRolyPouter ||
           m == modelComputerscareTolyPoolsV2 ||
           m == modelComputerscareSolyPequencer;
  };
  auto render = [&](const std::string& name,
                    const std::vector<std::vector<M*>>& rows,
                    bool family = false, bool patched = false,
                    const std::string& portalSource = "",
                    float portalWidth = 0.f) {
    auto group = std::unique_ptr<widget::Widget>(new widget::Widget);
    auto* rails = new app::RailWidget;
    group->addChild(rails);
    std::vector<std::vector<app::ModuleWidget*>> widgets;
    std::map<M*, app::ModuleWidget*> byModel;
    std::vector<app::CableWidget*> cables;
    struct Disconnect {
      widget::Widget* group;
      std::vector<app::CableWidget*>& cables;
      ~Disconnect() {
        for (auto* cable : cables) {
          group->removeChild(cable->inputPlug);
          group->removeChild(cable->outputPlug);
          group->removeChild(cable);
          delete cable;
        }
      }
    } disconnect{group.get(), cables};
    float width = 0.f;
    std::set<std::string> slugs;
    auto gap = [&](M* left, M* right) {
      return (!family && name.find("visual") != std::string::npos) ||
                     (advanced(left) && advanced(right))
                 ? 30.f
                 : 0.f;
    };
    for (const auto& row : rows) {
      widgets.emplace_back();
      float rowWidth = 0.f;
      M* previous = nullptr;
      for (auto* model : row) {
        model->plugin = &plugin;
        engine::Module* module = nullptr;
        if ((patched && (advanced(model) || basic(model))) ||
            model == modelComputerscareFolyPace ||
            model == modelComputerscareStolyFickPigure ||
            (model == modelComputerscarePortaloof && !portalSource.empty())) {
          module = model->createModule();
          APP->engine->addModule(module);
          if (model == modelComputerscarePortaloof) {
            auto* portal = static_cast<ComputerscarePortaloof*>(module);
            portal->width = portalWidth > 0 ? portalWidth : 300.f;
            portal->clearSource(0);
            portal->clearSource(1);
            portal->params[ComputerscarePortaloof::INPUT_SOURCE_MIX].setValue(
                1.f);
            portal->params[ComputerscarePortaloof::ROT_TOGGLE].setValue(1.f);
            portal->params[ComputerscarePortaloof::ROT_KNOB].setValue(-12.f);
            portal->params[ComputerscarePortaloof::SCALE_KNOB].setValue(.65f);
            portal->params[ComputerscarePortaloof::KALEIDO_TOGGLE].setValue(
                0.f);
            portal->params[ComputerscarePortaloof::KALEIDO_KNOB].setValue(2.f);
            engine::Module::ProcessArgs args;
            args.sampleRate = 48000;
            args.sampleTime = 1.f / 48000;
            args.frame = 0;
            portal->process(args);
          } else if (model == modelComputerscareFolyPace ||
                     model == modelComputerscareStolyFickPigure) {
            // Fixed offline CV samples keep the scene identical in the source
            // capture and its Portaloof transformation; no visual cables.
            module->params[1].setValue(1.f);
            module->params[2].setValue(0.f);
            module->inputs[0].setChannels(16);
            for (int ch = 0; ch < 16; ++ch)
              module->inputs[0].setVoltage(3.f + random::uniform() * 4.f, ch);
            if (model == modelComputerscareFolyPace) {
              // Keep the face clear of its title and footer in the group shot.
              const float values[] = {-2.4177f, 0.f,    -7.f,   2.26548f,
                                      4.f,      5.484f, -1.365f};
              for (int ch = 0; ch < 7; ++ch)
                module->inputs[0].setVoltage(values[ch], ch);
            }
            engine::Module::ProcessArgs args;
            args.sampleRate = 48000;
            args.sampleTime = 1.f / 48000;
            for (int frame = 0; frame < 1024; ++frame) {
              args.frame = frame;
              module->process(args);
            }
          } else {
            module->onRandomize(engine::Module::RandomizeEvent{});
            for (auto* quantity : module->paramQuantities)
              if (quantity &&
                  quantity->name.find("Poly Channels") != std::string::npos)
                quantity->setValue(12.f);
          }
        }
        auto* widget = model->createModuleWidget(module);
        widget->step();
        if (!slugs.insert(model->slug).second)
          throw std::runtime_error("Duplicate module in group: " + model->slug);
        if (previous) rowWidth += gap(previous, model);
        widgets.back().push_back(widget);
        byModel[model] = widget;
        rowWidth += widget->box.size.x;
        previous = model;
      }
      width = std::max(width, rowWidth);
    }
    group->box.size = Vec(width + 30.f, rows.size() * 380.f + 18.f);
    rails->box.size = group->box.size;
    rails->step();
    for (size_t row = 0; row < widgets.size(); ++row) {
      float rowWidth = 0;
      M* previous = nullptr;
      for (size_t col = 0; col < widgets[row].size(); ++col) {
        M* model = rows[row][col];
        if (previous) rowWidth += gap(previous, model);
        rowWidth += widgets[row][col]->box.size.x;
        previous = model;
      }
      float x = 15.f + (width - rowWidth) * .5f;
      previous = nullptr;
      for (size_t col = 0; col < widgets[row].size(); ++col) {
        auto* widget = widgets[row][col];
        M* model = rows[row][col];
        if (previous) x += gap(previous, model);
        widget->box.pos = Vec(x, 9.f + row * 380.f);
        x += widget->box.size.x;
        group->addChild(widget);
        previous = model;
      }
    }
    if (family) {
      json_error_t error;
      auto* manifest =
          json_load_file((plugin.path + "/plugin.json").c_str(), 0, &error);
      if (!manifest) throw std::runtime_error("Cannot read module manifest");
      auto* modules = json_object_get(manifest, "modules");
      size_t index;
      json_t* item;
      json_array_foreach(modules, index, item) {
        if (json_is_true(json_object_get(item, "hidden"))) continue;
        const char* slug = json_string_value(json_object_get(item, "slug"));
        if (!slug || !slugs.count(slug)) {
          json_decref(manifest);
          throw std::runtime_error("Family image is missing a listed module");
        }
      }
      json_decref(manifest);
    }
    auto patch = [&](M* source, int out, M* target, int in, NVGcolor color) {
      if (!byModel.count(source) || !byModel.count(target)) return;
      auto* cable = new GroupCableWidget;
      cable->outputPort = byModel[source]->getOutput(out);
      cable->inputPort = byModel[target]->getInput(in);
      if (!cable->outputPort || !cable->inputPort) {
        delete cable;
        throw std::runtime_error(
            "Group patch references a missing port: " + source->slug + " out " +
            std::to_string(out) + " to " + target->slug + " in " +
            std::to_string(in));
      }
      cable->color = color;
      cable->box.size = group->box.size;
      group->addChild(cable);
      group->addChild(cable->outputPlug);
      group->addChild(cable->inputPlug);
      cable->updateCable();
      cables.push_back(cable);
    };
    if (patched) {
      patch(modelComputerscareKnolyPobs, 0, modelComputerscareBolyPuttons, 1,
            nvgRGB(193, 54, 116));
      patch(modelComputerscareBolyPuttons, 0, modelComputerscareRolyPouter, 0,
            nvgRGB(66, 158, 203));
      patch(modelComputerscareRolyPouter, 0, modelComputerscareTolyPoolsV2, 0,
            nvgRGB(203, 179, 47));
      patch(modelComputerscareTolyPoolsV2, 0, modelComputerscareSolyPequencer,
            0, nvgRGB(65, 170, 109));
      patch(modelComputerscareVolyPector, 0, modelComputerscareMelyPorge, 0,
            nvgRGB(66, 158, 203));
      patch(modelComputerscareVolyPector, 1, modelComputerscareMelyPorge, 1,
            nvgRGB(193, 54, 116));
      patch(modelComputerscareMelyPorge, 0, modelComputerscareSlolyPit, 0,
            nvgRGB(203, 179, 47));
      patch(modelComputerscareSlolyPit, 3, modelComputerscareVolyPector, 13,
            nvgRGB(65, 170, 109));
      APP->engine->stepBlock(128);
    }
    settings::cableOpacity = .55f;
    settings::cableTension = .67f;
    if (!portalSource.empty()) {
      if (name == "visualization") {
        auto source = std::unique_ptr<widget::Widget>(new widget::Widget);
        source->box.size = Vec(group->box.size.x, 398.f);
        auto* sourceRails = new app::RailWidget;
        sourceRails->box.size = source->box.size;
        source->addChild(sourceRails);
        for (auto* widget : widgets.back()) {
          group->removeChild(widget);
          widget->box.pos.y -= 380.f;
          source->addChild(widget);
        }
        capture(source.get(), portalSource, scale);
        for (auto* widget : widgets.back()) {
          source->removeChild(widget);
          widget->box.pos.y += 380.f;
          group->addChild(widget);
        }
      } else
        capture(group.get(), portalSource, scale);
      auto* portal = static_cast<ComputerscarePortaloof*>(
          byModel[modelComputerscarePortaloof]->module);
      portal->setSourceImage(1, portalSource);
    }
    capture(group.get(), output + "/" + name + ".png", scale);
  };
  std::vector<std::vector<M*>> family = {
      {modelComputerscareWindchimes, modelPhlooperPreview, modelBlunchPreview,
       modelComputerscareDebug, modelComputerscareOhPeas},
      {modelComputerscarePatchSequencer, modelComputerscareLaundrySoup,
       modelComputerscareILoveCookies, modelComputerscareHorseADoodleDoo,
       modelComputerscareMolyPatrix, modelComputerscareGolyPenerator},
      {modelComputerscareKnolyPobs, modelComputerscareBolyPuttons,
       modelComputerscareRolyPouter, modelComputerscareTolyPoolsV2,
       modelComputerscareSolyPequencer, modelComputerscareVolyPector,
       modelComputerscareMelyPorge, modelComputerscareSlolyPit,
       modelComputerscareFolyPace, modelComputerscareStolyFickPigure,
       modelComputerscarePortaloof, modelComputerscareBlank,
       modelComputerscareBlankExpander}};
  render("family", family, true, true, output + "/family-source.png");
  render("basic-poly-utilities",
         {{modelComputerscareKnolyPobs, modelComputerscareBolyPuttons,
           modelComputerscareRolyPouter, modelComputerscareTolyPoolsV2,
           modelComputerscareSolyPequencer}},
         false, true);
  render("advanced-poly-utilities",
         {{modelComputerscareVolyPector, modelComputerscareMelyPorge,
           modelComputerscareSlolyPit}},
         false, true);
  render("visualization",
         {{modelComputerscarePortaloof},
          {modelComputerscareFolyPace, modelComputerscareStolyFickPigure,
           modelComputerscareBlank}},
         false, false, output + "/visual-source.png", 450.f);
}
