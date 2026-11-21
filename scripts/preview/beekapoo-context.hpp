#pragma once
template<class Require>
void testBeekPortContext(Require require) {
  for(bool hidden:{false,true}) {
    widget::Widget root;
    auto* face=new beekapoo::PortFace;
    root.addChild(face);
    app::ComputerscareSvgPort* art[]={&face->tinyDown,&face->tinyUp,&face->largeDown,&face->largeUp};
    for(int cycle=0;cycle<2;++cycle) {
      for(int i=0;i<4;++i) {
        face->compact=i<2;face->input=i%2==0;face->box.size=art[i]->box.size;
        // Top-level drawing allocates the native jack framebuffers. Rendering
        // into the screenshot framebuffer instead would bypass those caches.
        APP->window->fbCount()=0;
        auto* vg=APP->window->vg;nvgBeginFrame(vg,64,64,1);
        widget::Widget::DrawArgs args;args.vg=vg;args.clipBox=Rect(Vec(),Vec(64,64));args.fb=nullptr;
        face->draw(args);nvgEndFrame(vg);
        require(art[i]->fb->getImageHandle()>=0,"Live jack drawing must allocate its native framebuffer");
      }
      // Resizing leaves caches in previously shown variants. All of them,
      // including hidden artwork, must receive Rack's shutdown notification.
      face->visible=!hidden;
      widget::Widget::ContextDestroyEvent destroyed;destroyed.vg=APP->window->vg;
      root.onContextDestroy(destroyed);
      for(auto* jack:art)
        require(jack->fb->getImageHandle()<0,"Graphics shutdown must release all exposed-jack caches before the window disappears");
      if(cycle==0) {
        widget::Widget::ContextCreateEvent created;created.vg=APP->window->vg;
        root.onContextCreate(created);
        for(auto* jack:art)require(jack->fb->dirty,"Recreated graphics context must redraw every jack variant");
      }
    }
    // Rack deletes its window first and its scene second. Reproduce that
    // destructor ordering without closing the isolated test's actual window.
    auto* window=APP->window;APP->window=nullptr;
    root.clearChildren();
    APP->window=window;
  }
  std::puts("Exposed jack graphics lifecycle: four variants, hidden artwork, context recreation and window-before-scene teardown passed");
}

template<class Add,class Require>
void testBeekGroupShutdown(Add add,Require require) {
  auto* rack=APP->scene->rack;
  auto* model=plugin::getModel("computerscare","demo-gain");
  for(int mode=0;mode<3;++mode) {
    rack->clear();APP->history->clear();
    auto* child=add(model,Vec());
    auto* owner=static_cast<beekapoo::Beekapoo*>(add(modelBeekapooPreview,Vec(240,0)));
    rack->select(child,true);owner->collapse();
    owner->exposeParameter(child->module->id,0);
    owner->expose(child,child->getInput(0));owner->expose(child,child->getOutput(0));
    if(mode==1)owner->expand();
    if(mode==2)owner->editSubpatch();
    owner->step();
    for(const auto& p:owner->ports) {
      APP->window->fbCount()=0;
      auto* vg=APP->window->vg;nvgBeginFrame(vg,64,64,1);
      widget::Widget::DrawArgs args;args.vg=vg;args.clipBox=Rect(Vec(),Vec(64,64));args.fb=nullptr;
      p.face->draw(args);nvgEndFrame(vg);
    }
    widget::Widget::ContextDestroyEvent destroyed;destroyed.vg=APP->window->vg;
    APP->scene->onContextDestroy(destroyed);
    {
      struct WindowGone {
        window::Window* previous=APP->window;
        WindowGone(){APP->window=nullptr;}
        ~WindowGone(){APP->window=previous;}
      } windowGone;
      rack->clear();
    }
    require(APP->engine->getModuleIds().empty(),"Quit must remove the owner and all subpatch modules");
    widget::Widget::ContextCreateEvent created;created.vg=APP->window->vg;
    APP->scene->onContextCreate(created);
  }
  std::puts("Beek shutdown: collapsed, expanded and active Edit sessions destroy cleanly after window shutdown");
}
