#pragma once
// Safe during plugin init; macOS defers attachment until the UI is ready.
// Panel retries remain idempotent. Inactive without VCVMON_CONTROL.
void installDevWatcherToolbar();
