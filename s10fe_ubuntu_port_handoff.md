# Handoff: Port Ubuntu สำหรับ Samsung Galaxy Tab S10 FE (SM-X520)

**สถานะโครงการ:** Kernel Source Assembly Complete & Build Environment Setup Phase  
**เป้าหมาย:** พัฒนา Ubuntu ARM64 แบบเต็มระบบสำหรับ Samsung Galaxy Tab S10 FE โดยใช้โปรเจกต์ [xiaomipad-6pro-mainline](https://www.google.com/search?q=https://github.com/yzddmr6/xiaomipad-6pro-mainline) เป็นตัวอย่างด้าน Ubuntu root filesystem, GNOME และ build workflow เท่านั้น ไม่ใช้ kernel หรือ Device Tree ของ Xiaomi กับ Samsung

## 1. ข้อมูลอุปกรณ์ (ยืนยันจากเครื่องจริงแล้ว)

| รายการ | ข้อมูล |
| :--- | :--- |
| อุปกรณ์ | Samsung Galaxy Tab S10 FE (Wi-Fi) |
| Model / Codename | `SM-X520` / `gts10fewifi` (`gts10fe`) |
| SoC / Platform | Samsung Exynos 1580 (`S5E8855` / `mx8855` / `erd8855`) |
| Compatible Strings | `"SAMSUNG,GTS10FEWIFI_EUR_OPEN"`, `"SAMSUNG,S5E8855"` |
| GPU | Samsung Xclipse 540 (`SGPU M2_MID` ฐานสถาปัตยกรรม AMD `amdgpu`, เฟิร์มแวร์ `mgfx2.0_mid.bin`) |
| Wi-Fi / Bluetooth | Samsung Maxwell **SCSC WLBT** (`scsc_mx`, release `13.0.0.85`) |
| Architecture | ARM64 (`aarch64`, Page Size `4K`) |
| OS ปัจจุบันบนเครื่องจริง | Android 16 / One UI 8.5 (Security Patch: 5 กันยายน 2026) |
| Build Number เครื่องจริง | `BP4A.251205.006.X520XXSBCZI2` (Binary revision `B`) |
| Kernel Version เครื่องจริง | `6.6.102-android15-8-abX520XXSBCZI2-4k` (`#1 Tue Sep 8 06:56:51 UTC 2026`) |
| CSC / Region | `XSP` (`SAOMC_SM-X520_OLM_XSP_16_0005`, `XSP/XSP/`) |
| SELinux / Knox | Enforcing (`SEPF_SM-X520_16_0001`) / Knox 3.13 (API level 40, HDM 2.0 - 005) |
| Bootloader Unlock | **รองรับการปลดล็อก** (พบเมนู `Long press Volume Up: Device Unlock Mode` ในหน้าเข้า Download Mode; ยังไม่ได้กดปลดล็อก) |
| Development PC | Windows และ Linux |
| เป้าหมาย | Ubuntu เต็มระบบแทน Android |

## 2. สรุปสถานะโครงการ

| ระยะ | สถานะ | งานที่ทำแล้วและงานที่ต้องทำต่อ |
| :--- | :--- | :--- |
| **M1: Hardware Research** | เก็บข้อมูลซอฟต์แวร์เครื่องจริงและแกะไดรเวอร์ครบแล้ว | ได้ Build Number (`X520XXSBCZI2`), Kernel (`6.6.102-4k`), CSC (`XSP`) และรายการชิปครบทุกหมวด (รวมถึง Wi-Fi/BT `SCSC WLBT` และ GPU `sgpu/amdgpu`) เหลือรัน ADB เพื่อเช็กว่าเครื่องนี้ได้จอ `BOE` หรือ `CSOT` |
| **M2: Bootloader Research** | **ยืนยันว่าปลดล็อกได้แล้ว** | เข้าหน้า Warning ของ Download Mode แล้วพบ `Long press Volume Up: Device Unlock Mode` (รอสำรองข้อมูลและเตรียม Stock Firmware ก่อนปลดล็อกจริง) |
| **M3: Kernel Source Assembly** | **รวมซอร์สโค้ดครบทั้ง 3 ชั้นเรียบร้อยแล้ว** | รวมชุด Base (`CZE3`) + SM-X520 DTS (`CZE3`) + Security & Firmware Update (`CZI2`) ลงในโฟลเดอร์ `Kernel/` พร้อมตั้งค่า `SCMVERSION` เป็น `-abX520XXSBCZI2` ตรงกับเคอร์เนลเครื่องจริงสำเร็จ |
| **M4: Kernel Build** | **เชื่อมต่อสภาพแวดล้อม Bazel/Toolchain เรียบร้อยแล้ว** | เชื่อม Symlink AOSP Toolchain (`build`, `external`, `prebuilts/clang` ฯลฯ) จาก `~/s10fe-linux/aosp-kernel-toolchain` เข้ากับ `Kernel/` และทดสอบคำสั่ง Bazel (`mx8855_dist`) สำเร็จเรียบร้อย เหลือปรับแก้ `defconfig` และทดสอบคอมไพล์ |
| **M5: Device Tree Bring-up** | ได้ไฟล์ `.dtso` / `.dtsi` ต้นฉบับครบแล้ว | ตรวจสอบ `gts10fewifi_eur_open_w00_r00..r05.dtso` และวางแผนปรับแต่งสำหรับ Linux userspace |
| **M6: First Linux Boot** | ยังไม่เริ่ม | ยืนยัน boot path, โครงสร้างพาร์ทิชัน `boot`/`vendor_boot`/`dtbo` และเตรียมไฟล์ stock firmware `X520XXSBCZI2` (`XSP`) สำหรับ recovery ก่อนทดสอบ |

## 3. แหล่งข้อมูลและ Repository

- Xiaomi Ubuntu reference: https://www.google.com/search?q=https://github.com/yzddmr6/xiaomipad-6pro-mainline
- Samsung Open Source Release Center (ค้นหา `X520XXSBCZI2`): https://www.google.com/search?q=https://opensource.samsung.com/uploadSearch%3FsearchValue%3DX520XXSBCZI2
- AOSP Kernel Build Guide: https://www.google.com/search?q=https://source.android.com/docs/setup/build/building-kernels
- Linux Kernel: https://www.google.com/search?q=https://www.kernel.org/
- Linux mainline source: https://www.google.com/search?q=https://github.com/torvalds/linux

### ข้อควรทราบ

- โปรเจกต์ Xiaomi Pad 6 Pro ใช้ Qualcomm SM8475 จึงไม่สามารถนำ kernel image หรือ Device Tree มาแฟลชบน SM-X520 ได้
- สามารถนำแนวทาง Ubuntu rootfs, GNOME และ build workflow มาเป็นต้นแบบได้
- เคอร์เนลของเครื่องจริงทำงานแบบ **Page Size 4K** (`6.6.102-android15-8-abX520XXSBCZI2-4k`) ซึ่งเข้ากันได้กับแพ็กเกจ Ubuntu ARM64 มาตรฐานทั้งหมดทันที
- บอร์ด `gts10fewifi` มีการใช้ชิ้นส่วนหน้าจอและทัชสกรีนจากผู้ผลิต 2 ราย (Dual-source: BOE และ CSOT) ต้องตรวจสอบจากเครื่องจริงผ่าน ADB ว่าใช้ชิ้นส่วนชุดใด

## 4. M1 — Hardware Research & Hardware Compatibility Matrix

### 4.1 ข้อมูลซอฟต์แวร์จากแท็บเล็ตจริง (บันทึกผลแล้ว)

- **Model number:** `SM-X520`
- **Android version / One UI version:** Android 16 / One UI 8.5
- **Google Play System Update:** 1 กันยายน 2026
- **Build number:** `BP4A.251205.006.X520XXSBCZI2`
- **Kernel version:** `6.6.102-android15-8-abX520XXSBCZI2-4k` (`#1 Tue Sep 8 06:56:51 UTC 2026`)
- **Service provider software version / CSC:** `SAOMC_SM-X520_OLM_XSP_16_0005` (`XSP/XSP/`)
- **Android Security Patch Level:** 5 กันยายน 2026 (`SMR Sep-2026 Release 1`, `ASKS v9.5 Release 20260814`, `ADP v3.5 Release 20260312`)

### 4.2 เก็บข้อมูลฮาร์ดแวร์เชิงลึกผ่าน ADB

เปิด USB debugging และเชื่อมต่อแท็บเล็ตกับ Linux PC จากนั้นรันคำสั่งต่อไปนี้เพื่อดูรุ่นพาเนลจอ (`BOE` vs `CSOT`) และรีวิชันบอร์ด:

```bash
mkdir -p s10fe-research

adb devices

adb shell getprop > s10fe-research/getprop.txt
adb shell uname -a > s10fe-research/uname.txt
adb shell cat /proc/cmdline > s10fe-research/cmdline.txt
adb shell cat /proc/version > s10fe-research/kernel-version.txt
adb shell cat /proc/cpuinfo > s10fe-research/cpuinfo.txt
adb shell cat /proc/meminfo > s10fe-research/meminfo.txt
adb shell lsmod > s10fe-research/lsmod.txt

adb shell dumpsys SurfaceFlinger > s10fe-research/surfaceflinger.txt
adb shell dumpsys battery > s10fe-research/battery.txt
```

ตรวจสอบ Device Tree และโมดูลหน้าจอ/ทัชสกรีนที่โหลดอยู่:

```bash
adb shell 'cat /sys/firmware/devicetree/base/model 2>/dev/null; echo'
adb shell 'cat /sys/firmware/devicetree/base/compatible 2>/dev/null; echo'
adb shell 'lsmod | grep -E "mcd|panel|himax|novatek|wacom|stm32|scsc|sgpu"'
```

### 4.3 Hardware Inventory (สกัดจากชุดซอร์สโค้ด `X520XXU7CZE3` และ `X526NKOSBCZI2`)

| กลุ่มฮาร์ดแวร์ | ชิป / อุปกรณ์ที่ตรวจพบ | ไดรเวอร์ / โมดูล (`.ko`) / ซอร์สโค้ด | เฟิร์มแวร์ (`.bin`) และรายละเอียดเพิ่มเติม | ความสำคัญ |
| :--- | :--- | :--- | :--- | :--- |
| **SoC / Platform** | Samsung Exynos 1580 (`S5E8855` / `mx8855` / `erd8855`) | Base GKI 6.6.102 (4K page size) + Samsung SoC modules | Thermal zones 7 โซน: `BIG`, `MID`, `LITTLE`, `G3D`, `ISP`, `NPU`, `battery` | สูงมาก |
| **GPU** | Samsung Xclipse 540 (`SGPU M2_MID` ฐานสถาปัตยกรรม AMD RDNA) | `CONFIG_DRM_SGPU=m`, `exynos/external-modules/sgpu/sgpu/amdgpu_*.c` | เฟิร์มแวร์ `sgpu/mgfx2.0_mid.bin` (ถูกอัปเดตเวอร์ชันใหม่ในแพ็กเกจ `CZI2`) | สูงมาก |
| **Display (LCD)** | พาเนล TFT WUXGA `1440×2304` (90Hz / 60Hz / 30Hz, Video Mode + DSC) มี 2 ผู้ผลิต:<br>1) **BOE HX83102J**<br>2) **CSOT NT36523N** | `exynos_drm_dqe.c`, `exynos_drm_hdr.c`, `mcd-panel.ko`, `usdm-panel-tft-common.ko`,<br>`mcd-panel-hx83102j_gts10fe_boe.ko`,<br>`mcd-panel-nt36523n_gts10fe_csot.ko` | ชิปคุมไฟหน้าจอ (BLIC) ใช้ **MAX77816** ผ่าน I2C (`CONFIG_USDM_BLIC_I2C=y`) | สูงมาก |
| **Touchscreen** | ทัชสกรีนผ่านบัส SPI แยกตามรุ่นจอ:<br>1) **Himax HX83102** (คู่จอ BOE)<br>2) **Novatek NT36523** (คู่จอ CSOT) | `himax_ts_83xxx_spi.ko` (`CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102=m`, zero-flash),<br>`novatek_ts_nt36523.ko` (`nvt_ts_spi`) | โหลดเฟิร์มแวร์ทัชผ่านไดรเวอร์ SPI ตอนเริ่มต้นทำงาน | สูงมาก |
| **Wi-Fi / Bluetooth** | **Samsung Maxwell SCSC WLBT** (`wlbt` subsystem) | `drivers/misc/samsung/scsc/scsc_mx_impl.c` (`SCSC_RELEASE_PRODUCT 13`, `POINT 85`) | ใช้สถาปัตยกรรม WLBT ของ Samsung (ต้องใช้บริการ `wlbtd` หรือตั้งค่า firmware path จากพาร์ทิชัน `vendor` ตอนบูต Ubuntu) | สูง |
| **S Pen Digitizer** | **Wacom WEZ01** (`wacom,wez01`, chip-on-board) | `wez01.ko` (`CONFIG_EPEN_WACOM_WEZ01=m`) | เชื่อมต่อผ่าน I2C พร้อมสาย GPIO สำหรับ PDCT / FWE / IRQ | ปานกลาง |
| **Pogo Keyboard** | **STM32 Pogo v3** (`stm,stm32_pogo` @ I2C `0x2a`) | `stm32_pogo_v3.ko` (`CONFIG_KEYBOARD_STM32_POGO_V3=m`) | ใช้เฟิร์มแวร์ `keyboard_stm/stm32_gts10family.bin` รองรับทั้ง `stm,touchpad` และ `stm,keypad` | ปานกลาง |
| **Storage** | UFS Controller และ MMC/SD | `CONFIG_SEC_UFS=y`, `CONFIG_SEC_MMC=y` | Built-in ในเคอร์เนลหลัก พร้อม I/O scheduler `MQ_IOSCHED_SSG` | สูงมาก |
| **PMIC & Regulators** | PMIC หลัก **S2MPU15** + PMIC ย่อย **S2MPB03**, **S2DOS05** | `pmic_S2MPU15.dtsi`, `CONFIG_REGULATOR_S2MPB03=m`, `CONFIG_REGULATOR_S2DOS05=m` | ควบคุมแรงดันไฟหลักของบอร์ดและอุปกรณ์ต่อพ่วง | สูงมาก |
| **Battery & Charging** | **Silicon Mitus SM5714** (Charger/Fuelgauge/MFD) + **SM5440** (Direct Charger) | `mfd_sm5714.ko`, `sm5714-charger.ko`, `sm5714_fuelgauge.ko`, `sm5440-charger.ko`, `sec-battery.ko` | มีชิปยืนยันตัวตนแบตเตอรี่ 2 รุ่น: **DS28E30** (`sec-auth-ds28e30.ko`) และ **SLE956681** (`sec-auth-sle956681.ko`) | สูง |
| **USB-C / PD / Repeater** | **SM5714** (Type-C PD 3.0 DRP & MUIC) + USB eUSB2/Redriver Repeater | `pdic_sm5714.ko`, `muic_sm5714.ko` (`sm5714-muic.c` อัปเดตใน `CZI2`), `repeater_ptn3222_module.ko`, `repeater_tusb2e11_module.ko` | รองรับ USB Repeater 2 รุ่น: **NXP PTN3222** และ **TI TUSB2E11** | สูง |
| **Audio** | Exynos8855 ABOX + Smart Amp **TI TAS25XX** | `CONFIG_SND_SOC_SAMSUNG_EXYNOS8855_AUDIO=m`, `CONFIG_SND_SOC_SAMSUNG_EXYNOS_TAS25XX=m` | ใช้ระบบเสียง Samsung ABOX ร่วมกับแอมป์ลำโพงตระกูล TAS25xx | ปานกลาง |
| **Sensors & Biometrics** | SensorHub **SHUB** + Fingerprint **Goodix GW3X** + Grip Sensor **ISG6320** + Hall IC | `shub.ko`, `fingerprint.ko` (`CONFIG_SENSORS_GW3X=m`), `isg6320.ko`, `hall_ic.ko` | เฟิร์มแวร์ `sensorhub/shub_santa_gts10fe.bin` (ถูกอัปเดตเวอร์ชันใหม่ในแพ็กเกจ `CZI2`) | ปานกลาง |
| **Multimedia / NPU / Camera** | MFC Video Codec v17.x, NPU AIE, กล้อง **OV13A10** & **HI1337** (Actuator **DW9808**) | `CONFIG_VIDEO_EXYNOS_PABLO_ISP=m`, `CONFIG_CAMERA_CIS_OV13A10_OBJ=m`, `CONFIG_CAMERA_CIS_HI1337_OBJ=m` | เฟิร์มแวร์ `mfc_fw_v17.x.bin` และ `npu/AIE.bin` (ถูกอัปเดตเวอร์ชันใหม่ในแพ็กเกจ `CZI2`) | ต่ำ–ปานกลาง |

## 5. M2 — Bootloader Research (ยืนยันแล้ว)

ผลการตรวจสอบจากเครื่องจริง:

1. ในเมนู **ทางเลือกผู้พัฒนา (Developer options)** บน Android 16 (One UI 8.5) ไม่แสดงสวิตช์ OEM unlocking
2. แต่เมื่อปิดเครื่องแล้วกด **Volume Up + Volume Down** ค้างขณะเสียบสาย USB เข้าหน้า Warning ของ Download Mode พบตัวเลือก **`Long press Volume Up: Device Unlock Mode`** อย่างชัดเจน ซึ่งยืนยันว่าเครื่องสามารถปลดล็อก Bootloader ได้ผ่านหน้านี้โดยตรง
3. สถานะปัจจุบัน: **ยังไม่ได้กดปลดล็อก** เพื่อรักษาข้อมูลและรอให้เตรียมไฟล์ Stock Firmware (`X520XXSBCZI2` / CSC `XSP`) พร้อมอิมเมจทดสอบให้เสร็จก่อน

**ข้อควรระวังก่อนกดปลดล็อกจริงในอนาคต**

- การกด `Long press Volume Up` เพื่อ Unlock Bootloader จะล้างข้อมูลในเครื่องทั้งหมดทันที (Factory Reset)
- การแฟลช custom image จะทำให้ Knox Warranty Bit เปลี่ยนสถานะเป็น `0x1` อย่างถาวร
- ห้ามแฟลช Stock Firmware ที่มีเลข Binary ต่ำกว่า **`B`** (ต้องใช้ Stock Firmware รหัส **`X520XXSBCZI2`** หรือ Binary `B` ขึ้นไปเท่านั้น)

## 6. M3 — Samsung Kernel Source Analysis (วิเคราะห์ครบทั้ง `CZE3` และ `CZI2` แล้ว)

จากการแกะและเปรียบเทียบไฟล์ทั้งสองชุดที่คุณส่งมา (`SM-X520_16_Opensource_dts.zip` และ `SM-X526N_16_Opensource_..._X520XXSBCZI2_...zip`) พบว่าแต่ละไฟล์ทำหน้าที่ต่างกันอย่างชัดเจน และ **ต้องแตกไฟล์ทับตามลำดับที่ถูกต้องเท่านั้น**:

### 6.1 เปรียบเทียบสิ่งที่อยู่ในแต่ละแพ็กเกจ

| ชื่อไฟล์ซิป | ไฟล์ `.tar.gz` ด้านใน | บทบาทและสิ่งที่บรรจุอยู่ภายใน |
| :--- | :--- | :--- |
| **`SM-X526N_16_Opensource.zip`** | `X526NKOU7CZE3_kernel.tar.gz` | **ชั้นที่ 1 (Base Kernel Source):** ซอร์สโค้ดเต็มของ Linux 6.6 และไดรเวอร์ Exynos 1580 (`mx8855`) ทั้งหมด แต่ตั้งค่าเริ่มต้นเป็นของรุ่น 5G (`SM-X526N`) |
| **`SM-X520_16_Opensource_dts.zip`** | `X520XXU7CZE3_kernel.tar.gz` | **ชั้นที่ 2 (SM-X520 Device & DTS Overlay):** เพิ่มไฟล์เฉพาะบอร์ด Wi-Fi (`gts10fewifi`) ได้แก่ `.dtso` ทั้ง 5 รีวิชัน, `gts10fewifixx_00_defconfig`, `s5e8855-bazel_defconfig`, `lego.bzl`, `lego_dtbo`, `lego_module` และไดรเวอร์ `battery_auth` |
| **`SM-X526N_16_Opensource_..._X520XXSBCZI2_...zip`** | `X526NKOSBCZI2_kernel.tar.gz` | **ชั้นที่ 3 (Security & Firmware Update `CZI2`):** แพตช์อัปเดตโค้ด C/H และเฟิร์มแวร์ไบนารีจากรุ่น `CZE3` เป็น `CZI2` (อัปเดต `iommu.c`, `hid`, `usb/gadget`, `exynos_drm_dqe/hdr.c`, `scsc_mx_impl.c`, `sm5714-muic.c`, `amdgpu_*.c` และอัปเดตไฟล์ไบนารี `.bin` ทั้งหมดใน `prebuilts/platform/erd8855/firmware/`) **โดยไม่มีไฟล์ `.dtso` หรือ `lego.bzl` ของ SM-X520 อยู่ในซิปนี้** |

### 6.2 ลำดับการแตกไฟล์ที่ถูกต้อง (ห้ามสลับลำดับ)

หากแตกไฟล์สลับลำดับ ไฟล์ไบนารีเฟิร์มแวร์ `.bin` รุ่นเก่าของ `CZE3` จะไปทับรุ่นใหม่ของ `CZI2` หรือหากไม่ลงชั้นที่ 2 ก็จะไม่มีไฟล์ `.dtso` และ `defconfig` ของ `SM-X520` ดังนั้นบนเครื่อง Linux ต้องรันตามลำดับนี้:

```bash
mkdir -p ~/s10fe-linux/kernel/samsung-source
cd ~/s10fe-linux/kernel/samsung-source

# 1. แตก Base Source ของตระกูล Tab S10 FE (X526NKOU7CZE3)
tar -xzf /path/to/X526NKOU7CZE3_kernel.tar.gz

# 2. แตกไฟล์เฉพาะบอร์ด SM-X520 Wi-Fi (X520XXU7CZE3) ทับลงไป
tar -xzf /path/to/X520XXU7CZE3_kernel.tar.gz

# 3. แตกไฟล์อัปเดตความปลอดภัยและเฟิร์มแวร์รุ่น CZI2 (X526NKOSBCZI2) ทับเป็นลำดับสุดท้าย
tar -xzf /path/to/X526NKOSBCZI2_kernel.tar.gz

# 4. ปรับ SCMVERSION ใน workspace_status.json ให้ตรงกับเคอร์เนล SM-X520 บนเครื่องจริง
cat << 'EOF' > Kernel/kernel-6.6/workspace_status.json
{"SCMVERSION":"-abX520XXSBCZI2","SOURCE_DATE_EPOCH":1788864812}
EOF
```

*หมายเหตุสำคัญเรื่องข้อ 4:* ในไฟล์ `X526NKOSBCZI2_kernel.tar.gz` ค่า `SCMVERSION` ถูกตั้งไว้เป็น `-abX526NKOSBCZI2` (ซึ่งมี `SOURCE_DATE_EPOCH: 1788864812` ตรงกับวันที่ `Tue Sep 8 06:53:32 UTC 2026` วันเดียวกับเคอร์เนลบนเครื่องจริงของคุณ) การแก้ชื่อเป็น `-abX520XXSBCZI2` จะทำให้สตริงเวอร์ชันเคอร์เนลที่คอมไพล์ออกมาตรงกับ `6.6.102-android15-8-abX520XXSBCZI2-4k` ของเครื่องจริงทุกตัวอักษร

## 7. M4 — เตรียม Linux Build Environment

แนะนำใช้ Ubuntu 24.04 LTS (x86_64) หรือ WSL2 (ที่มีพื้นที่ดิสก์เพียงพอ) เป็นเครื่อง build หลัก โดยเตรียมพื้นที่ว่างอย่างน้อย 100–150 GB สำหรับ AOSP Kernel Toolchain + Samsung Kernel Source

ติดตั้งแพ็กเกจพื้นฐาน:

```bash
sudo apt update

sudo apt install -y \
  git repo curl wget rsync \
  build-essential bc bison flex \
  libssl-dev libelf-dev \
  dwarves device-tree-compiler \
  python3 python3-pip \
  gcc-aarch64-linux-gnu \
  g++-aarch64-linux-gnu \
  clang lld llvm \
  cpio gzip xz-utils lz4 \
  adb fastboot
```

## 8. M4 — Kernel Build Workflow (Bazel / Kleaf)

### 8.1 การเตรียม Toolchain และโครงสร้าง Bazel

Samsung ใช้ระบบคอมไพล์ **Bazel (Kleaf)** บนฐาน AOSP `common-android15-6.6`:

1. ดึง Toolchain และ Build environment จาก AOSP Kernel manifest (`common-android15-6.6`) หรือดาวน์โหลดแพ็กเกจ `toolchain` จาก Samsung Open Source:
   ```bash
   mkdir -p ~/s10fe-linux/aosp-kernel-toolchain
   cd ~/s10fe-linux/aosp-kernel-toolchain
   repo init -u https://android.googlesource.com/kernel/manifest -b common-android15-6.6
   repo sync -c -j"$(nproc)"
   ```
2. คัดลอกหรือ Symlink โฟลเดอร์ `build/`, `external/`, `prebuilts/` และ `tools/` มาไว้ในโฟลเดอร์ `Kernel/` (ที่ได้จากการรวมซอร์สโค้ด 3 ชั้นในหัวข้อ 6.2) และสร้าง Symlink ให้ `kernel-6.6` ชี้เป็น `common`:
   ```bash
   cd ~/s10fe-linux/kernel/samsung-source/Kernel
   ln -s $(pwd)/kernel-6.6 common
   ```

### 8.2 คำสั่งคอมไพล์มาตรฐานของ Samsung

```bash
tools/bazel run --config=stamp --config=mx8855_user //exynos/device/mx/mx8855:mx8855_dist
```

ไฟล์ผลลัพธ์ทั้งหมด (`Image`, `dtbo.img`, และไฟล์ `.ko` ทั้งหมดที่ระบุใน `lego.bzl`) จะถูกสร้างไว้ที่:

```text
out/mx8855/dist/
```

### 8.3 การปรับแก้ Kernel Config สำหรับบูต Ubuntu ARM64

ในไฟล์ `s5e8855-bazel_defconfig` มีระบบรักษาความปลอดภัยเฉพาะของ Samsung/Android ที่บล็อกการรันโปรแกรมนอกเหนือจาก Android ก่อนนำไปบูต Ubuntu ต้องปรับแก้ `s5e8855-bazel_defconfig` ดังนี้:

- **ปิดระบบป้องกันของ Samsung ที่ขัดขวาง Linux rootfs:**
  ```text
  # CONFIG_SECURITY_DEFEX is not set
  # CONFIG_PROCA is not set
  # CONFIG_FIVE is not set
  # CONFIG_KNOX_NCM is not set
  # CONFIG_SECURITY_KUMIHO is not set
  # CONFIG_SECURITY_SEATD is not set
  # CONFIG_SECURITY_DSMS is not set
  ```
- **เปิดฟีเจอร์ที่จำเป็นสำหรับ Desktop Ubuntu / `systemd`:**
  - ตรวจสอบให้แน่ใจว่าเปิด `CONFIG_DEVTMPFS=y`, `CONFIG_CGROUPS=y`, `CONFIG_INOTIFY_USER=y`, `CONFIG_TMPFS_POSIX_ACL=y`, `CONFIG_SECCOMP=y`, `CONFIG_NETFILTER=y`, `CONFIG_VETH=y` และ `CONFIG_VT=y` / `CONFIG_FRAMEBUFFER_CONSOLE=y`

## 9. M5 — Device Tree Bring-up

ในแพ็กเกจ `SM-X520_16_Opensource_dts.zip` มีไฟล์ Device Tree Overlay (`.dtso`) และ Include (`.dtsi`) ภายใต้พาธ `Kernel/exynos/device/mx/mx8855/arch/arm64/boot/dts/samsung/` ดังนี้:

| ชื่อไฟล์ | บทบาทและรายละเอียด |
| :--- | :--- |
| `gts10fewifi/gts10fewifi_eur_open_w00_r00.dtso` | Overlay สำหรับบอร์ด SM-X520 ฮาร์ดแวร์รีวิชัน `r00` |
| `gts10fewifi/gts10fewifi_eur_open_w00_r01.dtso` | Overlay สำหรับบอร์ด SM-X520 ฮาร์ดแวร์รีวิชัน `r01` |
| `gts10fewifi/gts10fewifi_eur_open_w00_r03.dtso` | Overlay สำหรับบอร์ด SM-X520 ฮาร์ดแวร์รีวิชัน `r03` |
| `gts10fewifi/gts10fewifi_eur_open_w00_r04.dtso` | Overlay สำหรับบอร์ด SM-X520 ฮาร์ดแวร์รีวิชัน `r04` |
| `gts10fewifi/gts10fewifi_eur_open_w00_r05.dtso` | Overlay สำหรับบอร์ด SM-X520 ฮาร์ดแวร์รีวิชัน `r05` ถึง `r32` (`dtbo-hw_rev = <5>; dtbo-hw_rev_end = <32>;`) |
| `pmic_S2MPU15.dtsi` | กำหนดค่า PMIC `S2MPU15` |
| `sensors_isg6320.dtsi` | กำหนดค่า Grip / SAR sensor `ISG6320` |
| `sec_thermistor.dtsi` | กำหนดค่าเซนเซอร์วัดอุณหภูมิบนบอร์ด (`sec-thermistor`) |
| `sec_auth_ds28e30.dtsi` / `sec_auth_sle956681.dtsi` | กำหนดค่า GPIO 1-Wire / SWI สำหรับชิปยืนยันตัวตนแบตเตอรี่ |

## 10. M6 — First Linux Boot

ก่อนทดสอบต้องมี:

- ไฟล์ Stock Firmware ครบชุด (`BL`, `AP`, `CSC`) รหัส **`X520XXSBCZI2`** สำหรับภูมิภาค **`XSP`** เพื่อใช้แฟลชกลับผ่าน Odin
- ความเข้าใจโครงสร้างพาร์ทิชันของ Android 16 (GKI 6.6) บน SM-X520 (`boot`, `init_boot`, `vendor_boot`, `dtbo`, `vbmeta`, `vendor_dlkm`, `super`)
- `initramfs` ที่บรรจุโมดูล `.ko` ที่จำเป็นต่อการบูตและแสดงผล (ตามลำดับ `lego_module_list_second` และ `lego_module_list` ใน `lego.bzl`) พร้อมเฟิร์มแวร์รุ่นใหม่จากแพ็กเกจ `CZI2` ใน `/lib/firmware` (`mgfx2.0_mid.bin`, `shub_santa_gts10fe.bin`, `stm32_gts10family.bin`)

## 11. การแบ่งหน้าที่ Windows และ Linux

| ระบบ / เครื่องมือ | งานที่เหมาะสม |
| :--- | :--- |
| **Windows** | ติดตั้ง Samsung USB Driver, Smart Switch และ Odin สำหรับแฟลช Stock Firmware (`X520XXSBCZI2` / `XSP`) กู้คืนเมื่อจำเป็น |
| **Linux (Ubuntu 24.04 / WSL2)** | รัน ADB, ดึง AOSP Kernel toolchain, คอมไพล์เคอร์เนลด้วย Bazel, แก้ไข Device Tree และสร้าง Ubuntu ARM64 rootfs + `initramfs` |
| **Git** | เก็บประวัติการแก้ `defconfig`, `.dtso` และสคริปต์สร้างอิมเมจ |
| **Python** | วิเคราะห์ log, ตรวจสอบลำดับ dependency ของโมดูล `.ko` (`modules.dep`) และจัดการไฟล์อิมเมจ |

## 12. สรุปข้อมูลสถานะปัจจุบัน (Checklist ข้อมูลเครื่องและซอร์สโค้ด)

| รายการ | สถานะ / ข้อมูลที่ยืนยันแล้ว |
| :--- | :--- |
| **Build Number เครื่องจริง** | **`BP4A.251205.006.X520XXSBCZI2`** (Android 16 / One UI 8.5, Binary `B`) |
| **Kernel Version เครื่องจริง** | **`6.6.102-android15-8-abX520XXSBCZI2-4k`** (Page size 4K) |
| **CSC / Region เครื่องจริง** | **`XSP`** (`SAOMC_SM-X520_OLM_XSP_16_0005`) |
| **Bootloader Unlock** | **ยืนยันแล้วว่าปลดล็อกได้** (`Long press Volume Up: Device Unlock Mode` ใน Download Mode) |
| **Kernel Source ชั้นที่ 2 (DTS SM-X520)** | วิเคราะห์แล้ว (`SM-X520_16_Opensource_dts.zip` → `X520XXU7CZE3_kernel.tar.gz`) |
| **Kernel Source ชั้นที่ 3 (Update `CZI2`)** | วิเคราะห์แล้ว (`SM-X526N_16_Opensource_..._X520XXSBCZI2_...zip` → `X526NKOSBCZI2_kernel.tar.gz`) |
| **Kernel Source ชั้นที่ 1 (Base Source)** | **แตกไฟล์รวม 3 ชั้นใน `Kernel/` สำเร็จเรียบร้อย** |
| **รุ่นพาเนลจอของเครื่องจริง (`BOE` vs `CSOT`)** | รอรันคำสั่ง ADB เพื่อตรวจสอบ |
| **Stock Firmware Backup** | รอดาวน์โหลดรหัส **`X520XXSBCZI2`** (CSC: **`XSP`**) เก็บไว้เป็นแผนสำรอง |

## 13. งานถัดไป (Immediate Next Actions)

- [x] แตกไฟล์และวิเคราะห์ `SM-X520_16_Opensource_dts.zip` (`X520XXU7CZE3`)
- [x] แตกไฟล์และวิเคราะห์ `SM-X526N_16_Opensource_..._X520XXSBCZI2_...zip` (`X526NKOSBCZI2`) พร้อมเปรียบเทียบความต่างกับ `CZE3`
- [x] ตรวจสอบ Build Number (`X520XXSBCZI2`), Kernel (`6.6.102-4k`) และ CSC (`XSP`) จากแท็บเล็ตจริง
- [x] ยืนยันความสามารถในการปลดล็อก Bootloader (`Long press Volume Up: Device Unlock Mode`)
- [x] ดาวน์โหลดไฟล์ฐาน **`SM-X526N_16_Opensource.zip`** ลงบนเครื่อง Linux PC แล้วแตกไฟล์รวม 3 ชั้นตามขั้นตอนในหัวข้อ 6.2 (สำเร็จแล้วใน `Kernel/`)
- [ ] รันคำสั่ง ADB ในหัวข้อ 4.2 เพื่อดูว่าเครื่องจริงใช้จอ `BOE HX83102J` หรือ `CSOT NT36523N`
- [ ] ดาวน์โหลด Stock Firmware รหัส **`X520XXSBCZI2`** (CSC **`XSP`**) เก็บไว้สำหรับ Odin
- [x] ตั้งค่าสภาพแวดล้อม Bazel (`common-android15-6.6`) และ Toolchain บนเครื่อง Linux (เชื่อมโยงและทดสอบ target `mx8855_dist` สำเร็จแล้ว)
- [ ] ปรับแก้ `defconfig` สำหรับ Ubuntu และทดลองสั่งคอมไพล์เคอร์เนลรอบแรก (`tools/bazel run ...`)
