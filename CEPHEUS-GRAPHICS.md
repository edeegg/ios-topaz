# Mi 9 GPU and Metal port assessment

Status: reference sources collected and reviewed; no GPU or Metal implementation installed in XNU.

The user has re-enabled Metal in the task scope. Volume buttons are user-confirmed working. The ambiguous GPU/touch report still needs clarification; current code and logs do not demonstrate XNU GPU execution.

## Public source baseline

The ios-topaz public branch at89451b8 does not contain its private HackiOS GPU work. An independent public source candidate now exists: UniXNU NativeAdreno and NativeMetal, documented for Adreno740/sheng. This supersedes treating unpublished ios-topaz patches as the only possible source route.

Parent revision: UniXNU/UniXNU fb2bb310564ce66779e472a021881cf7c5764919

- NativeAdreno: `805c7e4bfb7451eb6923f4a14dea8c657b9be7d2`
- NativeMetal: `6dcd4cbb69eb5625988706de42aa8ad3009b8ee7`
- AdrenoFirmware: `40fdb8ea87606612ac6f1e80bbb49955d0ce4051`
- AdrenoPM4: `445f8fe6d115738352b9d78dc70af6d61aa7f62f`
- ArmSmmu4K: `4d0d6598b1a36aeec3de01091893fa6660e49388`

Reference trees reside in ignored research/cepheus/unixnu-reference on G. Downloads are shallow and non-recursive; Mesa and the full shader toolchains were not downloaded. The unchanged MIT ArmSmmu4K core is now vendored with its license and pinned provenance under drivers/cepheus/third_party.

## Implemented components, not yet connected to XNU

`drivers/cepheus/adreno640_clock.c` implements A640 HWCG/GBIF sequences with
separate GPU/GMU register domains and explicit chip/power/quiescence guards.
`gpu_tables.c` builds inactive GPU4K tables in CPU16K allocations. It requires
caller-owned physical memory and an explicitly verified output address width.
Neither component activates hardware or publishes an accelerator service.

The cepheus-ios build now stages these sources as XNU BSD objects through
generated patch0075. Other board builds do not include this backend.
Full XNU build39582019432 at8de7143 passed. The final kernel contains the
backend symbols (verified with llvm-nm), but has no startup hook that activates
them. Kernel SHA256:8c064f0795ceed53fee57b97f77dc084428fe2517c612e9ce08cc52ccd5b9ccc.
`adreno640_gmu.c` programs A640 RSC/PDC sequencing using the Mi9 CMD DB addresses;
it requires externally verified CX/clocks/reset-held/exclusive prerequisites.
`adreno640_mmio.c` translates global GMU DWORD indices from1a800 and bounds
accesses against the actual separate apertures. Mapping, power and real device
barriers remain platform responsibilities.

`adreno640_firmware.c` validates the complete GMU image before routing TCM words
and cache blocks. Its verified Android layout is16KiB ITCM at0,240KiB ICACHE
at4000,16KiB DTCM at40000 and240KiB DCACHE at44000. Cache IOVAs must resolve
through owned allocations; they are never CPU pointers. The stock37680-byte
image passes RAM-callback routing checks, including all seven blocks. No live
firmware upload, reset release or HFI initialization has been performed.

Run `python tools/check-cepheus-a640-clock.py` and
`python tools/check-cepheus-gpu-tables.py` on Windows LLVM. Both pass host
behavior tests and compile actual ARM64/iOS objects. The table test walks
translations above4GiB and rejects overlapping table memory, insufficient leaf
budgets and misaligned CPU allocations. These are component tests, not proof
of GPU execution, coherent SMMU walks or Metal support.

`python tools/check-cepheus-gmu-firmware.py` tests firmware validation and upload
routing using synthetic input. Pass `--stock-firmware PATH` to additionally
validate an existing local stock image without committing it or writing it to
hardware. The unchanged AdrenoFirmware core is vendored with its BSD patent
license and pinned provenance.

## Required adaptations

- Replace NativeAdreno A740-only hardware-init and sheng-specific GMU/power inputs with the verified A640 register, firmware and power sequences. NativeAdrenoService calls buildA740; changing a device name does not retarget those writes.
- Use the stock Mi9 SQE, GMU and signed ZAP firmware already extracted and hashed locally. The recovery driver successfully queried chip06040001 and GMEM1MiB; that is not proof of XNU execution or rendering.
- Port the native SMMU mapping, BO allocation, cache maintenance, CP rings and fences. Current Mi9 XNU uses16KiB CPU pages; the reference GPU tables use4KiB pages. These can potentially coexist, but allocator and mapping assumptions must be reviewed rather than blindly changing the kernel page size.
- Adapt NativeMetal IOSurface/discovery/encoding integration to the actual iOS16.7.16 runtime. The reference documents an iPadOS/sheng plugin, so binary/API compatibility is not established.
- Build the AIR-to-SPIR-V translator and Turnip A6xx compiler path; qualify shaders, GPU fence completion and pixels on real hardware before publishing a Metal device.

## Evidence still required

GPU: a real XNU submission completes and produces a checked output buffer/frame. Metal: discovery, shader translation, execution, synchronization and rendered output succeed on this Mi9. Neither result is currently achieved.

References: [UniXNU architecture](https://unixnu.com/drivers/native-adreno), [NativeAdreno](https://github.com/UniXNU/NativeAdreno), [NativeMetal](https://github.com/UniXNU/NativeMetal).
