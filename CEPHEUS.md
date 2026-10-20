# Xiaomi Mi 9 (cepheus): initial bring-up

Status: offline/live DTB inspection works; a separate stage-0 CPU probe and a
candidate single-CPU console ADT generator are written.
**XNU and iOS have not booted on this device. The stage-0 boot image is compiled
and inspected offline, but has not been tested on hardware.** The topaz loader and kernel remain unchanged, apart from optional
framebuffer compile-time overrides for the standalone probe.

## Observed on 2026-10-20

The connected ADB device reports `cepheus`, Android 16 / SDK 36, aarch64,
1080x2340 and MemTotal 5,579,548 KiB (approximately a 6 GB model). ADB is an
unprivileged shell, `su` is absent, and SELinux denies the live FDT, memory/reg
and /proc/iomem. Android properties report flash.locked=1 / verifiedbootstate
green; this is not yet confirmed in fastboot and must not be assumed to describe
the actual bootloader on this custom ROM. Both `mindowsesp` and `mindowswin`
exist. Preserve Android, Mindows, firmware, GPT and all existing backups.

The local boot backup contains a legacy Android v0 header (4096-byte pages),
and four contiguous DTBs appended to the kernel: SM8150 v2/v1 and SM8150P v2/v1.
These are base trees, **not the bootloader-patched live tree or DTBO overlays**.
In particular `/memory/reg` is all zero and the UFS node is disabled in the base
tree. Do not treat that as evidence the actual phone has no RAM or storage.

| Item | Backup evidence | Topaz value that must not be reused |
| --- | --- | --- |
| GIC distributor | 0x17a00000, size 0x10000 | 0x0f200000 |
| GIC redistributors | 0x17a60000, size 0x100000; stride 0x20000 | 0x0f300000 |
| CPU MPIDRs | 0x000, 0x100, ..., 0x700 | 0..3, 0x100..0x103 |
| Timer frequency | 19,200,000 Hz | same nominal frequency; check CNTFRQ |
| UFS host | 0x01d84000, size 0x2500 | 0x04804000 |
| Continuous splash reservation | 0x9c000000, size 0x02400000 | 0x5c000000 |
| Screen | ADB: 1080x2340 | 1080x2400 |
| Usable physical RAM | unknown until live DT is read | never assume topaz's high bank |

Later in the same session the user entered custom recovery. ADB has uid=0 there.
The live FDT (665,717 bytes) and /proc/iomem were pulled read-only into ignored
`research/cepheus/`. They confirm SM8150 v2, board-id 0x25/0, active UFS and:

- RAM banks: 0x80000000 + 0x3cc00000; 0x140000000 + 0xc0000000;
  0xc0000000 + 0x80000000. The high bank is **3 GiB**, not topaz's 4 GiB.
- All reported fixed carve-outs and dynamic reservation allocation ranges are
  below 4 GiB. /proc/iomem lists 0xc0000000 through 0x1ffffffff as System RAM.
- Touch: `st,fts`, I2C address 0x49 at GENI 0xc80000, interrupt GPIO122.
  The topaz FT5452 driver is incompatible.
- DRM DSI connector reports `1080x2340x60x179431`, connected. No legacy fb0
  sysfs attributes are exposed by this recovery.

No reboot, unlock, fastboot boot, partition write or storage mount was issued.

The standalone probe uses 0x9c000000 / 1080x2340 / 4320-byte stride.
The reservation is confirmed in the backup; pixel layout/scanout are still
unverified. Geometry and format have precedent in the original
[cepheus Linux bring-up DTS](https://lkml.indiana.edu/hypermail/linux/kernel/2201.0/04738.html).
The SM8150 CPU addressing can also be checked in
[Linux sm8150.dtsi](https://github.com/torvalds/linux/blob/master/arch/arm64/boot/dts/qcom/sm8150.dtsi).

## Reproduce offline inspection

From the repository root:

```powershell
python tools/cepheus-dtb.py ../woa-mi9/partition_backup/partitions/boot.img --out research/cepheus/boot-hardware.json
python tools/test-cepheus-dtb.py
python tools/build-cepheus-probe.py --stock-boot ../woa-mi9/partition_backup/partitions/boot.img
```

The builder needs clang, ld.lld and llvm-objcopy on PATH. It compiles only
start.S, console.c and cepheus-probe.c, pads the Image to its linked memory size,
preserves all four original DTBs, and writes a fresh v0 header with the original
load addresses and a recomputed SHA-1 ID. Output is ignored under
`shim/out/cepheus-probe/`. It does not access a phone. The initial sandbox denied
compiler execution. After the user changed the session permissions, Scoop LLVM
compiled and linked successfully. The builder now resolves executable paths
explicitly, since Windows CreateProcess does not append .exe to `ld.lld`.

Successful artifact: `shim/out/cepheus-probe/cepheus-probe-boot.img`, 2,084,864
bytes, SHA256 `9656472bf90ffde17c0b309f6ad589f84b79600136eef4de9742dee9c45dcc57`.
ELF inspection confirms AArch64, entry 0, no remaining relocations and no ABS64
address relocations in the C objects. ARM64 Image size is 77,824 bytes. The boot
v0 header, SHA-1 ID, padding/size and all four original DTBs were independently
checked (DTBs byte-identical). Four DTB reader tests pass. These are offline
checks; bootloader acceptance, CPU register values and scanout remain untested.

The probe displays MIDR, MPIDR, MMFR0/TGran16, ISAR0, CNTFRQ, SCTLR and the DTB
pointer, then parks. It does not start XNU, wake other CPUs, probe UFS, alter
clocks, reset or write partitions. Boot it only temporarily after checking the
actual bootloader state and original device-tree selection. An image built by
this script is not yet a validated recovery or iOS image.

`tools/mkadt-cepheus.py` builds a console ADT using a live cepheus FDT. It checks
the high bank against the firmware reservation map and fixed/dynamic DT
reservation ranges, validates the boot MPIDR against the actual CPU nodes,
and uses the cepheus GIC/framebuffer/timer map. It omits UFS, WLAN and the display
service. Example used for offline validation:

```powershell
python tools/mkadt-cepheus.py --live-dtb research/cepheus/live-recovery.dtb --boot-mpidr 0 --out shim/out/cepheus-console-candidate.adt
```

The generated ADT is 3940 bytes. **MPIDR 0 is an explicit candidate, not a measured
ABL entry CPU.** The iPhone D22AP product artifact is absent locally (warning
printed), so this tree is only for early console preparation. Existing kernel
patches still hardcode topaz MMIO and must be ported before any XNU boot attempt.

## Next milestones

1. With the user's agreement, enter fastboot/recovery for diagnostics. Confirm
   product/unlocked state; never run an unlock command (it can erase data).
2. Live DTB and iomem collected; GPT still needs a current read-only inventory.
   Keep raw data under
   ignored research/ or existing backup folders. Inspect RAM banks, reserved
   regions, overlays, touch/controller and active splash configuration.
3. Compile and validate the probe, then temporary RAM boot. Confirm its screen,
   boot CPU's 16K support and actual timer frequency.
4. Add a cepheus ADT/shim path using the measured memory map. Audit hardcoded
   addresses across XNU patches (early framebuffer assembly, GIC, CPU startup,
   watchdog, direct display, UFS, cpufreq, RPMh, SPMI, touch and WLAN). Keep
   topaz-specific hardware drivers disabled until individually ported.
5. Reach XNU console using one CPU and no storage writes; only then bring up
   UFS read-only, input and iOS userland. Plan storage around existing Mindows
   partitions. Any destructive storage operation needs separate user approval;
   the old topaz/sweet userdata approval does not authorize erasing this Mi 9.
