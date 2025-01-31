// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/devices/board/drivers/av400/av400.h"

#include <assert.h>
#include <fuchsia/hardware/platform/device/c/banjo.h>
#include <lib/ddk/debug.h>
#include <lib/ddk/device.h>
#include <lib/ddk/driver.h>
#include <lib/ddk/platform-defs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fbl/algorithm.h>
#include <fbl/alloc_checker.h>

#include "src/devices/board/drivers/av400/av400-bind.h"

namespace av400 {

zx_status_t Av400::Create(void* ctx, zx_device_t* parent) {
  iommu_protocol_t iommu;

  zx_status_t status = device_get_protocol(parent, ZX_PROTOCOL_IOMMU, &iommu);
  if (status != ZX_OK) {
    return status;
  }

  fbl::AllocChecker ac;
  auto board = fbl::make_unique_checked<Av400>(&ac, parent, &iommu);
  if (!ac.check()) {
    return ZX_ERR_NO_MEMORY;
  }

  if (!board->pbus_.is_valid()) {
    return ZX_ERR_INTERNAL;
  }

  status = board->DdkAdd("av400");
  if (status != ZX_OK) {
    return status;
  }

  if (status == ZX_OK) {
    // devmgr is now in charge of the device.
    __UNUSED auto* dummy = board.release();
  }

  return status;
}

int Av400::Thread() {
  // Load protocol implementation drivers first.
  zx_status_t status;

  zxlogf(INFO, "Initializing AV400 board!!!");

  if ((status = GpioInit()) != ZX_OK) {
    zxlogf(ERROR, "GpioInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = PwmInit()) != ZX_OK) {
    zxlogf(ERROR, "PwmInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = ClkInit()) != ZX_OK) {
    zxlogf(ERROR, "ClkInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = I2cInit()) != ZX_OK) {
    zxlogf(ERROR, "I2cInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = RegistersInit()) != ZX_OK) {
    zxlogf(ERROR, "RegistersInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = EmmcInit()) != ZX_OK) {
    zxlogf(ERROR, "EmmcInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = SpiInit()) != ZX_OK) {
    zxlogf(ERROR, "SpiInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = SdioInit()) != ZX_OK) {
    zxlogf(ERROR, "SdioInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = EthInit()) != ZX_OK) {
    zxlogf(ERROR, "EthInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = RtcInit()) != ZX_OK) {
    zxlogf(ERROR, "RtcInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = AudioInit()) != ZX_OK) {
    zxlogf(ERROR, "AudioInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = UsbInit()) != ZX_OK) {
    zxlogf(ERROR, "UsbInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = ThermalInit()) != ZX_OK) {
    zxlogf(ERROR, "ThermalInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = SysmemInit()) != ZX_OK) {
    zxlogf(ERROR, "SysmemInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = TeeInit()) != ZX_OK) {
    zxlogf(ERROR, "TeeInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = PowerInit()) != ZX_OK) {
    zxlogf(ERROR, "PowerInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = CpuInit()) != ZX_OK) {
    zxlogf(ERROR, "CpuInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = DmcInit()) != ZX_OK) {
    zxlogf(ERROR, "DmcInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = ButtonsInit()) != ZX_OK) {
    zxlogf(ERROR, "ButtonsInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = MailboxInit()) != ZX_OK) {
    zxlogf(ERROR, "MailboxInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  if ((status = DspInit()) != ZX_OK) {
    zxlogf(ERROR, "DspInit() failed: %s", zx_status_get_string(status));
    init_txn_->Reply(status);
    return status;
  }
  init_txn_->Reply(status);
  return ZX_OK;
}

void Av400::DdkInit(ddk::InitTxn txn) {
  init_txn_ = std::move(txn);
  int rc = thrd_create_with_name(
      &thread_, [](void* arg) -> int { return reinterpret_cast<Av400*>(arg)->Thread(); }, this,
      "av400-start-thread");
  if (rc != thrd_success) {
    init_txn_->Reply(ZX_ERR_INTERNAL);
  }
}

static constexpr zx_driver_ops_t av400_driver_ops = []() {
  zx_driver_ops_t ops = {};
  ops.version = DRIVER_OPS_VERSION;
  ops.bind = Av400::Create;
  return ops;
}();

}  // namespace av400

ZIRCON_DRIVER(av400, av400::av400_driver_ops, "zircon", "0.1");
