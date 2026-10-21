# Xiaomi Mi 9 (cepheus): experimental XNU/iOS port

## Current hardware status (2026-10-20)

The user has observed the iOS lock screen on real hardware. This is an
experimental XNU/iOS port, not a complete hardware-supported installation.
The older bring-up entries below describe earlier stages.

| Function | Observed state / remaining work |
| --- | --- |
| XNU, UFS, HFS root, launchd | Boot confirmed; writes constrained to the existing userdata geometry. |
| Lock screen / compositor | User-confirmed lock screen; CPU copies to ABL framebuffer, no GPU acceleration. |
| Console overlay | `_cnputs` mute was insufficient for direct kprintf; patch0070 also gates the framebuffer serial backend, retaining panic display. Physical verification pending. |
| Red status bar | Logical iPhone X canvas1125x2436 is confirmed in persistent log; panel remains1080x2340. Need user verification of the current image. |
| Touch | ST FTS521 at I2C0x49 on SE17. First test stopped before chip access on S_AHB HWCG/HALT. Patch0070 handles HWCG; photo then identified a14MB I/O mapping allocation panic. Patch0074 maps only the pin windows and avoids that panic. Latest log confirms loaded I2C firmware but FIFO disabled: a GPI DMA/SMMU transport is required. |
| Buttons | Experimental PM8150 read-only prototype compiled; keys1 image now temporarily booted, real test pending. Live FDT maps power/volume-down to PM8150 PON; volume-up to PM8150 GPIO6; assistant key to TLMM GPIO97. Requires a reviewed Mi9 SPMI/input implementation. |
| GPU / Metal | XNU driver not implemented; Metal explicitly deferred. Recovery KGSL query succeeds with stock firmware: chip06040001, GMEM1MiB. Requires Adreno command/memory/scheduling support and compatible iOS graphics services/userspace; Windows GPU binaries are not XNU drivers. |
| Wi-Fi | Not implemented; requires SM8150 connectivity power, firmware, bus and network integration. Original Topaz WLAN node intentionally excluded. |
| Bluetooth | Not implemented; requires transport, controller firmware and iOS interface integration. Launching bluetoothd does not establish hardware support. |
| Cellular data | Not implemented; modem remoteproc/shared-memory/transport and iOS radio integration remain. Preserve modem/calibration partitions. |
| Camera | Not implemented; requires sensor power/I2C, CSI/ISP and compatible camera services. |
| Fast charging | Not implemented; PMIC/USB negotiation, thermal control and actual battery telemetry need validation before higher-current operation. |

The local Windows BSP under `woa-mi9/drivers/extracted` supplies useful device
identifiers and firmware/configuration references, not directly loadable XNU
drivers. The open-source [Project Aloha FTS521 driver](https://github.com/Project-Aloha/STFingerTipS521-Touch)
and pinned Android kernel references are saved under ignored research paths on G.
The [Mi9 Windows BSP status](https://github.com/qaz6750/XiaoMi9-Drivers/blob/main/Status.md)
itself lists only slow charging and incomplete camera support; its GPU support
must not be interpreted as iOS Metal support.

Do not claim a function fixed from compilation or fastboot acceptance. Every
input/display change needs a real-device test and a subsequent persistent log.


## First observed XNU panic and next storage stage

User photo 20261020_122256.jpg confirms XNU reached panic logging on hardware.
Saved PC is sha1_init+0x34 (0xfffffe0007910b28), LR is the instruction after
SHA1Init in IOPlatformExpertDevice::generatePlatformUUID (0xfffffe00079791dc),
FAR0x28 with x22=0. Disassembly loads [x22,#0x28]: the absent corecrypto function
table. Baseline0018 omitted the existing portable SHA1 patch0032. This is a real
kernel failure before iOS, not a missing-filesystem-only symptom.

New cepheus-ufs target stages0001-0034 plus forced read-only UFS, adding portable
crypto/pthread/HFS and 64-bit DMA support. CI39457341486 is building commit81a48d4.
The live FDT UFS reg-names are ufs_mem/ufs_ice, with regions 0x1d84000+0x2500
and 0x1d90000+0x8000; only the first region is exposed. No ICE programming.
mkadt-cepheus.py --storage-read-only enables that validated region. The boot
builder requires matching explicit --storage-read-only, ufsrw=0, ufsroot=userdata,
rd=ufsroot0; output is separated as shim/out/cepheus-xnu-ufs-ro/.
This stage is for read-only storage discovery; it is not a validated iOS boot.

## Real XNU console build, 2026-10-20

CI run: https://github.com/edeegg/ios-topaz/actions/runs/39454541376
Source commits 5947680 and 4bcaaa6 on owned fork branch `cepheus-bringup`.
`--board cepheus` stages only 0001-0018, changes early geometry to 1080x2340
(candidate stride 4320), and labels the CPU Kryo 485. Uses shared SWEET config,
plain arm64, single CPU, GICv3 Group1 and 19.2MHz virtual timer. No storage,
input, USB, modem, direct display, cpufreq or battery drivers are included.
This first build targets kernel console bring-up, not iOS userland.

`shim/cepheus-xnu.c` refuses XNU handoff unless entry is EL1, MMU/D-cache off,
MPIDR affinity zero, 16K translation supported and CNTFRQ=19200000. It uses
the existing segment/boot_args loader and does not start secondary CPUs.
The Windows-native builder accepts `--payload` from mkpayload.py, checks the
known high RAM bank and single-CPU no-storage cepheus ADT, and preserves the
stock Qualcomm wrapper and four original DTBs. Loader and dependencies compile
and link with LLVM warnings-as-errors, no unresolved symbols or C ABS64 relocs.

Preparation after downloading the real CI kernel:

```powershell
python tools/mkadt-cepheus.py --live-dtb research/cepheus/live-recovery.dtb --boot-mpidr 0 --out shim/out/cepheus-console-candidate.adt
python tools/mkpayload.py KERNEL shim/out/cepheus-console-candidate.adt shim/out/cepheus-xnu-payload.bin --cmdline 'debug=0xc serial=3 cpus=1 keepsyms=1 -enable_kprintf_spam'
python tools/build-cepheus-probe.py --stock-boot ../woa-mi9/partition_backup/partitions/boot.img --payload shim/out/cepheus-xnu-payload.bin
```

Reconfirmed root recovery ADB after starting adb with ADB_LIBUSB=1. A tiny
Linux user-mode CPUID helper ran from recovery /tmp and returned MIDR 51df804e,
MMFR0 ff000000, ISAR0 100010211120, CNTFRQ 124f800. Linux emulates/sanitizes
ID register reads at EL0, so this MMFR0 does NOT establish hardware 16K support,
and the running Linux CPU is NOT proof of the ABL entry CPU. No partition writes.

## Latest session: firmware staged, hardware boot unconfirmed

The requested iPhone X iOS 16.7.16 (20H392) IPSW was downloaded from Apple's CDN
to ignored `research/ipsw-x/iPhone10,3,iPhone10,6_16.7.16_20H392_Restore.ipsw`.
Size: 6,171,035,648 bytes; SHA256 independently checked:
`ece1194f7e233e9109c5790a8d23a4564a10a254e15ec5b09978a343265568fb`.
The D22AP Erase identity in BuildManifest selects:

| Component | Extracted file | Bytes |
| --- | --- | --- |
| OS | 087-86698-044.dmg | 4730814044 |
| Cryptex1,SystemOS | 087-86593-047.dmg | 2743074816 |
| Cryptex1,AppOS | 087-86715-047.dmg | 10485760 |

All three were extracted with ZIP CRC validation. DeviceTree.d22ap.im4p was
decoded using a locally installed LZFSE wheel under ignored research/pydeps;
`research/ipsw-x/d22ap.adt` is 186084 bytes. Candidate console ADT now includes
the product node and is 6628 bytes. No HFS+ root image was built or installed.
The current WSL installation has no distro and reports a missing WSL2 kernel;
the guide's Linux apfs-fuse/HFS+ build environment is not available yet.

The first temporary raw-image diagnostic was accepted by fastboot but the user
observed only a black screen. An entry/reset-only variant was added, omitting
all console code; it waits five seconds, sets restart_reason at 0x146bf65c
(verified live FDT: IMEM 0x146bf000 + 0x65c), and calls PSCI SYSTEM_RESET.
An exception parks instead, to avoid falsely reporting successful execution.
The bootloader did not visibly return within 30 seconds of the wrapped test;
CPU execution/reset remains unconfirmed. User was asked whether recovery was
entered manually or automatically. Do not equate a Booting OKAY with execution.

**Format correction:** the stock boot kernel has a 20-byte Qualcomm wrapper,
`UNCOMPRESSED_IMG` plus little-endian u32 raw Image file length, before the Image.
Its length exactly locates the first appended DTB. The builder now reproduces
that wrapper. Current artifacts supersede the older hashes elsewhere below:

- display probe: 2084864 bytes, SHA256
  `0fa65d6d044259bb91d0be78ba78ee4211cb02f0c6c4f0d96bdd2fcb53a1d413`;
- entry/reset probe: 2080768 bytes, SHA256
  `39dee2fbdfe06194784ff3643a5809f0ff695f06f75fa10ff0573c1d4d0f3f3f`.

Both pass independent wrapper/ARM64-header/length/SHA-1/byte-identical-DTB
checks. The four DTB tests still pass. The corrected display probe was then
temporarily booted: Sending OKAY 0.055s, Booting OKAY 0.139s, total 0.198s.
The user's screen report is pending; no CPU execution claim yet.
Recovery DRM uses AR24 at pitch 4352, unlike the candidate
probe's 4320; bootloader pitch is still unknown. Recovery dmesg confirms splash
reservation 0x9c000000 + 0x2400000, but this does not identify active ABL scanout.

Fastboot explicitly confirmed product=cepheus, unlocked=yes, secure=yes.
The Windows Xiaomi driver oem141.inf needed association via elevated pnputil.
A forced USB/IP share later captured the phone's ADB interface; the phone's
bus 1-4 was unbound with usbipd (other devices were untouched). ADB then returned
in recovery with uid=0. The phone was subsequently booted into the corrected
display diagnostic; current screen observation is pending. Current GPT primary/secondary
metadata were saved read-only under research/cepheus. userdata is sda33,
61,223,206,912 bytes; Mindows is sda31/sda32 and remains intact. No flash, erase,
unlock, GPT mutation or installation was performed. Debugfs was mounted for
read-only display inspection; no disk filesystem was mounted by these steps.

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
