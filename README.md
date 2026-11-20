# cc26x0f128-hello

> **The avoidance of TI tooling in this project is deliberate.**
>
> Do not raise issues regarding CCS, SysConfig or the TI BLE SDK. Such issues
> will be deleted and their authors blocked immediately. This is not an
> oversight, an unmet prerequisite or a gap to be filled in: the constraint is
> the reason the project exists. Let this serve as forewarning.
>
> This is a moderation rule covering those three tools and nothing wider.
> Issues and pull requests on anything else are welcome.

A bare-metal C starter for the TI CC2640F128 (SimpleLink CC26x0), built on an
open toolchain: GNU make, `arm-none-eabi-gcc`, and a device header generated
from a CMSIS-SVD. No vendor IDE, no vendor SDK, no vendor build system.

**Status: abandoned.** Kept as a record of why, and of the parts that did
work. The short version: the clock setup is vendor code that is at least
readable, but the radio stack is precompiled archives with no source and no
equivalent, so anything using this part as a radio takes TI's tree along with
it. The hardware is being scrapped for a separate reason 鈥?see
[the hardware](#the-hardware-and-why-it-is-being-scrapped). See below.

## Why this is a dead end

**Anything beyond a blink requires the clock setup, the clock setup requires
TI's code, and using this part as a radio requires TI's binary stack. The
constraint was open tools throughout, so there was no version of this that
worked.**

The plan was to treat the CC26x0 like any other Cortex-M3: obtain a CMSIS-SVD,
generate register headers from it, and write the peripheral code directly. The
pipeline for that was built and works. What kills it is what has to be linked
underneath.

### 1. The clock tree needs factory values the manual does not give you

The device starts on its internal high-frequency RC oscillator, and everything
else is derived from it 鈥?`SystemCoreClock` included, which every delay, timer,
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

### 2. The clock setup is vendor code 鈥?and the ROM is a distraction

TI's answer to the above is driverlib, and driverlib is genuinely open:
BSD-3-Clause, readable, patchable, redistributable. It also contains the clock
setup in full. `setup.c` is a complete 394-line `SetupTrimDevice` that reads
FCFG1 and trims the device, and all ten `ROM_*` calls in `osc.c` have a
`NOROM_` source equivalent 鈥?`OSCClockSourceSet`, `OSCHF_TurnOnXosc`,
`OSCHF_AttemptToSwitchToXosc` and the rest.

That last point matters, because it removes the mask ROM from the argument.
`ROM_SetupTrimDevice` and the `ROM_OSC*` family are flash-saving duplicates of
behaviour that is published as readable source in the same package. Redirecting
a call site to the `NOROM_` implementation is a `#define`; the ROM is never
required. So a card that ships a ROM block is not a disclosure problem when the
same logic is available as source 鈥?and here it is.

What is genuinely undocumented is narrower: the FCFG1 calibration the code
consumes. You can read exactly what `SetupTrimDevice` does with each value, but
not what the values mean, because their field semantics are what the manual
declines to document. So the clock is inspectable, and reimplementable only
with the source in hand 鈥?but not verifiable against a specification, and not
writable from published information alone.

That is a real limit, and it is a modest one. It is not the limit that kills
this project. The next finding is.

### 3. The radio is a binary, and unlike the ROM it has no source equivalent

The RF core is a separate Cortex-M0 running TI-supplied firmware, and the BLE
protocol stack ships as precompiled archives with no source: hundreds of them
in `blelib/`, `cc2640_ctrl_*.a` at 0.7鈥?.1 MB each and `host_*.a` at 1.0鈥?.8 MB
each, plus the HCI transport layers. Nothing in the SDK corresponds to them.

This is where the ROM comparison stops being a distraction and becomes the
argument. For the oscillator, TI publishes the logic as source and the ROM is
optional. For the radio, there is no source at all, no `NOROM_` equivalent, and
no way to build a link without the archives. There is no register-level path to
a working radio either, so the binary is not a convenience 鈥?it is the only
implementation that exists.

Which means using this part as a radio means adopting TI's build system,
TI-RTOS and XDCtools along with the archives. A project that chooses that
deliberately is a normal project. This one did not, and could not: the
constraint was open tools, and the radio alone violates it regardless of
anything else in the tree.

Worth being explicit about what would fix it, since it is not exotic. TI could
document the ROM in the reference manual or an application note, and an
operator could then show that document to an inspector 鈥?the oscillator case is
already nearly there, because driverlib's source is the reference. No
equivalent exists for the stack, and no amount of documentation substitutes for
source when the question is what the transmitter is actually doing.

### 4. The device description that might have bypassed all of it does not exist

**No vendor SVD exists.** Neither BLE SDK 2.2 nor the cc26xxware package it
depends on contains one, and a recursive search of the whole `C:\ti` install
(BLE SDK 2.2, TI-RTOS, SysConfig, MSPM0 SDK) returns zero `.svd` files. What
TI ships instead is hand-maintained `hw_*.h` register headers 鈥?an equivalent
for humans, not an input for a generator.

**SysConfig is not an alternative.** Its device data is JSON under
`dist\deviceData\<PART>\<PART>.json`, but searching a 414 kB device file for
`register`, `bitfield`, `interrupt`, `addressOffset` and `baseAddress` returns
zero hits for all five. It carries pinmux, packages, power domains and cores.
There is no register data to convert, and there is no `CC2640F128` part entry
either 鈥?only the CC2640R2F variants. No converter, however written, can
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
  semantic content at all 鈥?`CALC_EN` is documented as "CALC_EN".

That last point is what matters most for the SVD route. Even after the other
defects are corrected, the SVD is an address map without meaning: it says a
bit exists at position 7, not what it does.

### 5. What the manual does document, it does not share

`swcu117i.pdf` contains 1441 occurrences of "Only to be used through TI
provided API" 鈥?the semantic content an SVD cannot supply and the manual
declines to. Separately, the manual is copyrighted, so even the documented
half cannot be redistributed next to the code that depends on it.

## The same method, on two parts

This was not a question of difficulty, and it is worth recording why, because
the same technique was applied to TI's MSPM0 family first and worked: take the
vendor's per-peripheral headers, rebuild them into a CMSIS-SVD, generate a
device header, drive the registers directly. That is a reasonable amount of
work and it pays off.

The same approach here does not pay off, and the difference is measurable in
the same units on both families:

| | MSPM0 | CC26x0 |
|---|---|---|
| CMSIS-SVD shipped by TI | none | none |
| per-peripheral header tree | 43 files, 6.5 MB | 58 files |
| "Only to be used through TI provided API" in those headers | **0** | **1054**, across 15 files |
| the same phrase in the reference manual | **0** in 6.0 M characters | **1441** in 3.0 M characters |
| header-to-SVD rebuild | viable 鈥?the manual specifies the registers | not viable 鈥?the clock cannot be specified from published material |

The MSPM0 manual (SLAU847, 6.0 M characters extracted) does not contain the
phrase at all. The CC26x0 manual (SWCU117, 3.0 M characters) contains it once
every ~2,100 characters.

In the headers it is concentrated precisely where it does the most damage:

| Header | Occurrences |
|---|---|
| `hw_flash.h` | 322 |
| `hw_fcfg1.h` | 231 |
| `hw_adi_0_rf.h` | 159 |
| `hw_adi_3_refsys.h` | 66 |
| `hw_ddi_0_osc.h` | 61 |
| `hw_adi_4_aux.h` | 57 |

`hw_fcfg1.h` is the factory calibration from finding 1, and `hw_ddi_0_osc.h`
is the oscillator controller. The two headers that would have to be understood
to bring up a clock are the second and fifth most withheld in the tree.

So the conclusion is not that this was too hard, or that the tooling was
missing. A method that works on one TI family is defeated on another by what
the documentation chooses to say, in a way no amount of engineering effort
recovers.

## Why it could never have been maintained

Openness is a licensing argument. This is the practical one, and it is the one
that decides whether the result is a project or a one-off.

The test is whether `git clone && make` works. That is what makes a project
maintainable at all: a new contributor builds it, CI builds it, a bisect
builds it, and a bug report from two years ago can be reproduced.

With the radio in the tree, that test cannot pass, and the licence is why:

- **The blobs cannot be redistributed.** The SDK licence permits distribution
  only as "embedded executable copies of such Licensed Materials that execute
  solely and exclusively on, or in conjunction with, TI Devices" (搂iv). They
  cannot be committed, vendored, mirrored, or attached to a release.
- **The download cannot be delegated.** 搂11 requires the downloader to make
  export-control representations personally 鈥?that they are not located in or
  under the control of an embargoed country, are not on the SDN or Denied
  Persons lists, and will not use the materials for weapons end-uses. A CI
  runner cannot make those representations on a contributor's behalf, and a
  hosted service has no business accepting them. There is also a PRC
  registration obligation in 搂11a that falls on the recipient.
- **The binary cannot be inspected to work around it.** The licence forbids
  reverse engineering the object-code portions, "nor permit any person or
  entity to do so" 鈥?which closes the obvious fallback of disassembling the
  stack to demonstrate what it does.

So every consumer must obtain the SDK individually, accept terms in their own
name, and end up with a dependency that cannot be pinned by content hash,
cannot be cached, cannot be mirrored, and cannot be audited. Builds are not
hermetic; two people with the same commit do not have the same inputs.

The result is a class of project that is buildable exactly once, by the person
who assembled it, on the machine where they assembled it. Not a library, not a
platform, not something anyone can fork and fix. One-off. Consumable. That is
what "e-waste" means in engineering terms rather than as an insult: an artefact
whose value cannot outlive its assembly.

**This repository is the proof by construction.** It builds from a fresh clone
鈥?public submodules, `make`, zero errors, 1480 bytes of text 鈥?precisely
because it dropped the radio. Reproducibility was not achieved alongside the
dead end; it was achieved *by* accepting the dead end.

## Where the black box sits

The question is not whether a black box exists. Silicon is opaque, always, and
that is not a problem. The question is where the boundary falls — because that
is what decides whether anyone can point at it.

Auditors and compliance reviewers accept a boundary they can treat as a fact: a
die, a core, a discrete flashed artefact with an address and a version. What
nobody can work with is an opaque region with no edge, because there is then
nothing to name, hash, version, replace or declare.

Three ways to ship a radio, in order of how well they survive scrutiny:

**AT86RF233 — the boundary is the die.** A transceiver with no firmware at all:
a documented register interface over SPI, and every line that drives it is
yours. Your firmware contains no vendor code, so auditing means checking your
own source against the datasheet. The split is silicon, which means it is a fact
about the part and survives any build system. This is the normal shape of a
hardware component, and it is fine.

**STM32WB55 — the boundary is a core.** The wireless firmware is a binary too,
but it runs on the M0+ and is flashed as its own image. ST is explicit about the
split: "All the code running on CPU2 is delivered as an encrypted binary.
Blackbox for customer perspective" (AN5289 §4.1), and "the customer has only
access to CPU1 core and sees the firmware running on CPU2 as a blackbox"
(§10.4). Two things follow. The opaque part has an address, a version and a hash,
so it can be declared and attested even if it cannot be read — and the system
clock on CPU1 is yours to write from a reference manual that documents the
bits. Bounded, attributable, and reproducible by anyone with the binary, which
SLA0044 permits redistributing.

**CC26x0 — the boundary is a vendor toolchain, and only there.** The stack
arrives as `.a` archives. TI's own IAR flow does produce a separate stack image:
`lib_search.exe` generates the link list from `blelib/`, and `frontier.exe`
applies a `.bdef`/`.xcl` boundary in a post-build step. But that split is a
property of their build tools, not of the silicon. Hand-roll the build — which
is the entire premise of this repository — and there is no `frontier` step: the
archives link into your image and the boundary reverts to what it structurally
is, which is none.

It is also worse than a linking question, and this is where the leeching shows.
A radio does not run on its own clock. Everything derives from the system clock,
which is set up by vendor code executing on *your* core, reading factory
calibration values whose semantics the manual withholds. So even if the radio
were sealed off tomorrow, vendor logic would still be inside your firmware,
because the clock tree the radio depends on is vendor-written and cannot be
rewritten from published information. There is no line that excludes it.

Nor does it help that this code only writes numbers into registers. A
calibration constant applied to an oscillator register whose derivation is
undocumented is not merely as opaque as an encrypted blob — it is worse, in two
ways a blob is not.

A blob can be attested. Flash the vendor's image, hash it, compare that against
the copy they published: identical hashes are evidence of provenance that an
auditor can accept without reading a single byte. Calibration constants admit no
such check. They are measured on each die at final test and differ from device
to device, so there is no upstream value to compare against, no hash that means
anything, and nothing to declare in an SBOM beyond "read out of the chip at
runtime".

And they are not incidental. The trim sets the oscillator's frequency accuracy,
which is precisely what certification measures and what an inspection asks
about: the carrier offset decides whether a transmitter stays inside its
allocation. `ccfg.c` shows the shape of it, where
`MODE_CONF_XOSC_CAPARRAY_DELTA` is a signed 8-bit per-device adjustment applied
directly to the trimmed crystal cap-array value. In a diff, in a review, in an
SBOM, that number is indistinguishable from one you chose yourself. The blob at
least announces that it is a blob; the constant says nothing at all.

There is a configuration structure — CCFG, the last 88 bytes of flash — and it
does not help. It is not a register interface that hardware consults; it is a
parameter block that code at a fixed address reads. Every access to it in the
SDK is software: `setup.c` and `osc.c` read it while trimming, `setup_rom.c`
mirrors what the boot ROM does with it, and `flash.c` takes its sector-protection
offsets from it.

That mechanism is not itself a defect, and it would be wrong to present it as
one. Renesas RA parts do the same thing: option-setting memory sits at fixed
addresses in flash, consumed by the boot ROM and by reset-time hardware options,
and FSP fills it from ordinary compile-time constants — `BSP_CFG_OPTION_SETTING_OFS0`
and `OFS1` placed into `.option_setting_ofs0` and `.option_setting_ofs1`. A
flash-resident config block read by immutable ROM is an ordinary design, and on a
part whose fields are documented it is a perfectly workable one. A bare-metal
RA2E1 port that keeps its option bytes in an ordinary `config.c`, with no FSP in
the build at all and device headers generated from a vendor-supplied SVD, is at
[SushiBits/ra2e1-hello](https://github.com/SushiBits/ra2e1-hello).

What differs is what the fields mean. Renesas documents its option bytes, so the
constants that produce them can be chosen, reviewed and defended. CCFG's fields
do not stand on their own. Three of them are defined only as offsets against
values the manual does not document: `MODE_CONF_1.DELTA_IBIAS_INIT` and
`DELTA_IBIAS_OFFSET` are each specified as "See FCFG1:AMPCOMP_CTRL1.IBIAS_INIT"
and "…IBIAS_OFFSET", and `MODE_CONF.VDDR_TRIM_SLEEP_DELTA` as "See
FCFG1:VOLT_TRIM.VDDR_TRIM_SLEEP_H". To set them you need the semantics of the
internal registers, which is precisely what cannot be obtained.

Had those registers been documented, none of this would be necessary. The same
intent would be a handful of `#define`s in a project header — compile-time
constants the compiler folds into a short chain of writes, or read-modify-writes,
against registers whose meaning you can state and defend. That is not a
hypothetical shape: it is what Renesas ships. The structure is not the problem.
The withheld semantics are.

That is the shape of the difference, and it is not about size or licence text.
AT86RF233 puts the box in silicon and STM32WB55 puts it on another core; both
boundaries are facts about the part that survive being rebuilt by anybody. The
CC26x0 puts it inside your image, behind a split that only the vendor's tooling
can produce, with the clock tree — the thing everything else depends on —
vendor-supplied and unspecified. The first is fully auditable. The second is
auditable at its boundary. Only the third leaves you unable to answer the
question at all.

Which is the irony worth recording, because TI's customers for these dev kits
are overwhelmingly companies, and companies are the ones who have to produce
auditable evidence: security review, supply-chain attestation, certification
against functional-safety or medical standards, and — increasingly — software
bill-of-materials obligations. Those obligations attach to the shipped image. A
component that cannot be declared in an SBOM, cannot be reviewed, and cannot be
replaced is a procurement problem before it is an ideological one. The closed
shape here is least suited to exactly the customer it is aimed at.

## Auditability is a delivery requirement, not a preference

That obligation is no longer a matter of taste, and it lands on the integrator,
who these regimes treat as the manufacturer:

- **EU Cyber Resilience Act** (Reg. (EU) 2024/2847) 鈥?in force since December
  2024 with obligations phasing in through 2027: vulnerability handling,
  security updates for a defined support period, and a software bill of
  materials.
- **FDA 搂524B** of the FD&C Act 鈥?premarket cybersecurity for medical devices,
  including an SBOM and a monitoring and patching plan.
- **IEC 62443** for industrial automation and **UN R155** for automotive 鈥?
  component-level transparency pushed down the supply chain.

An SBOM can *name* a binary. It cannot characterise one, and none of these
regimes is satisfied by the vendor's assurance that a blob is fine.

And the radio stack is not an incidental blob. It is remotely reachable. The
precedent is **SweynTooth** (2020): 18 vulnerabilities across the BLE SDKs of
six SoC vendors, producing deadlocks, crashes, buffer overflows and in one case
a bypass of the pairing procedure. **TI was among the affected vendors, with
three CVEs in that set targeting the CC26xx family** 鈥?CVE-2019-17520,
CVE-2019-19193 and CVE-2020-13593.

So the question a compliance function asks is not academic: *when the next one
lands, can you tell whether you are affected, and can you remediate?* Here the
answer is no twice over. The controller cannot be rebuilt, so a fix exists only
at TI's discretion and only if they choose to ship one for this part; and where
logic sits in silicon rather than flash, not even then.

Unpatchable is not an inconvenience. It is a liability with a name attached.
That is why an unauditable radio stack fails security review before it ever
reaches a licensing review 鈥?and why this is a project killer rather than an
aesthetic objection.

## The hardware, and why it is being scrapped

The KiCad sources in `hardware/` are kept for the record. **Do not fabricate
this board.**

The design drives the antenna through an RF front-end module rather than from
the CC2640 directly: an AT2401C PA/LNA behind a 2450BM14G0011 balun and filter,
with the matching network following TI's SWRA117D. That was deliberate 鈥?it was
meant to extend range.

It also takes the board out of the unlicensed regime. With the FEM in the path
the output no longer fits inside the conducted and EIRP limits for an
unlicensed intentional radiator under FCC Part 15, and the same applies to the
equivalents in ETSI and SRRC. A board like this cannot be certified, and cannot
lawfully be operated as a Part 15 device. What it is instead is amateur radio
equipment.

That in itself would be fine. Home-built transmitters are a normal part of the
hobby, and they do not need certification. But amateur operation carries its
own obligations, and one of them is that the station must be open to
inspection: the operator has to be able to account for what the equipment is
and what it emits.

A firmware image that links a radio stack supplied as unreadable archives
cannot be accounted for 鈥?not by the operator to an inspector, and not by
anyone downstream. The clock is not the obstacle here: driverlib's source is
readable, and a mask ROM with a published source equivalent is a flash
optimisation, not a secrecy problem. The stack is the obstacle, because there
is nothing to read, no equivalent to substitute, and the licence forbids
disassembling it to demonstrate what it does.

So the two failures compound. The FEM turned the board into a transmitter that
needs amateur rules in order to be legal at all. The closed radio stack made it
a transmitter that cannot satisfy those rules. Neither is fixable within this
design, which is why the boards are being scrapped rather than shelved.

*This is the reasoning behind the decision, not legal advice; the specifics
vary by jurisdiction.*

## The rest of the record

The sections below are what the attempt produced. They are accurate and are
the reason this repository is published rather than deleted, but none of them
supply a clock setup or a radio stack.

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
PRCM 66, FCFG1 7, IOC 2, and **zero** within 卤400 characters of GPIO, UART,
SSI, I2C, GPT, WDT, UDMA, CRYPTO or TRNG.

So on paper the peripherals are well documented. It does not help, and the
distinction is worth being exact about: a documented register is not a working
peripheral. GPIO, UART, SPI, I2C and the timers all take their timing from the
clock tree, and the clock tree needs driverlib. The registers being described
does not make them reachable without something to set the clock up.


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
is derived from the clock tree that driverlib would have to set up, so it would
not run as built.

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

Continuing means accepting two dependencies: TI's clock code, and 鈥?for
anything using the radio 鈥?TI's precompiled stack. The first is a reasonable
trade; driverlib is BSD-3-Clause, readable and patchable, and reimplementing it
against undocumented calibration data is worse in every way. The second is the
one that matters, because there is no alternative to it at all.

`cc26xxware` (driverlib plus the `hw_*.h` headers) on top of the BLE SDK's
build system is the supported arrangement and it works. What it is not is a
free-standing open stack: the clock has no specification and the radio has no
source. A project content to depend on TI for those two things is perfectly
viable 鈥?it just is not the project this repository was.

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
| `hardware/` | KiCad sources for the CC2640 EVM 鈥?do not fabricate, see above |
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

No TI SDK, BLE stack or other TI object code is included, because none of it
may be redistributed. This repository contains no vendor binaries at all, which
is why it is self-contained and why it cannot use the radio.
