
# 1. Variable definitions for future modifications
CC = gcc
CFLAGS = -Wall -Wextra -O2 
TARGET = fguard
PREFIX = /usr/local/bin

# Source files for separate language environments
SRC_ES = guardian.c
SRC_EN = guardianEng.c

# 2. Default rule: provides usage guidance if plain 'make' is executed
all:
	@echo "====================================================="
	@echo " fguard Build System "
	@echo "====================================================="
	@echo "Please specify a target language for compilation:"
	@echo "  make install_en  -> Compiles and deploys English version"
	@echo "  make install_es  -> Compiles and deploys Spanish version"
	@echo "  make clean       -> Removes local binary build artifacts"
	@echo "  make uninstall   -> Purges binary and backups from system"
	@echo "====================================================="

# 3. Secure Installation Logic (Deploys with 0755 permissions, no SUID bit)
install_en: $(SRC_EN)
	@echo "[+] Compiling English binary..."
	$(CC) $(CFLAGS) $(SRC_EN) -o $(TARGET)
	@echo "[+] Installing binary to $(PREFIX)..."
	sudo install -m 0755 $(TARGET) $(PREFIX)/$(TARGET)
	@echo "[+] Running initial fstab guardian provisioning setup..."
	sudo $(PREFIX)/$(TARGET) -install

install_es: $(SRC_ES)
	@echo "[+] Compiling Spanish binary..."
	$(CC) $(CFLAGS) $(SRC_ES) -o $(TARGET)
	@echo "[+] Installing binary to $(PREFIX)..."
	sudo install -m 0755 $(TARGET) $(PREFIX)/$(TARGET)
	@echo "[+] Running initial fstab guardian provisioning setup..."
	sudo $(PREFIX)/$(TARGET) -install

# 4. Clean rule for local intermediate compilation artifacts
clean:
	rm -f $(TARGET)

# 5. Complete removal rule (tested for Arch Linux environment)
uninstall:
	@echo "[-] Dropping immutability flag lock from master backup..."
	-sudo chattr -i /etc/fstab.bak 2>/dev/null || true
	@echo "[-] Purging core binary and backup targets from the system root..."
	-sudo rm -f /etc/fstab.bak $(PREFIX)/$(TARGET)
	@echo "[+] Uninstallation complete."

