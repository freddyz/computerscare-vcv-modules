#pragma once
inline void checkPhlooperStorage(rack::plugin::Model* model) {
  using P=ComputerscarePhlooper;
  auto m=std::unique_ptr<P>(static_cast<P*>(model->createModule()));
  m->id=991337;
  const std::string dir=m->createPatchStorageDirectory();
  auto countWavs=[&]() { int count=0; for(const auto& path:rack::system::getEntries(dir)) if(rack::system::getExtension(path)==".wav")++count; return count; };
  rack::system::removeRecursively(dir); rack::system::createDirectories(dir);
  m->loop.size=8;
  for(int v=0;v<16;++v) { m->loop.stereo[v]=false; for(int i=0;i<16;++i)m->loop.audio[v][i]=5.f*(i%3); }
  m->onSave({});
  if(countWavs()!=1)throw std::runtime_error("Identical recordings should save one WAV");
  m->loop.audio[3][0]=15.f;
  m->loop.stereo[7]=true;
  m->onSave({});
  if(countWavs()!=3)throw std::runtime_error("Edited and stereo recordings should save independently");
  m->loop.size=0;
  for(int v=0;v<16;++v)std::fill(m->loop.audio[v].get(),m->loop.audio[v].get()+16,0.f);
  m->onAdd({});
  if(m->loop.size!=8||m->loop.audio[3][0]!=15.f||m->loop.audio[0][0]!=0.f||!m->loop.stereo[7])throw std::runtime_error("Deduplicated module audio reload failed");
  m->loop.audio[0][2]=35.f;
  if(m->loop.audio[1][2]==35.f)throw std::runtime_error("Reloaded loop RAM must remain independent");
  // Legacy patches still load, then migrate on their next save.
  rack::system::removeRecursively(dir); rack::system::createDirectories(dir);
  for(int v=0;v<16;++v)phlooper::saveWav(dir+"/loop-"+std::to_string(v+1)+".wav",m->loop.audio[v].get(),8,m->loop.stereo[v]);
  m->onAdd({});
  if(m->loop.size!=8||m->loop.audio[0][2]!=35.f)throw std::runtime_error("Legacy audio reload failed");
  m->onSave({});
  if(countWavs()!=4)throw std::runtime_error("Legacy migration should discard redundant legacy WAVs");
  m->loop.size=0; m->onSave({});
  if(countWavs()!=0)throw std::runtime_error("Empty save should remove stale audio");
  m->onAdd({}); if(m->loop.size)throw std::runtime_error("Empty reload restored stale audio");
  rack::system::removeRecursively(dir);
  std::puts("Phlooper deduplicated save/reload, independent edits, stereo, legacy migration and empty-save checks passed");
}
