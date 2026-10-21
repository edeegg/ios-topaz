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

Sources reside only in ignored research/cepheus/unixnu-reference on G. Clones are shallow and non-recursive; Mesa and the full shader toolchains were not downloaded.

## Required adaptations

- Replace NativeAdreno A740-only hardware-init and sheng-specific GMU/power inputs with the verified A640 register, firmware and power sequences. NativeAdrenoService calls buildA740; changing a device name does not retarget those writes.
- Use the stock Mi9 SQE, GMU and signed ZAP firmware already extracted and hashed locally. The recovery driver successfully queried chip06040001 and GMEM1MiB; that is not proof of XNU execution or rendering.
- Port the native SMMU mapping, BO allocation, cache maintenance, CP rings and fences. Current Mi9 XNU uses16KiB CPU pages; the reference GPU tables use4KiB pages. These can potentially coexist, but allocator and mapping assumptions must be reviewed rather than blindly changing the kernel page size.
- Adapt NativeMetal IOSurface/discovery/encoding integration to the actual iOS16.7.16 runtime. The reference documents an iPadOS/sheng plugin, so binary/API compatibility is not established.
- Build the AIR-to-SPIR-V translator and Turnip A6xx compiler path; qualify shaders, GPU fence completion and pixels on real hardware before publishing a Metal device.

## Evidence still required

GPU: a real XNU submission completes and produces a checked output buffer/frame. Metal: discovery, shader translation, execution, synchronization and rendered output succeed on this Mi9. Neither result is currently achieved.

References: [UniXNU architecture](https://unixnu.com/drivers/native-adreno), [NativeAdreno](https://github.com/UniXNU/NativeAdreno), [NativeMetal](https://github.com/UniXNU/NativeMetal).
