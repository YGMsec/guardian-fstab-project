#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

int main(int argc, char *argv[]) {
	
	// 1) Declaramos los file descriptors y búffers comunes
	int fd_fstab, fd_bak, fd_tmp;
	char byte_fstab, byte_bak;
	int r1, r2;
	int error_detectado = 0;


	// Control de argumentos de la terminal
	// Si el usuario introduce más de una opcion, avisamos del error
	if (argc > 2) {
		printf("\n[-] Error: Demasiados argumentos. Usa -h o --help para abrir el panel de ayuda.\n");
		return 1;
	}


	// Si introduce exactamente una opción adicional (argc == 2)
	if (argc == 2) {
		// Opción A: Panel de ayuda 
		if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
			printf("\n=== Panel de ayuda del guardián del fstab ===\n");
			printf("Uso: sudo fguard [Opción]\n\n");
			printf("\t-h, --help\t\tMuestra el panel de ayuda.\n");
			printf("\t-install\t\tCrea el backup inicial y configura el entorno.\n");
			printf("\t-update\t\t\tActualiza el fstab de forma legítima tras un cambio de disco.\n");
			printf("\t(sin opciones)\t\tModo normal de uso.\n\n");
			return 0;
		}
		// Opción B: Modo instalador. Crea el backup y le da el atributo de inmutabilidad (Que luego, en caso de actualizarlo, se le quitara la inmutabilidad, se procederá a los cambios y se le volverá a dar)
		else if (strcmp(argv[1], "-install") == 0) {
			printf("[+] Iniciando el modo instalador automático...\n");

			// Comprobamos si el backup ya existe con access()
			if (access("/etc/fstab.bak", F_OK) == 0) {
				printf("[!] Aviso: Ya existe una copia de seguridad en /etc/fstab.bak.\n");
				printf("[+] Si deseas actualizarla con cambios legítimos, usa la flag -update.\n");
				return 0;
			}

			printf("[+] No se detectó un backup previo. Creando copia inicial...\n");
			// Aqui programaremos el trasvase del original al .bak con ioctl() en el siguiente paso, creando primero un archivo temporal por si hay un corte de luz o se apaga el ordenador de manera inesperada y no quede un archivo corrupto
			// *commit, bloque añadido
			// Abrimos el original y creamos el temporal para el backup
			fd_fstab = open("/etc/fstab", O_RDONLY);
			fd_tmp = open("/etc/.fstab.bak.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
				
			if (fd_fstab == -1 || fd_tmp == -1) {
				printf("[-] Error crítico: No se pudo crear el entorno de instalación.\n");
				if (fd_fstab != -1) close(fd_fstab);
				if (fd_tmp != -1) close(fd_tmp);
				return 1;
			}

			// Trasvase de bytes al archivo temporal del backup
			while ((r1 = read(fd_fstab, &byte_fstab, 1)) > 0) {
				write(fd_tmp, &byte_fstab, 1);
			}

			// Sincronización física y cierre de flujos
			fsync(fd_tmp);
			close(fd_fstab);
			close(fd_tmp);

			// Reemplazo de la copia temporal del backup al backup
			if (rename("/etc/.fstab.bak.tmp", "/etc/fstab.bak") != 0) {
				printf("[!] Error crítico del sistema operativo al generar el archivo de backup.\n");
				return 1;
			}
			
			// Bajo nivel: aplicar el atributo inmutable (+i) nativo
			int attr_flags;

			// Abrimos el nuevo backup en modo lectura para leer y modificar sus metadatos del inodo
			fd_bak = open("/etc/fstab.bak", O_RDONLY);
			if (fd_bak ==  -1) {
				printf("[-] Error al abrir el archivo de backup para aplicar la inmutabilidad\n");
				return 1;
			}

			// Extraemos las flags actuales del inodo desde el kernel
			ioctl(fd_bak, FS_IOC_GETFLAGS, &attr_flags);
			
			// Encendemos el bit especifico de inmutabilidad usando el operador lógico OR
			attr_flags |= FS_IMMUTABLE_FL;

			// Le ordenamos al kernel que guarde las nuevas flags modificadas en el disco
			if (ioctl(fd_bak, FS_IOC_SETFLAGS, &attr_flags) == 0) {
				printf("[+] Backup configurado con éxito con su flag de inmutabilidad.\n");
			} else {
				printf("[-] Alerta: No se pudo aplicar el atributo de inmutabilidad en el backup.\n");
			}

			close(fd_bak);
			return 0;
		}

		// Opción C: Modo actualización
		else if (strcmp(argv[1], "-update") == 0) {
			printf("[+] Iniciando la actualización segura del fstab...\n");
			//Aquí programaremos el desbloqueo inmutable y la posible corrupción de datos con rename
			// commit o bloque de código añadido
			int attr_flags;

			// Paso 1: Abrir el backup actual(en modo lectura basta para leer/escribir flags en el inodo)
			fd_bak = open("/etc/fstab.bak", O_RDONLY);
			if (fd_bak == -1) {
				printf("[-] Error: no se pudo abrir /etc/fstab.bak. Ejecutaste primero el modo -install? (-h o --help para abrir el panel de ayuda)\n");
				return 1;
			}

			// Paso 2: traer del kernel los bits del inodo a nuestra memoria RAM
			if (ioctl(fd_bak, FS_IOC_GETFLAGS, &attr_flags) < 0) {
				printf("[-] Error crítico: no se pudieron leer los atributos del backup.\n");
				close(fd_bak);
				return 1;
			}

			// Paso 3: Operación matemática binaria para apagar el bit de inmutabilidad
			attr_flags &= ~FS_IMMUTABLE_FL; // ~ operador NOT
			

			// Paso 4: Enviar de vuelta al kernel las flags modificadas para desbloquear el archivo
			if (ioctl(fd_bak, FS_IOC_SETFLAGS, &attr_flags) < 0) {
				printf("[-] Error crítico: El kernel denegó quitar la inmutabilidad del backup.\n");
				close(fd_bak);
				return 1;
			}

			// Cerramos el file descriptor. Al cerrarlo, el sistema de archivos asienta el cambio en el disco
			close(fd_bak);
			printf("[+] Candado del backup abierto con éxito.\n");

			//Paso 5: Preparar el trasvase seguro del fstab modificado al archivo temporal del backup
			fd_fstab = open("/etc/fstab", O_RDONLY);
			fd_tmp = open("/etc/.fstab.bak.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);

			if (fd_fstab == -1 || fd_tmp == -1) {
				printf("[!] Error: no se ha podido leer el fstab o crear el entorno temporal de copia. Lo has ejecutado con privilegios? (sudo)\n");
				if (fd_fstab != -1) close(fd_fstab);
				if (fd_tmp != -1) close(fd_tmp);
				return 1;
			}

			// Paso 6: bucle de clonación byte a byte
			while ((r1 = read(fd_fstab, &byte_fstab, 1)) > 0) {
				write(fd_tmp, &byte_fstab, 1);
			}

			// Paso 7: Forzar la sincronización física del búffer de la RAM a los platos del disco o celdas del SSD
			fsync(fd_tmp);
			close(fd_fstab);
			close(fd_tmp);

			// Paso 8: Reemplazo. El viejo backup desaparece y el .tmp toma su lugar
			if (rename("/etc/.fstab.bak.tmp", "/etc/fstab.bak") != 0) {
				printf("[-] Error crítico del sistema operativo al consolidar el nuevo backup.\n");
				return 1;
			}
			printf("[+] Nuevo fstab clonado en la zona del backup.\n");


			// Paso 9: Reabrir el nuevo backup para ponerle la inmutabilidad otra vez
			fd_bak = open("/etc/fstab.bak", O_RDONLY);
			if (fd_bak == -1) {
				printf("[-] Error: No se ha podido abrir el nuevo backup para protegerlo.\n");
				return 1;
			}

			// Leemos las flags que tiene este archivo recién generado
			ioctl(fd_bak, FS_IOC_GETFLAGS, &attr_flags);

		 	// La operación binaria para encender el bit de la inmutabilidad
			attr_flags |= FS_IMMUTABLE_FL;


			// Ordenamos al kernel aplicar el bloqueo en el disco duro
			if (ioctl(fd_bak, FS_IOC_SETFLAGS, &attr_flags) == 0) {
				printf("[+] Copia de seguridad actualizada con éxito!\n");
			} else {
				printf("[-] Alerta de seguridad: No se ha podido aplicar la inmutabilidad al nuevo backup.\n");

			}

			close(fd_bak);
			return 0;
		}

		// Opción D: Opción incorrecta
		else {
			printf("[-] Error: Opción '%s' no reconocida. Usa -h o --help para abrir el panel de ayuda.\n", argv[1]);
			return 1;
		}
	}
	

	// Modo normal (Si argc == 1, cuando no pasas flags en el apagado o al usarlo)
	fd_fstab = open("/etc/fstab", O_RDONLY);
	fd_bak = open("/etc/fstab.bak", O_RDONLY);

	// Control de errores inteligente
	// Si el backup no existe (-1), es un error crítico, no podemos reparar nada.
	if (fd_bak == -1) {
		printf("\n[!] Error crítico: No se encuentra la copia de seguridad /etc/fstab.bak .\n");
		printf("[+] Asegúrate de haber ejecutado antes 'sudo fguard -install'.\n");
		if (fd_fstab != -1) close(fd_fstab);
		return 1;
	}

	// Si el backup SÍ existe pero el fstab NO existe (-1), significa que ha sido borrado por error
	// Forzamos la reparación directa sin pasar por el bucle de comparación
	if (fd_fstab == -1 && fd_bak != -1) {
		error_detectado = 1;
	}
	
	// Si ambos existen correctamente, entramos al bucle ordinario para comparar byte a byte
	if (fd_fstab != -1 && fd_bak != -1) {
	
		// Bucle de comparación byte a byte
		while (1) {
			
			r1 = read(fd_fstab, &byte_fstab, 1);
			r2 = read(fd_bak, &byte_bak, 1);

			if (r1 == 0 && r2 == 0) {
				break; // idénticos ambos archivos, el sistema está "sano"
			}

			if (r1 != r2 || byte_fstab != byte_bak) {
				error_detectado = 1;
				break; // modificado o dañado el fstab original
			}
		}
	}


	// Veredicto y resolución del problema
	if (error_detectado) {
		printf("\n[!] Alerta: El fstab ha sido modificado o borrado de forma no autorizada.\n");
		printf("\n[+] Iniciando el protocolo de restauración...\n");

		// Cerramos los file descriptos. Los habiamos abierto en modo lectura y ahora debemos resetearlos para poder trabajar con ellos
		close(fd_fstab);
		close(fd_bak);

		// iniciaremos la apertura del fstab temporal por si hubiera algún apagón o problema, para que no se quede corrupto, y utilizaremos el rename
		fd_bak = open("/etc/fstab.bak", O_RDONLY); // El backup siempre lo abrimos en modo lectura, evitamos comprometerlo
		fd_tmp = open("/etc/.fstab.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644); // Sabemos que el temporal todavía no existe, por esto estas opciones
		

		if (fd_bak == -1 || fd_tmp == -1) {
			printf("Error crítico: no se pudo iniciar el entorno de reparación. fd_bak=%d, fd_tmp=%d\n", fd_bak, fd_tmp); // Aquí ponemos el fd_bak=%d y fd_tmp=%d a modo de traza, nos dirá donde ha estado el problema con los file descriptos que el kernel nos asigne. El -1 significa que ese archivo no ha podido ser abierto
			if (fd_bak != -1) close(fd_bak); // si uno se ha abierto y el otro no, cierra el que esté abierto
			if (fd_tmp != -1) close(fd_tmp);
			return 1;
		}


		// Trasvase de bytes al archivo temporal
		// Nota: Reutilizamos la variable 'byte_fstab' como contenedor temporal para ahorrar memoria
		while ((r1 = read(fd_bak, &byte_fstab, 1)) > 0) {
			write(fd_tmp, &byte_fstab, 1);
		}
		
		// Sincronización física con el disco (Garantía contra posible corrupción de datos del archivo)
		fsync(fd_tmp);
		
		// Cerramos los canales antes del rename()
		close(fd_bak);
		close(fd_tmp);


		// Reemplazo del archivo original por el temporal siendo ahora el original
		if (rename("/etc/.fstab.tmp", "/etc/fstab") == 0) {
			printf("[+] Restauración del archivo fstab completada con éxito.\n\n");
		} else {
			printf("[!] Error crítico del sistema operativo al renombrar el archivo temporal.\n"); // si llegamos aquí, una posible causa es falta de espacio o carencia de inodes en la inode table
			return 1;
		}

	} else {
		printf("\n[+] Sistema íntegro.\n\n");
		close(fd_fstab);
		close(fd_bak);
	}
	
	return 0; 
}
