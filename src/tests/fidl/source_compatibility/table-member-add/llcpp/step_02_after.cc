// Copyright 2021 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fidl/fidl.test.tablememberadd/cpp/wire.h>  // nogncheck
namespace fidl_test = fidl_test_tablememberadd;

// [START contents]
void use_table(const fidl_test::wire::Profile& profile) {
  if (profile.has_timezone()) {
    printf("timezone: %s", profile.timezone().data());
  }
  if (profile.has_temperature_unit()) {
    printf("preferred unit: %s", profile.temperature_unit().data());
  }
  if (profile.has_dark_mode()) {
    printf("dark mode on: %s", profile.dark_mode() ? "true" : "false");
  }
}
// [END contents]

int main(int argc, const char** argv) { return 0; }