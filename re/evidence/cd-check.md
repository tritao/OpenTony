# THPS2 PC CD check

Status: observed
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `FUN_004BB240`, `FUN_004BB2D0`, `FUN_004F7E30`, `FUN_004F6510`

## Observation

`FUN_004BB240` loads `texture.dat`, scans `C:` through `Z:` for CD-ROM drives, opens each candidate through MCI, reads the audio-disc table of contents, and compares it with the expected data. Both the startup path and a later recheck treat a nonzero return as “Please insert the Tony Hawk's Pro Skater 2 CD”.

The executable does not import `DeviceIoControl`; the optical `IOCTL_CDROM_READ_TOC` request is made by the Wine/MCI stack while servicing this game-side check.

## Reproducible mutation

`tony exe patch-nocd` verifies the recorded executable and creates an adjacent `THawk2.nocd.exe`. At file offset `0xBB240` it replaces:

```text
81 EC 20 02 00 00    sub esp,0x220
```

with:

```text
31 C0 C3 90 90 90    xor eax,eax; ret; nop; nop; nop
```

This makes the CD-check helper return success without modifying the canonical executable. `tony run` and `tony play` launch the generated copy automatically.

## Open questions

The patch bypasses authentication, not asset loading. The ISO filesystem should remain mounted as Wine `D:` when the game needs music or movie files that are not beside the executable.
