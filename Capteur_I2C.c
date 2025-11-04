#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h> //for IOCTL defs
#include <fcntl.h>

//#define I2C_BUS "/dev/i2c-0" // fichier Linux representant le BUS #0
#define I2C_BUS "/dev/i2c-1" // fichier Linux representant le BUS #1

/*******************************************************************************************/
/*******************************************************************************************/
#define CAPTEUR_I2C_ADDRESS 0x29	// adresse I2C du capteur de distance -- A REMPLACER
#define CAPTEUR_REGID 0x0000	// adresse du registre ID du capteur de distance -- A REMPLACER
// Registres VL6180X
#define VL6180X_SYSTEM_FRESH_OUT_OF_RESET 0x0016
#define VL6180X_SYSRANGE_START 0x0018
#define VL6180X_RESULT_RANGE_STATUS 0x004D
#define VL6180X_RESULT_RANGE_VAL 0x0062
/*******************************************************************************************/
/*******************************************************************************************/
int i2c_file;

int WriteByte(uint16_t reg, uint8_t value)
{
    uint8_t buffer[3];
    buffer[0] = (reg >> 8) & 0xFF;  
    buffer[1] = reg & 0xFF;          
    buffer[2] = value;                 
    if (write(i2c_file, buffer, 3) != 3) 
    {
        printf("Erreur d'écriture au registre 0x%04X\n", reg);
        return -1;
    }
    return 0;
}

int ReadByte(uint16_t reg, uint8_t *value) 
{
    uint8_t reg_addr[2];
    reg_addr[0] = (reg >> 8) & 0xFF;
    reg_addr[1] = reg & 0xFF;
    
    if (write(i2c_file, reg_addr, 2) != 2) 
    {
        printf("Erreur d'écriture de l'adresse registre 0x%04X\n", reg);
        return -1;
    }
    if (read(i2c_file, value, 1) != 1) 
    {
        printf("Erreur de lecture du registre 0x%04X\n", reg);
        return -1;
    }
    return 0;
}

int configurer_capteur() {
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
    WriteByte(0x0133, 0x01);
    WriteByte(0x0096, 0x00);
    WriteByte(0x0097, 0xFD);
    WriteByte(0x00e3, 0x00);
    WriteByte(0x00e4, 0x04);
    WriteByte(0x00e5, 0x02);
    WriteByte(0x00e6, 0x01);
    WriteByte(0x00e7, 0x03);
    WriteByte(0x00f5, 0x02);
    WriteByte(0x00D9, 0x05);
    WriteByte(0x00DB, 0xCE);
    WriteByte(0x00DC, 0x03);
    WriteByte(0x00DD, 0xF8);
    WriteByte(0x009f, 0x00);
    WriteByte(0x00a3, 0x3c);
    WriteByte(0x00b7, 0x00);
    WriteByte(0x00bb, 0x3c);
    WriteByte(0x00b2, 0x09);
    WriteByte(0x00ca, 0x09);
    WriteByte(0x0198, 0x01);
    
    // Réinitialiser le flag "fresh out of reset"
    WriteByte(VL6180X_SYSTEM_FRESH_OUT_OF_RESET, 0x00);
    
    printf("Configuration terminée!\n");
    return 0;
}

int lire_distance(uint8_t *distance_mm) {
    uint8_t status;
    
    // Démarrer une mesure de distance (mode single-shot)
    WriteByte(VL6180X_SYSRANGE_START, 0x01);
    
    // Attendre que la mesure soit prête
    usleep(10000); // 10ms
    
    // Lire le statut
    if (ReadByte(VL6180X_RESULT_RANGE_STATUS, &status) != 0) {
        return -1;
    }
    
    // Vérifier si la mesure est valide (bit 0 du statut)
    if ((status & 0x01) == 0) {
        printf("Erreur: Mesure non prête\n");
        return -1;
    }
    
    // Lire la valeur de distance
    if (ReadByte(VL6180X_RESULT_RANGE_VAL, distance_mm) != 0) {
        return -1;
    }
    
    return 0;
}

int main() {
    uint8_t id_capteur;
    
    // Ouvrir le bus I2C
    i2c_file = open(I2C_BUS, O_RDWR);
    if (i2c_file < 0) {
        printf("Erreur: Impossible d'ouvrir le bus I2C %s\n", I2C_BUS);
        return 1;
    }
    
    // Configurer l'adresse I2C du capteur
    if (ioctl(i2c_file, I2C_SLAVE, CAPTEUR_I2C_ADDRESS) < 0) {
        printf("Erreur: Impossible de configurer l'adresse I2C\n");
        close(i2c_file);
        return 1;
    }
    
    printf("=== Programme de lecture VL6180X ===\n\n");
    
    // Lire l'ID du capteur
    if (ReadByte(CAPTEUR_REGID, &id_capteur) == 0) {
        printf("ID du capteur: 0x%02X\n\n", id_capteur);
    } else {
        printf("Erreur de lecture de l'ID\n");
        close(i2c_file);
        return 1;
    }
    
    // Configurer le capteur
    if (configurer_capteur() != 0) {
        close(i2c_file);
        return 1;
    }
    
    printf("\n=== Lecture de distances ===\n");
    printf("Appuyez sur Ctrl+C pour arrêter\n\n");
    
    // Boucle de lecture continue
    while (1) {
        uint8_t distance;
        
        if (lire_distance(&distance) == 0) {
            printf("Distance mesurée: %d mm\n", distance);
        } else {
            printf("Erreur de lecture\n");
        }
        
        sleep(1); // Attendre 1 seconde entre les mesures
    }
    
    close(i2c_file);
    return 0;
}