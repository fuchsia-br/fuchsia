// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(https://fxbug.dev/84961): Fix null safety and remove this language version.
// @dart=2.9

import 'sl4f_client.dart';

/// Get information about the Device.
class Device {
  final Sl4f _sl4f;

  Device(this._sl4f);

  /// Returns target's nodename.
  Future<String> getDeviceName() async =>
      await _sl4f.request('device_facade.GetDeviceName');
}
