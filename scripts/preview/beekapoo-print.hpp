#pragma once
// Sparse layout contact sheet, rendered from actual Beekapoo widgets.
void renderBeekapooPrintSpectrum(plugin::Model* sourceModel,
                                const std::string& output, float scale) {
  struct Case { int knobs, buttons, inputs, outputs, hp; };
  const Case cases[] = {
    {0,0,0,0,4}, {0,0,1,1,4}, {1,0,1,1,4}, {1,1,1,1,5}, {2,0,2,1,5}, {3,0,3,1,6},
    {0,0,3,1,6}, {0,1,1,0,6}, {2,1,1,2,7}, {3,1,2,1,7}, {4,0,3,1,8}, {5,0,3,1,8},
    {0,0,0,2,8}, {0,2,2,1,8}, {1,2,1,1,9}, {3,2,3,1,9}, {5,0,3,1,10}, {2,2,0,0,10},
    {0,0,2,0,4}, {0,1,0,1,4}, {2,0,1,0,5}, {0,2,1,1,5}, {3,0,0,1,6}, {1,2,2,1,6},
    {0,0,4,1,7}, {2,2,3,1,7}, {4,1,3,1,8}, {1,1,0,2,8}, {0,3,3,1,9}, {4,0,1,3,9},
    {5,0,3,1,11}, {2,1,2,2,11}, {1,0,1,1,12}, {0,2,1,1,12}, {3,2,3,1,12}, {4,1,3,1,14}
  };
  auto* rack = APP->scene->rack;
  auto add = [&](plugin::Model* model, Vec position) {
    auto* m = model->createModule(); APP->engine->addModule(m);
    auto* w = model->createModuleWidget(m); w->box.pos = position;
    rack->addModule(w); return w;
  };
  auto require = [](bool pass, const char* why) {
    if (!pass) throw std::runtime_error(why);
  };
  auto* manifest = json_array();
  const int knobIds[] = {0,4,5,6,7};
  const char* knobNames[] = {"Pitch", "Tone", "Decay", "Spread", "Level"};
  for (int index = 0; index < 36; ++index) {
    const auto& c = cases[index];
    auto* source = add(sourceModel, Vec(0,0));
    auto* host = static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview, Vec(300,0)));
    rack->deselectAll(); rack->select(source,true); host->collapse();
    for (int n = 0; n < c.knobs; ++n) {
      auto* q = source->module->getParamQuantity(knobIds[n]); q->name = knobNames[n];
      q->setImmediateValue(q->getMinValue() + (q->getMaxValue()-q->getMinValue()) * ((index*5+n*7)%19)/18.f);
      host->exposeParameter(source->module->id,knobIds[n]);
    }
    for (int n = 0; n < c.buttons; ++n) {
      int id = n+1; auto* q = source->module->getParamQuantity(id);
      q->setImmediateValue((index+n)%int(q->getMaxValue()+1));
      host->exposeParameter(source->module->id,id);
    }
    for (int n = 0; n < c.inputs; ++n) {
      source->module->getInputInfo(n)->name = "In " + std::to_string(n+1);
      host->expose(source,source->getInput(n));
    }
    for (int n = 0; n < c.outputs; ++n) {
      source->module->getOutputInfo(n)->name = "Out " + std::to_string(n+1);
      host->expose(source,source->getOutput(n));
    }
    host->requestedWidth = c.hp*15; host->layoutControls(); host->step();
    int visible = 0, buttons = 0;
    for (auto& param : host->parameters) {
      visible += param.knob->visible;
      buttons += dynamic_cast<beekapoo::ExposedSwitch*>(param.knob) != nullptr;
    }
    require(visible == c.knobs+c.buttons,"Sparse print case unexpectedly hides a mapped control");
    require(buttons == c.buttons,"Sparse print case must render actual switches as buttons");
    require(host->ports.size() == size_t(c.inputs+c.outputs),"Sparse print port count changed");
    require(host->box.size.x == c.hp*15,"Sparse print case unexpectedly changed width");
    std::string name = string::f("print-%02d.png",index+1);
    auto position = host->box.pos;
    rack->getModuleContainer()->removeChild(host); host->box.pos = Vec();
    capture(host,output+"/"+name,scale,false);
    host->box.pos = position; rack->addModule(host);
    auto* item = json_pack("{s:i,s:i,s:i,s:i,s:i,s:i,s:s}",
                          "index",index+1,"knobs",c.knobs,"buttons",c.buttons,
                          "inputs",c.inputs,"outputs",c.outputs,"hp",c.hp,
                          "image",name.c_str());
    json_array_append_new(manifest,item);
    rack->clear();
  }
  int result = json_dump_file(manifest,(output+"/print-manifest.json").c_str(),JSON_INDENT(2));
  json_decref(manifest);
  require(result==0,"Could not save sparse spectrum manifest");
  std::puts("36 sparse Beekapoo layouts rendered with exact exposed knob, button and jack counts");
}
