#pragma once
// Real toolbar actions write only into the preview's isolated runtime folder.
template<typename Capture>
void checkPatchSaves(rack::widget::Widget*& toolbar,Capture capture) {
  using namespace rack;
  auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
  auto button=[&](const std::string& name){
    for(auto* child:toolbar->children)if(auto* b=dynamic_cast<ui::Button*>(child))if(b->text==name)return b;
    return static_cast<ui::Button*>(nullptr);
  };
  auto click=[&](const std::string& name){auto* b=button(name);require(b,"Patch save button exists");widget::Widget::ActionEvent event;b->onAction(event);};
  auto options=[&](){click("Options");return dynamic_cast<ui::Menu*>(APP->scene->children.back()->children.front());};
  auto folderItem=[&](ui::Menu* menu){for(auto* child:menu->children)if(auto* item=dynamic_cast<ui::MenuItem*>(child))if(item->text=="Save in patch folder")return item;return static_cast<ui::MenuItem*>(nullptr);};
  auto close=[](ui::Menu* menu){auto* overlay=menu->parent;APP->scene->removeChild(overlay);delete overlay;};
  auto toggle=[&](){auto* menu=options();auto* item=folderItem(menu);require(item,"Folder option exists");event::Action action;item->onAction(action);close(menu);};
  std::string previous=APP->patch->path;
  auto* initialMenu=options();initialMenu->step();bool folderOn=!folderItem(initialMenu)->rightText.empty();close(initialMenu);if(!folderOn)toggle();
  std::string scratch=asset::user("save-tests-"+std::to_string(random::u32()));system::createDirectories(scratch);
  auto source=[&](const char* filename){std::string path=system::join(scratch,filename);{std::ofstream file(path);file<<"original";}APP->patch->path=path;return path;};
  source("patchxyz.vcv");
  auto* dirty=new history::Action;dirty->name="preview save edit";APP->history->push(dirty);
  click("Save version");
  std::string folder=system::join(scratch,"patchxyz");
  require(APP->patch->path==system::join(folder,"patchxyz1.vcv"),"First save creates a folder using the whole opened name");
  require(system::exists(APP->patch->path)&&APP->history->isSaved(),"Numbered action writes a patch and marks it saved");
  std::string unpacked=asset::user("saved-archive");system::createDirectories(unpacked);
  system::unarchiveToDirectory(APP->patch->path,unpacked);
  json_error_t error;json_t* saved=json_load_file(system::join(unpacked,"patch.json").c_str(),0,&error);
  require(saved&&!json_is_true(json_object_get(saved,"unsaved")),"Patch archive itself records clean saved history");json_decref(saved);
  click("Save version");require(APP->patch->path==system::join(folder,"patchxyz2.vcv"),"Further saves reuse the flat folder");
  click("Save branch");std::string branch=system::getStem(APP->patch->path);
  require(system::getDirectory(APP->patch->path)==folder&&branch.find("patchxyz2-")==0,"Branch appends word inside the same folder");
  click("Save branch");std::string sibling=system::getStem(APP->patch->path);
  require(sibling!=branch&&computerscare::patch_saves::branchBase(sibling)=="patchxyz2"&&system::getDirectory(APP->patch->path)==folder,"Root branch save creates a sibling without nesting folders or filename suffixes");
  APP->patch->path=system::join(folder,branch+".vcv");
  click("Save version");require(system::getFilename(APP->patch->path)==branch+"1.vcv"&&system::getDirectory(APP->patch->path)==folder,"Numbered save keeps branch and appends outer number");
  click("Save version");require(system::getFilename(APP->patch->path)==branch+"2.vcv","Numbered branch increments its trailing number");
  std::ifstream original(system::join(scratch,"patchxyz.vcv"));std::string untouched;original>>untouched;require(untouched=="original","Original opened file is untouched");
  source("test1.vcv");click("Save version");require(APP->patch->path==system::join(scratch,"test1/test2.vcv"),"An opened numbered patch retains its complete folder name");
  // Reopen a generated file independently: marker alone prevents nesting.
  APP->patch->path=system::join(folder,"patchxyz1.vcv");click("Save version");
  require(APP->patch->path==system::join(folder,"patchxyz3.vcv"),"Reopened generated file uses highest sibling number and existing folder");
  std::string moved=system::join(scratch,"moved-patchxyz");
  require(system::rename(folder,moved),"Fixture folder moves");APP->patch->path=system::join(moved,"patchxyz3.vcv");click("Save branch");
  require(system::getDirectory(APP->patch->path)==moved,"Moving a folder preserves flat saves without absolute parent mappings");
  toggle();
  APP->scene->removeChild(toolbar);delete toolbar;toolbar=nullptr;
  ::installDevWatcherToolbar();
  for(auto* child:APP->scene->children)for(auto* control:child->children)
    if(auto* b=dynamic_cast<ui::Button*>(control))if(b->text=="Save version")toolbar=child;
  require(toolbar,"Toolbar recreates using saved preferences");
  source("loose.vcv");click("Save version");require(APP->patch->path==system::join(scratch,"loose1.vcv"),"Folder mode off saves beside the current patch");
  json_t* settings=json_load_file(asset::user("computerscare-patch-saves.json").c_str(),0,&error);
  require(settings&&json_is_false(json_object_get(settings,"folderMode")),"Folder preference is persisted");json_decref(settings);
  toggle();
  std::string manual=system::join(scratch,"manual");system::createDirectories(manual);
  APP->patch->path=system::join(manual,"manual.vcv");click("Save version");require(APP->patch->path==system::join(manual,"manual1.vcv"),"Existing same-name folder is not nested");
  source("full.vcv");std::string full=system::join(scratch,"full");system::createDirectories(full);
  for(const char* word:computerscare::patch_saves::words()){std::ofstream taken(system::join(full,std::string("full-")+word+".vcv"));taken<<"occupied";}
  click("Save branch");std::string combined=system::getStem(APP->patch->path);
  bool single=false;for(const char* word:computerscare::patch_saves::words())single |= combined==std::string("full-")+word;
  require(!single&&computerscare::patch_saves::branchBase(combined)=="full"&&system::getDirectory(APP->patch->path)==full,"Full single-word namespace saves concatenated words in the same folder");
  click("Save branch");require(computerscare::patch_saves::branchBase(system::getStem(APP->patch->path))=="full","Concatenated branch roots also create siblings");
  auto* menu=options();menu->step();require(folderItem(menu)->rightText.find(CHECKMARK_STRING)!=std::string::npos,"Folder mode defaults/restores to on");close(menu);
  toolbar->step();
  capture(toolbar,"04-patch-saves.png");
  menu=options();auto* overlay=menu->parent;overlay->removeChild(menu);APP->scene->removeChild(overlay);delete overlay;
  widget::Widget composition;composition.box.size=Vec(toolbar->box.size.x,240);
  Vec position=toolbar->box.pos;APP->scene->removeChild(toolbar);toolbar->box.pos=Vec();composition.addChild(toolbar);composition.addChild(menu);menu->step();menu->box.pos=Vec(300,32);composition.box.size.y=menu->box.size.y+40;
  capture(&composition,"05-patch-save-options.png");
  composition.removeChild(toolbar);composition.removeChild(menu);delete menu;toolbar->box.pos=position;APP->scene->addChild(toolbar);
  APP->patch->path=previous;
  std::puts("Patch toolbar saves, portable flat folders, numeric branches, archive history, settings and original preservation passed");
}
