# cc26x0f128-hello

A bare-metal C starter for the TI CC2640F128 (SimpleLink CC26x0), built
without TI's SDK and without Code Composer Studio.

**Status: abandoned.** Kept as a record of why, and of the parts that did
work. The blocker is not the amount of vendor code — it is that the clock tree
cannot be brought up without values the manual withholds, and the routines that
do it exist only as mask ROM with no source. See below.

## Why this is a dead end

**There is no free and open way to bring up a clock tree on this part, and
without a defined clock nothing beyond a blink runs.**

The plan was to treat the CC26x0 like any other Cortex-M3: obtain a CMSIS-SVD,
generate register headers from it, and write the peripheral code directly. The
pipeline for that was built and works. What kills it is the clock.

### 1. The clock tree needs factory values the manual does not give you

The device starts on its internal high-frequency RC oscillator, and everything
else is derived from it — `SystemCoreClock` included, which every delay, timer,
baud rate and bus timing scales from. Bringing it up is not a matter of setting
a divider: the oscillator has to be calibrated against values measured on each
individual die at final test.

Those values live in FCFG1, and the ones that matter are all marked "Internal.
Only to be used through TI provided API":

| FCFG1 register | Purpose |
|---|---|
| `SHDW_OSC_BIAS_LDO_TRIM` | oscillator bias and LDO trim |
| `SHDW_ANA_TRIM` | analog block trim |
| `ANA2_TRIM` | analog block trim |
| `LDO_TRIM` | LDO trim |
| `RCOSC_HF_TEMPCOMP` | HF RC oscillator temperature compensation |
| `CONFIG_OSC_TOP` | top-level oscillator configuration |
| `AMPCOMP_TH1`, `AMPCOMP_TH2`, `AMPCOMP_CTRL1` | amplitude compensation thresholds |
| `CAP_TRIM` | oscillator cap-array trim |
| `VOLT_TRIM` | voltage trim |

54 of FCFG1's 83 registers carry that marker. Only `MISC_TRIM` and `OSC_CONF`
are documented at all.

So the manual tells you a register exists and withholds what belongs in it.
There is no published source for those values and no description of how to
interpret them. Reading them out blind and writing them back is guesswork with
an oscillator.

### 2. The supported path is mask ROM, not source

TI's answer to the above is driverlib, and driverlib is genuinely open: it is
BSD-3-Clause and redistributable. But it does not contain the clock setup. It
is a thin wrapper over routines burned into the die:

| File | Calls into ROM |
|---|---|
| `setup.c` | `ROM_SetupTrimDevice` |
| `osc.c` | `ROM_OSCClockSourceSet`, `ROM_OSCClockSourceGet`, `ROM_OSCHF`, `ROM_OSC` (10 calls) |
| `aon_wuc.c` | `ROM_AONWUCOscConfig`, `ROM_AONWUCAuxReset`, `ROM_AONWUCRechargeCtrlConfigSet` |
| `aux_wuc.c` | `ROM_AUXWUCClockEnable/Disable/Status`, `ROM_AUXWUCPowerCtrl` |

The ROM is fabricated into the silicon. Its source does not exist in
distributable form and never will. This is not a licence term that can be
argued with — it is a load-bearing component for which source is physically
unavailable. `rom.h` declares the entry points, so they can be called and
linked, but not read, audited, ported, patched or replaced.

That is the precise sense in which this dependency tree is not open source.
Individual files are. The tree as a whole is not, because the part that does
the work you actually need has no source at all.

### 3. The radio cannot be programmed directly

The RF core is a separate Cortex-M0 with TI-supplied firmware. The BLE stack
is a binary. There is no register-level path to a working radio link, which
means anyone using this part ships TI's stack, which in turn ships its own
build system and its own HAL. That undercuts the point of a from-scratch
starter for anything above a toy.

This is not a defect in TI's documentation; it is how the part works. It is
also the least surprising of these, and on its own it would not have been
fatal — a project that used TI's stack deliberately is a normal project. What
makes it terminal here is finding 2: the stack is not merely closed, it is
partly not source.

### 4. The device description that might have bypassed all of it does not exist

**No vendor SVD exists.** Neither BLE SDK 2.2 nor the cc26xxware package it
depends on contains one, and a recursive search of the whole `C:\ti` install
(BLE SDK 2.2, TI-RTOS, SysConfig, MSPM0 SDK) returns zero `.svd` files. What
TI ships instead is hand-maintained `hw_*.h` register headers — an equivalent
for humans, not an input for a generator.

**SysConfig is not an alternative.** Its device data is JSON under
`dist\deviceData\<PART>\<PART>.json`, but searching a 414 kB device file for
`register`, `bitfield`, `interrupt`, `addressOffset` and `baseAddress` returns
zero hits for all five. It carries pinmux, packages, power domains and cores.
There is no register data to convert, and there is no `CC2640F128` part entry
either — only the CC2640R2F variants. No converter, however written, can
recover information that is not there.

**The one published SVD is defective.** `cmsis-svd-data` carries
`data/TexasInstruments/CC26x0.svd` v2.3, which is TI-derived and covers the
right registers. It is not directly consumable:

- Interrupt `<value>` entries are byte offsets into the vector table, not IRQ
  numbers. Fed to a generator unmodified, it produced
  `I2C_IRQ_IRQn = 4` and `TRNG_IRQ_IRQn = 132`, which displaces every NVIC
  call and the entire handler table by a factor of four, with no error.
- 48 violations of the CMSIS-SVD 1.0 schema (`alternateRegister` used as a
  child of `register`, empty `<fields>`, empty `<description>`, hex strings in
  an `xs:integer` field).
- **All 3242 field descriptions are the field name repeated.** There is no
  semantic content at all — `CALC_EN` is documented as "CALC_EN".

That last point is what matters most for the SVD route. Even after the other
defects are corrected, the SVD is an address map without meaning: it says a
bit exists at position 7, not what it does.

### 5. What the manual does document, it does not share

`swcu117i.pdf` contains 1441 occurrences of "Only to be used through TI
provided API" — the semantic content an SVD cannot supply and the manual
declines to. Separately, the manual is copyrighted, so even the documented
half cannot be redistributed next to the code that depends on it.

## The rest of the record

The sections below are what the attempt produced. They are accurate and are
the reason this repository is published rather than deleted, but none of them
unblock the clock.

## What is documented, and why it does not matter

A detail worth recording, because it shows the conclusion is not simply "the
documentation is unusable".

In the SVD, the "Internal" marker appears on **207 of 969 registers, 21.4%**,
and it is heavily concentrated:

| Peripheral | Registers marked internal |
|---|---|
| FLASH | 122 / 123 |
| FCFG1 | 54 / 83 |
| AUX_SCE | 10 / 10 |
| AON_BATMON | 9 / 13 |
| AUX_ADI4 | 5 / 11 |
| AON_IOC | 3 / 5 |
| CPU_TIPROP | 2 / 2 |
| PRCM | 1 / 56 |

**34 of 43 peripherals are entirely clean**: GPIO (15/15), IOC (32/32), PRCM
(56/57), UART0 (14/14), SSI0/1 (10/10), I2C0 (18/18), GPT0-3 (29/29 each),
UDMA0 (19/19), WDT (9/9), CRYPTO (40/40), TRNG (22/22). The manual agrees:
clustering its 1441 occurrences by nearby peripheral name gives VIMS 476,
PRCM 66, FCFG1 7, IOC 2, and **zero** within ±400 characters of GPIO, UART,
SSI, I2C, GPT, WDT, UDMA, CRYPTO or TRNG.

So on paper the peripherals are well documented. It does not help, and the
distinction is worth being exact about: a documented register is not a working
peripheral. GPIO, UART, SPI, I2C and the timers all take their timing from the
clock tree, and the clock tree is finding 1. The registers being described
does not make them reachable without a defined clock to run them from.


## What works

The pipeline is complete and was verified against TI's own headers rather than
against the SVD's claims:

```
contrib/platform/share/CC26x0F128.svd
  -> contrib/svdconv          -> contrib/platform/include/CC26x0F128.h
  -> contrib/irqn             -> contrib/platform/include/CC26x0F128_handlers.h
  -> CC26x0F128_it.c          -> vector table
```

- The SVD was normalized (interrupt values divided by four, schema violations
  removed, device renamed so the header filename is right) and now validates
  with zero schema errors. `svdconv` reports zero errors and zero warnings.
- **42/42** peripheral base addresses match `hw_memmap.h`.
- **33/33** external interrupts and **10/10** core exceptions match `hw_ints.h`.
- IRQ 3 remains a skip, matching the gap in TI's own `startup_gcc.c` table.
- The memory map was corrected to 128 kB flash (130,984 usable bytes), 20 kB
  SRAM and 8 kB GPRAM, following the four linker files TI ships.
- `systick.c`, the startup code and the system layer compile against the
  generated header, and `make` links.

That is where it stops. The code builds and links, but the tick it configures
is derived from the clock tree described at the top, so it would not run.

## What is unfinished

Left in place rather than papered over:

- **The LED is not driven.** `main.c` starts the tick and loops. The DIO the
  LED sits on has to come from the schematic and be muxed through IOC before
  GPIO sees it; that was not done.
- **Nothing populates CCFG.** The linker script reserves the last 88 bytes of
  flash as the `CCFG` region, but no `.ccfg` section is emitted, so the chip
  reads whatever the flash happens to contain there. TI's `ccfg.c` is the
  reference for doing it properly.
- `.vscode/launch.json` still points its `svdFile` at `MSPM0L130X.svd`.
- `src/systick.c` explains its interrupt masking in terms of Cortex-M0 lacking
  exclusive access instructions. The masking is still correct on this part;
  the reasoning no longer applies to an M3.
- `.vscode/c_cpp_properties.json` defines `HSE_VALUE=24000000`, an STM32-ism.
- The vendored submodules are pinned but `contrib/cmsis` tracks a release tag
  while `contrib/irqn` and `contrib/svdconv` track branch heads.

## If someone wants to continue

The SVD work in this repository is reusable as-is: the normalized device
description, the verification against TI's headers, and the generation
pipeline are all correct and independent of the application.

Continuing means accepting the ROM blob. There is no third option: the clock
tree is either set up by `ROM_SetupTrimDevice` and the `ROM_OSC*` family, or it
is not set up. `cc26xxware` (driverlib plus the `hw_*.h` headers) on top of the
BLE SDK's build system is the supported arrangement, and it works — it is just
not open source in the sense that word is normally meant, and treating it as
though it were is what this project got wrong.

Worth being clear about the size of the compromise: it is one opaque component
(a mask ROM that every CC26xx already contains), not a licence over the whole
tree. driverlib and the register headers are BSD-3-Clause and can be read and
patched. If a dependency on unreadable code is acceptable — as it is for most
people who ship this part — the project is workable today.

## Layout

| Path | Contents |
|---|---|
| `src/` | application: `main.c`, `systick.c` |
| `include/` | application headers |
| `lib/` | board linker script |
| `contrib/platform/` | device layer: memory map, startup, system, SVD |
| `contrib/cmsis` | submodule: ARM CMSIS 6 |
| `contrib/irqn` | submodule: handler-table generator |
| `contrib/svdconv` | submodule: SVD to header generator |
| `hardware/` | KiCad sources for the CC2640 EVM |
| `docs/` | not tracked; see below |

## Building

Requires `arm-none-eabi-gcc`, `make` and `python3`. Submodules must be checked
out. Device headers are generated, never committed:

```
git submodule update --init --recursive
make          # generates headers, then builds
make size     # flash and SRAM usage
make distclean
```

`docs/*.pdf` is ignored: TI's datasheet, technical reference manual
(SWCU117I), antenna note (SWRA117D) and the EVM schematic are copyrighted and
are not redistributed here. To get them back:

| Document | Source |
|---|---|
| `cc2640.pdf` (datasheet) | <https://www.ti.com/lit/ds/symlink/cc2640.pdf> |
| `swcu117i.pdf` (technical reference manual) | <https://www.ti.com/lit/ug/swcu117i/swcu117i.pdf> |
| `swra117d.pdf` (antenna measurements) | <https://www.ti.com/lit/an/swra117d/swra117d.pdf> |
| `cc2640-evm.pdf` | re-plot from `hardware/cc2640-evm.kicad_sch` (KiCad, Plot to PDF) |

## Licence

BSD 3-Clause; see [LICENSE.md](LICENSE.md). The licence grants copyright
rights only. Trademark terms are in [TRADEMARKS.md](TRADEMARKS.md).

`contrib/platform/share/CC26x0F128.svd` is derived from the device description
published by the [cmsis-svd-data](https://github.com/cmsis-svd/cmsis-svd-data)
project, itself derived from TI's device database. It is included here because
it cannot be obtained in this corrected form anywhere else; review TI's terms
before redistributing it further.
