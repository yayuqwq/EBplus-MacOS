/**********************************************************************************************************************
 * Copyright (c) Prophesee S.A.                                                                                       *
 * Copyright (c) 2024 CenturyArks Co.,Ltd.                                                                            *
 *                                                                                                                    *
 * Modified in 2026 by the EBplus project for the OpenEB 5.2 prepared-source integration.                            *
 * This adaptation retains only the audited CenturyArks live-device registrations and plugin integrator identity.    *
 *                                                                                                                    *
 * Licensed under the Apache License, Version 2.0 (the "License");                                                    *
 * you may not use this file except in compliance with the License.                                                   *
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0                                 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed   *
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.                      *
 * See the License for the specific language governing permissions and limitations under the License.                 *
 **********************************************************************************************************************/
#include <memory>
#include <utility>

#include "metavision/hal/plugin/plugin.h"
#include "metavision/hal/plugin/plugin_entrypoint.h"
#include "plugin/psee_plugin.h"

#if !defined(__ANDROID__) || defined(ANDROID_USES_LIBUSB)
#include "boards/treuzell/tz_camera_discovery.h"
#endif

using namespace Metavision;

void initialize_plugin(void *plugin_ptr) {
    Plugin &plugin = plugin_cast(plugin_ptr);
    initialize_psee_plugin(plugin, "CenturyArks");

#if !defined(__ANDROID__) || defined(ANDROID_USES_LIBUSB)
    auto discovery = std::make_unique<TzCameraDiscovery>();
    discovery->add_usb_id(0x31f7, 0x0002, 0x19);
    discovery->add_usb_id(0x31f7, 0x0003, 0x19);
    discovery->add_usb_id(0x31f7, 0x0004, 0x19);
    plugin.add_camera_discovery(std::move(discovery));
#endif
}
