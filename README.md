# fguard

A low-level system integrity utility written in C designed to safeguard the Linux `/etc/fstab` configuration file against potential boot failures caused by human error or unexpected update behaviors.

---

## 📌 Behind the Project: A Learning Journey

*While reading 'How Linux Works' and learning C, I started thinking about the challenges a sysadmin faces daily. I had an Arch Linux VM dedicated entirely to breaking, fixing, and experimenting. While tampering with the `/etc/fstab` file to understand its mount parameters, I realized just how critical this single file is for the system's survival. That is when the idea for `fguard` was born: to create a 'second check' before the computer shuts down.*

*This is not a commercial tool. I have been in the cybersecurity and tech sector for only about 4 months and 2 and a half with the new approach. This project is a handmade, craft utility built by someone who is just starting out, who found a cool problem to solve, and enjoyed every single moment spending time with C. I do not claim solo credit for the entire architecture; much of the code was written by documenting myself deeply and collaborating with AI, breaking down every single line and dash to truly understand the mechanics under the hood and learn how to program from scratch. My goal is to build the most solid foundations possible and understand the 'why' behind everything.*

---

## 🛠️ Features & Architecture

* **Dual-Language Native Source:** Contains separate implementations for both English (`guardianEng.c`) and Spanish (`guardian.c`) environments.
* **Systemd Integration:** Coupled with the system management lifecycle to trigger validation logic automatically during system shutdown.
* **Live Auditing:** Pumps operational status logs directly into the Linux `syslog` subsystem for native tracking.
* **Fault-Tolerant Atomic Writes:** Protects system files against unexpected power cuts by staging modifications in hidden temporary files, committing them with `fsync()`, and swapping them using atomic `rename()` operations.

---

## 🚀 Usage & Deployment

### 1. Create Directory & Download Source
Create a clean directory for the project and ensure you have the source file of your choice (`guardianEng.c` or `guardian.c`), along with the provided `Makefile` positioned inside that same folder:

```bash
mkdir fguard-build && cd fguard-build
# Place your selected source file and the Makefile inside this directory
```

### 2. Compilation & Installation via Makefile
The repository includes a dedicated `Makefile` that automates compilation with optimization flags (`-O2`), sets structural diagnostic checks (`-Wall -Wextra`), and handles system path positioning through the setup routine.

To compile the source code into the final `fguard` binary and automatically install it into your system's execution path, run:

* **For the English Version (`guardianEng.c`):**
  ```bash
  sudo make install_en
  ```

* **For the Spanish Version (`guardian.c`):**
  ```bash
  sudo make install_es
  ```

* **To clean build artifacts and intermediate objects:**
  ```bash
  make clean
  ```

### 3. First-Time Setup (Crucial)
Before activating the automated system service, you must initialize the protection environment to generate the immutable master backup. Run the tool with the `-install` flag:

```bash
sudo fguard -install
```
*Note: Whenever you make legitimate, authorized manual edits to your `/etc/fstab` file (e.g., adding a new drive), remember to update your master backup after modifying your 'etc/fstab' by running `sudo fguard -update`.*

### 4. Uninstalling the Tool
If you need to completely purge the binary, its systemd service configuration, and all associated deployment files from your system, use the cleanup rule:

```bash
sudo make uninstall
```

---

## ⚙️ Systemd Service Integration

To execute this utility automatically every time the computer shuts down or reboots, create a service unit configuration file:

```bash
sudo nano /etc/systemd/system/fguard.service
```

Add the following configuration (make sure `ExecStart` points to your compiled binary absolute path, usually `/usr/local/bin/fguard`):

```ini
[Unit]
Description=fguard Fstab Protection
DefaultDependencies=no

[Service]
Type=oneshot
ExecStart=/usr/local/bin/fguard
RemainAfterExit=yes

[Install]
WantedBy=halt.target poweroff.target reboot.target
```

### Activating the System Daemon
Reload the system manager configuration to register the unit file and enable it for automatic execution during shutdowns:

```bash
# Reload systemd manager configuration
sudo systemctl daemon-reload

# Enable the service to run on shutdown/reboot
sudo systemctl enable fguard
```

---

## 📊 Monitoring & Logs

You can audit the tool's runtime behavior, integrity checks, and backup status in real time through the native systemd journal:

```bash
# Check the current status of the service
sudo systemctl status fguard

# View detailed chronological logs from past integrity checks
sudo journalctl -u fguard
```

---

## ⚠️ Disclaimer
This utility performs operations on critical system configuration files. It is intended strictly for educational purposes and administrative staging environments.

