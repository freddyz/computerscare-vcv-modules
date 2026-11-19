#pragma once
// Called only on Rack's UI thread. Inactive without VCVMON_CONTROL. Safe to
// retry before UI readiness, after scene changes, or after toolbar removal.
void installDevWatcherToolbar();
