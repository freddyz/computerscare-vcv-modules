#pragma once
template<class Add, class Require>
void testBeekInputMinus(Add add, Require require) {
  using Owner = beekapoo::Beekapoo;
  auto* rack = APP->scene->rack;
  auto* model = plugin::getModel("computerscare", "demo-gain");
  auto wire = [&](app::PortWidget* from, app::PortWidget* to) {
    auto* c = new engine::Cable;
    c->outputModule = from->module; c->outputId = from->portId;
    c->inputModule = to->module; c->inputId = to->portId;
    APP->engine->addCable(c);
    auto* cw = new app::CableWidget; cw->setCable(c); rack->addCable(cw);
    return c->id;
  };
  for (float gain : {1.f, .37f, -.6f, 0.f}) for (bool edit : {false, true}) {
    rack->clear(); APP->history->clear();
    auto* source = add(model, Vec());
    auto* second = add(model, Vec(90,0));
    auto* target = add(model, Vec(180,0));
    auto* outside = add(model, Vec(1500,0));
    source->module->params[0].setValue(.3f);
    second->module->params[0].setValue(.7f);
    auto targetId = target->module->id;
    auto* owner = static_cast<Owner*>(add(modelBeekapooPreview, Vec(400,0)));
    auto ownerId = owner->module->id;
    rack->deselectAll();
    for (auto* w : {source,second,target}) rack->select(w,true);
    owner->collapse();
    owner->expose(source,source->getOutput(0));
    owner->expose(second,second->getOutput(0));
    for(int i:{1,0}) owner->expose(target,target->getInput(i));
    auto proxy = [&](int input) {
      for (const auto& p : owner->ports)
        if (p.input && p.id==targetId && p.portId==input) return owner->findPort(p);
      return static_cast<app::PortWidget*>(nullptr);
    };
    for (auto& p : owner->ports) if (p.input && p.portId==1)
      p.gain->getParamQuantity()->setImmediateValue(gain);
    std::vector<int64_t> routes;
    routes.push_back(wire(source->getOutput(0),proxy(1)));
    routes.push_back(wire(source->getOutput(0),proxy(1))); // duplicate source stack
    routes.push_back(wire(second->getOutput(0),proxy(1)));
    auto direct = wire(second->getOutput(0),target->getInput(1)); // bypasses gain
    owner->internalCableIds.insert(direct);
    auto outbound = wire(source->getOutput(0),outside->getInput(1));
    std::string phase="initial";
    auto settle = [&] {
      for(int i=0;i<3;++i) rack->step();
      APP->engine->stepBlock(256);
      owner=dynamic_cast<Owner*>(rack->getModule(ownerId));
      target=owner->find(targetId);
      try {checkBeekCablePresentation(require);} catch(...) {std::fprintf(stderr,"Input minus gain %.2f edit %d phase %s\n",gain,edit,phase.c_str());throw;}
    };
    settle();
    if(edit) { owner->editSubpatch(); settle(); }
    std::vector<float> expected;
    for(int ch=0;ch<4;++ch) expected.push_back(target->module->inputs[1].getVoltage(ch));
    auto verifySignal = [&] {
      settle();
      require(target->module->inputs[1].getChannels()==4,"Input minus retains polyphony");
      for(int ch=0;ch<4;++ch)
        require(std::abs(target->module->inputs[1].getVoltage(ch)-expected[ch])<.0001f,"Input minus preserves stacked signals, negative/zero gain and direct internal inputs");
      require(owner->ports.size()>=2 && owner->exposed(source->getOutput(0)) && owner->exposed(second->getOutput(0)),"Input minus keeps every exposed output");
      require(rack->getCable(direct)&&rack->getCable(outbound),"Unrelated internal and outside cables survive");
      for(auto id:routes) require(rack->getCable(id),"Each stacked self-patch retains its cable ID");
    };
    // Unpatched has priority even when the self-patched slot was exposed first.
    phase="remove unpatched";require(owner->removeExposedControl(1),"Minus removes an unpatched input first");
    verifySignal();
    require(!proxy(0)&&proxy(1)&&owner->members.size()==3,"First minus only hides the unpatched input");
    phase="undo unpatched";APP->history->undo(); settle(); require(proxy(0)&&proxy(1),"Undo restores unpatched exposure");
    phase="redo unpatched";APP->history->redo(); verifySignal();
    owner->randomLocks.insert({targetId,-1});
    require(!owner->removeExposedControl(1),"Minus respects a locked input module");
    owner->randomLocks.clear();
    auto external=wire(outside->getOutput(0),proxy(1));settle();
    require(!owner->removeExposedControl(1),"An outside source in a self-patched stack protects the entire input");
    auto* cw=rack->getCable(external);rack->removeCable(cw);delete cw;
    settle();
    auto before=owner->ports.size();
    phase="fold";require(owner->removeExposedControl(1),"Minus internalizes a self-only stack");
    auto verifyFolded = [&] {
      verifySignal();
      require(owner->ports.size()+1==before&&!proxy(1),"Fold removes exactly one exposed input");
      require(owner->members.size()==size_t(gain==1.f?3:4),"Only non-unity gain adds an 8vert inside the Beek");
      for(auto id:routes) require(owner->internalCableIds.count(id)&&!owner->panelCableIds.count(id),"Folded cables are internal, never dangling panel wires");
    };
    verifyFolded();
    for(int n=0;n<2;++n) {
      phase="undo fold";APP->history->undo();verifySignal();require(proxy(1)&&owner->members.size()==3,"Undo fold restores gain mapping and removes adapter");
      phase="redo fold";APP->history->redo();verifyFolded();
    }
    auto* saved=APP->engine->toJson();rack->mergeJson(saved);
    auto sourceId=source->module->id, secondId=second->module->id;
    phase="reload";rack->clear();APP->history->clear();APP->engine->fromJson(saved);rack->fromJson(saved);json_decref(saved);
    settle();source=owner->find(sourceId);second=owner->find(secondId);verifyFolded();
  }
  rack->clear();APP->history->clear();
  std::puts("Input minus: unpatched priority, self/duplicate stacks, outside-source protection, locks, four-channel gains, edit, Undo/Redo and reload passed");
}
