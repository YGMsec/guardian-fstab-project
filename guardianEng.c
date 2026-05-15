#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

int main(int argc, char *argv[]) {
	
	// 1) Declare common File Descriptos and buffers
	int fd_fstab, fd_bak, fd_tmp;
	char byte_fstab, byte_bak;
	int r1, r2;
	int error_detectado = 0;


	// Terminal argument control
	// If the user provides more than one option, alert them of the error
	if (argc > 2) {
		printf("\n[-] Error: too many arguments. Use -h or --help to open the help panel.\n");
		return 1;
	}


	// If exactly one additional option is provided (argc == 2)
	if (argc == 2) {
		// Option A: Help panel 
		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
			printf("\n=== Fstab Guardian Help Panel ===\n");
			printf("Usage: sudo fguard [Option]\n\n");
			printf("\t-h, --help\t\tDisplays the help panel.\n");
			printf("\t-install\t\tCreates the initial backup and configures the environment.\n");
			printf("\t-update\t\t\tLegitimately updates the fstab backup after disk changes.\n");
			printf("\t(sin opciones)\t\tNormal operation mode.\n\n");
			return 0;
		}
		// Option B: Installer mode. Creates the backup and grants immutability. 
        // (If updated later, immutability will be lifted, changes applied, and reapplied). 
		else if (strcmp(argv[1], "-install") == 0) {
			printf("[+] Starting automatic installer mode...\n");

			// Comprobamos si el backup ya existe con access()
			if (access("/etc/fstab.bak", F_OK) == 0) {
				printf("[!] Warning: A backup file already exists at /etc/fstab.bak .\n");
				printf("[+] If you want to update it with legitimate changes, use the -update flag.\n");
				return 0;
			}

			printf("[+] No previous backup detected. Creating initial copy...\n");
			// The original file will be cloned to .bak using ioctl() in the next step, creating a temporary file first to prevent corruption of data in case of unexpected power loss for example. 
			// *commit, added block
			// Open the original file and create the temporary file for the backup 
			fd_fstab = open("/etc/fstab", O_RDONLY);
			fd_tmp = open("/etc/.fstab.bak.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
				
			if (fd_fstab == -1 || fd_tmp == -1) {
				printf("[-] Critical Error: could not configure the instalation environment.\n");
				if (fd_fstab != -1) close(fd_fstab);
				if (fd_tmp != -1) close(fd_tmp);
				return 1;
			}

			// Copy bytes to the temporary backup file 
			while ((r1 = read(fd_fstab, &byte_fstab, 1)) > 0) {
				write(fd_tmp, &byte_fstab, 1);
			}

			// Physical disk synchronization and flushing streams
			fsync(fd_tmp);
			close(fd_fstab);
			close(fd_tmp);

			// Replace the temporary backup file with the actual backup target
			if (rename("/etc/.fstab.bak.tmp", "/etc/fstab.bak") != 0) {
				printf("[!] Error crítico del sistema operativo al generar el archivo de backup.\n");
				return 1;
			}
			
			// Low-level: apply the native immutable (+i) attribute
			int attr_flags;

			// Open the new backup in read-only mode to read and modify inode metadata
			fd_bak = open("/etc/fstab.bak", O_RDONLY);
			if (fd_bak ==  -1) {
				printf("[-] Error opening backup file to apply immutability\n");
				return 1;
			}

			// Retrieve current inode flags from the kernel
			ioctl(fd_bak, FS_IOC_GETFLAGS, &attr_flags);
			
			// Enable the specific immutability bit using the bitwise OR operator
			attr_flags |= FS_IMMUTABLE_FL;

			// Command the kernel to commit the modified flags to disk
			if (ioctl(fd_bak, FS_IOC_SETFLAGS, &attr_flags) == 0) {
				printf("[+] Backup successfully configured with its immutability flag.\n");
			} else {
				printf("[-] Alert: could not apply the immutability attribute to the backup.\n");
			}

			close(fd_bak);
			return 0;
		}

		// Option C: Update mode
		else if (strcmp(argv[1], "-update") == 0) {
			printf("[+] Starting secure fstab update...\n");
			
			// added block
			int attr_flags;

			// Step 1: Open current backup (read-only is sufficient to read/write inode flags)
			fd_bak = open("/etc/fstab.bak", O_RDONLY);
			if (fd_bak == -1) {
				printf("[-] Error: could not open /etc/fstab.bak. Did you run -install first? (-h or --help for help)\n");
				return 1;
			}

			// Step 2: Fetch inode bits from the kernel into RAM
			if (ioctl(fd_bak, FS_IOC_GETFLAGS, &attr_flags) < 0) {
				printf("[-] Error: could not read the backup attributes.\n");
				close(fd_bak);
				return 1;
			}

			// Step 3: bitwise binary operation to clear the immutability bit
			attr_flags &= ~FS_IMMUTABLE_FL; // '~' represents the bitwise NOT operator
			

			// Step 4: send modified flags back back to the kernel to unlock the file
			if (ioctl(fd_bak, FS_IOC_SETFLAGS, &attr_flags) < 0) {
				printf("[-] Error: the kernel denied lifting backup immutability.\n");
				close(fd_bak);
				return 1;
			}

			// Close file descriptor to ensure the filesystem commits changes to disk 
			close(fd_bak);
			printf("[+] Backup lock successfully removed.\n");

			// Step 5: Prepare secure cloning from the modified fstab to the temporary backup file
			fd_fstab = open("/etc/fstab", O_RDONLY);
			fd_tmp = open("/etc/.fstab.bak.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);

			if (fd_fstab == -1 || fd_tmp == -1) {
				printf("[!] Error: could not read fstab or create temporary copy environment. Did you run as root? (sudo)\n");
				if (fd_fstab != -1) close(fd_fstab);
				if (fd_tmp != -1) close(fd_tmp);
				return 1;
			}

			// Step 6: byte-by-byte cloning loop
			while ((r1 = read(fd_fstab, &byte_fstab, 1)) > 0) {
				write(fd_tmp, &byte_fstab, 1);
			}

			// Step 7: Force physical synchronization from RAM buffers to disk sectors/SSD cells
			fsync(fd_tmp);
			close(fd_fstab);
			close(fd_tmp);

			// Step 8: replacement. The old backup is replaced by the .tmp file
			if (rename("/etc/.fstab.bak.tmp", "/etc/fstab.bak") != 0) {
				printf("[-] Critical error while consolidating the new backup.\n");
				return 1;
			}
			printf("[+] New fstab successfully cloned into the backup.\n");


			// Step 9: reopen the new backup to reapply protection
			fd_bak = open("/etc/fstab.bak", O_RDONLY);
			if (fd_bak == -1) {
				printf("[-] Error: could not open the new backup file for protection.\n");
				return 1;
			}

			// Read flags belonging to this newly generated file
			ioctl(fd_bak, FS_IOC_GETFLAGS, &attr_flags);

		 	// the binary operation to enable the immutability bit
			attr_flags |= FS_IMMUTABLE_FL;


			// Command the kernel to apply the lock on the hard disk 
			if (ioctl(fd_bak, FS_IOC_SETFLAGS, &attr_flags) == 0) {
				printf("[+] Backup copy successfully created!\n");
			} else {
				printf("[-] Alert: could not apply immutability to the new backup.\n");

			}

			close(fd_bak);
			return 0;
		}

		// Option D: Invalid option
		else {
			printf("[-] Error: unrecognized option '%s'. Use -h or --help to open the help panel\n", argv[1]);
			return 1;
		}
	}
	

	// Normal mode (If argc == 1, when executed without flags during boot or manually
	fd_fstab = open("/etc/fstab", O_RDONLY);
	fd_bak = open("/etc/fstab.bak", O_RDONLY);

	// Smart error handling
	// If the backup does not exist (-1), it is a critical error, we cannot repair anything.
	if (fd_bak == -1) {
		printf("\n[!] Critical error: Backup file /etc/fstab.bak not found.\n");
		printf("[+] Please make sure you have run 'sudo fguard -install'.\n");
		if (fd_fstab != -1) close(fd_fstab);
		return 1;
	}

	// If the backup DOES exist but fstab DOES NOT (-1), it means it was deleted by error
	// Force direct repair bypassing the comparison loop
	if (fd_fstab == -1 && fd_bak != -1) {
		error_detectado = 1;
	}
	
	// If both files exist correctly, enter to the ordinary loop for byte-by-byte comparison
	if (fd_fstab != -1 && fd_bak != -1) {
	
		// Byte-by-byte comparison loop
		while (1) {
			
			r1 = read(fd_fstab, &byte_fstab, 1);
			r2 = read(fd_bak, &byte_bak, 1);

			if (r1 == 0 && r2 == 0) {
				break; // Both files are identical, the system is "healthy"
			}

			if (r1 != r2 || byte_fstab != byte_bak) {
				error_detectado = 1;
				break; // The original fstab was modified or corrupted
			}
		}
	}


	// Veredict and problem resolution
	if (error_detectado) {
		printf("\n[!] Alert: /etc/fstab has been modified or deleted without authorization.\n");
		printf("\n[+] Starting restoration protocol...\n");

		// Close file descriptors. They were opened in read-only mode, and we must reset them to work with them
		close(fd_fstab);
		close(fd_bak);

		// We open a temporary fstab file to prevent corruption in case of a power loss, using rename() later.
		fd_bak = open("/etc/fstab.bak", O_RDONLY); // Always open the backup in read-only mode to prevent compromising it
		fd_tmp = open("/etc/.fstab.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		

		if (fd_bak == -1 || fd_tmp == -1) {
			printf("Critical error: could not initialize repair environment. fd_bak=%d, fd_tmp=%d\n", fd_bak, fd_tmp); // We use fd_bak=%d and fd_tmp=%d for debugging. A value of -1 means kernel could not open the file. 
			if (fd_bak != -1) close(fd_bak); // If one opened and the other failed, close the open one
			if (fd_tmp != -1) close(fd_tmp);
			return 1;
		}


		// Transfer bytes to the temporary file
		// Note: we reuse the 'byte_fstab' variable as a temporary container to save memory		
        while ((r1 = read(fd_bak, &byte_fstab, 1)) > 0) {
			write(fd_tmp, &byte_fstab, 1);
		}
		
		// Physical synchronization with the disk (Guarantee against potential data corruption)
		fsync(fd_tmp);
		
		// Close streams before executing rename()
		close(fd_bak);
		close(fd_tmp);



		// Replacement. The temporary file becomes the new original file
		if (rename("/etc/.fstab.tmp", "/etc/fstab") == 0) {
			printf("[+] /etc/fstab restoration completed successfully.\n\n");
		} else {
			printf("[!] Critical operating system error while renaming the temporary staging file.\n"); // If we reach this scope, likely causes include disk space exhaustion or inode table saturation
			return 1;
		}

	} else {
		printf("\n[+] System integrity verified. No anomalies detected.\n\n");
		close(fd_fstab);
		close(fd_bak);
	}
	
	return 0; 
}
