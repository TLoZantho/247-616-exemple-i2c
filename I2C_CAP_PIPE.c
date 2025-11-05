#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

//#define I2C_BUS "/dev/i2c-0"
#define I2C_BUS "/dev/i2c-1"

#define CAPTEUR_I2C_ADDRESS 0x29
#define VL6180X_SYSTEM_FRESH_OUT_OF_RESET 0x0016
#define VL6180X_SYSRANGE_START 0x0018
#define VL6180X_RESULT_RANGE_VAL 0x0062
#define VL6180X_RESULT_RANGE_STATUS 0x004D

int i2c_file;

int WriteByte(uint16_t reg, uint8_t value) {
    uint8_t buffer[3];
    buffer[0] = (reg >> 8) & 0xFF;
    buffer[1] = reg & 0xFF;
    buffer[2] = value;
    if (write(i2c_file, buffer, 3) != 3) {
        perror("Erreur écriture I2C");
        return -1;
    }
    return 0;
}

int ReadByte(uint16_t reg, uint8_t *value) {
    uint8_t reg_addr[2];
    reg_addr[0] = (reg >> 8) & 0xFF;
    reg_addr[1] = reg & 0xFF;
    if (write(i2c_file, reg_addr, 2) != 2) return -1;
    if (read(i2c_file, value, 1) != 1) return -1;
    return 0;
}

int lire_distance(uint8_t *distance_mm) {
    WriteByte(VL6180X_SYSRANGE_START, 0x01);
    usleep(10000); // Attente courte
    if (ReadByte(VL6180X_RESULT_RANGE_VAL, distance_mm) != 0) return -1;
    return 0;
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int configurer_capteur() 
{
    printf("Configuration du capteur VL6180X...\n");
    
    // Vérifier si le capteur sort du reset
    uint8_t fresh_reset;
    if (ReadByte(VL6180X_SYSTEM_FRESH_OUT_OF_RESET, &fresh_reset) != 0) {
        printf("Erreur: Impossible de lire le statut du capteur\n");
        return -1;
    }
    
    if (fresh_reset != 0x01) {
        printf("Avertissement: Le capteur n'est pas en état de reset initial\n");
    }
    
    // Tuning Settings fournis
WriteByte(0x0207, 0x01);
WriteByte(0x0208, 0x01);
WriteByte(0x0096, 0x00);
WriteByte(0x0097, 0xfd);
WriteByte(0x00e3, 0x00);
WriteByte(0x00e4, 0x04);
WriteByte(0x00e5, 0x02);
WriteByte(0x00e6, 0x01);
WriteByte(0x00e7, 0x03);
WriteByte(0x00f5, 0x02);
WriteByte(0x00d9, 0x05);
WriteByte(0x00db, 0xce);
WriteByte(0x00dc, 0x03);
WriteByte(0x00dd, 0xf8);
WriteByte(0x009f, 0x00);
WriteByte(0x00a3, 0x3c);
WriteByte(0x00b7, 0x00);
WriteByte(0x00bb, 0x3c);
WriteByte(0x00b2, 0x09);
WriteByte(0x00ca, 0x09);
WriteByte(0x0198, 0x01);
WriteByte(0x01b0, 0x17);
WriteByte(0x01ad, 0x00);
WriteByte(0x00ff, 0x05);
WriteByte(0x0100, 0x05);
WriteByte(0x0199, 0x05);
WriteByte(0x01a6, 0x1b);
WriteByte(0x01ac, 0x3e);
WriteByte(0x01a7, 0x1f);
WriteByte(0x0030, 0x00);
WriteByte(0x0011, 0x10); // Enables polling for "New Sample ready" when measurement completes
WriteByte(0x010a, 0x30); // Set averaging sample period
WriteByte(0x003f, 0x46); // Sets light and dark gain
WriteByte(0x0031, 0xFF); // Auto calibration count
WriteByte(0x0040, 0x63); // ALS integration time 100ms
WriteByte(0x002e, 0x01); // Temperature calibration
WriteByte(0x001b, 0x09); // Default ranging inter-measurement period 100ms
WriteByte(0x003e, 0x31); // Default ALS inter-measurement period 500ms
WriteByte(0x0014, 0x24); // Interrupt on new sample ready
WriteByte(0x0016, 0x00); // Clear fresh-out-of-set status
    
    // Réinitialiser le flag "fresh out of reset"
    WriteByte(VL6180X_SYSTEM_FRESH_OUT_OF_RESET, 0x00);
    
    printf("Configuration terminée!\n");
    return 0;
}
int main() {
    // Création des pipes :
    int pipe_pere_fils[2];   // Père -> Fils : demande de démarrer/arrêter affichage
    int pipe_fils_petit[2];  // Fils -> Petit-fils : ordre de mesurer/arrêter
    int pipe_mesure[2];      // Petit-fils -> Fils : retour des mesures

    pipe(pipe_pere_fils);
    pipe(pipe_fils_petit);
    pipe(pipe_mesure);

    set_nonblocking(pipe_pere_fils[0]);
    set_nonblocking(pipe_fils_petit[0]);
    set_nonblocking(pipe_mesure[0]);
    if (configurer_capteur() != 0) {
        close(i2c_file);
        return 1;
    }
    pid_t pid_fils = fork();

    if (pid_fils == 0) {
        // ===== Processus FILS =====
        pid_t pid_petit = fork();

        if (pid_petit == 0) {
            // ===== Processus PETIT-FILS =====
            close(pipe_pere_fils[0]);
            close(pipe_pere_fils[1]);
            close(pipe_fils_petit[1]);
            close(pipe_mesure[0]);

            // Initialisation du capteur
            i2c_file = open(I2C_BUS, O_RDWR);
            ioctl(i2c_file, I2C_SLAVE, CAPTEUR_I2C_ADDRESS);

            printf("[Petit-fils] Démarré (capteur prêt)\n");

            char commande;
            uint8_t distance;
            while (1) {
                // Lire commande du fils (non bloquant)
                if (read(pipe_fils_petit[0], &commande, 1) > 0) {
                    if (commande == 'Q') break;
                }

                // Faire une mesure et envoyer
                if (lire_distance(&distance) == 0) {
                    write(pipe_mesure[1], &distance, 1);
                }

                usleep(500000); // 0.5s entre mesures
            }

            printf("[Petit-fils] Arrêté\n");
            close(i2c_file);
            exit(0);
        }

        // ===== Retour dans le FILS =====
        close(pipe_pere_fils[1]);
        close(pipe_fils_petit[0]);
        close(pipe_mesure[1]);

        printf("[Fils] En attente d'ordres du père...\n");

        char commande;
        uint8_t distance;
        int afficher = 0;

        while (1) {
            // Lire ordre du père
            if (read(pipe_pere_fils[0], &commande, 1) > 0) {
                if (commande == 'M') {
                    afficher = 1;
                    write(pipe_fils_petit[1], "M", 1);
                } else if (commande == 'S') {
                    afficher = 0;
                    write(pipe_fils_petit[1], "S", 1);
                } else if (commande == 'Q') {
                    write(pipe_fils_petit[1], "Q", 1);
                    break;
                }
            }

            // Lire mesure du petit-fils
            if (afficher && read(pipe_mesure[0], &distance, 1) > 0) {
                printf("[Fils] Distance : %d mm\n", distance);
            }

            usleep(100000);
        }

        printf("[Fils] Fin\n");
        exit(0);
    }

    // ===== Processus PÈRE =====
    close(pipe_pere_fils[0]);
    close(pipe_fils_petit[0]);
    close(pipe_fils_petit[1]);
    close(pipe_mesure[0]);
    close(pipe_mesure[1]);

    printf("[Père] Commandes :\n");
    printf("  M = démarrer mesures\n");
    printf("  S = suspendre mesures\n");
    printf("  Q = quitter\n");

    char c;
    while (1) {
        c = getchar();
        c = toupper(c);

        if (c == 'M') {
            write(pipe_pere_fils[1], "M", 1);
        } else if (c == 'S') {
            write(pipe_pere_fils[1], "S", 1);
        } else if (c == 'Q') {
            write(pipe_pere_fils[1], "Q", 1);
            break;
        }
    }

    printf("[Père] Programme terminé.\n");
    return 0;
}
