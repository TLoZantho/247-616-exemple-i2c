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
/*******************************************************************************************/
/*******************************************************************************************/

int Lire_ID_Capteur(int);

int main()
{
	int fdPortI2C;  // file descriptor I2C
	
	fdPortI2C = open(I2C_BUS, O_RDWR); // ouverture du 'fichier', création d'un 'file descriptor' vers le port I2C
	if(fdPortI2C == -1)
	{
		perror("erreur: ouverture du port I2C ");
		return 1;
	}
	
	/// Liaison de l'adresse I2C au fichier (file descriptor) du bus I2C et Initialisation
	if(ioctl(fdPortI2C, I2C_SLAVE_FORCE, CAPTEUR_I2C_ADDRESS) < 0)
	{ 	// I2C_SLAVE_FORCE if it is already in use by a driver (i2cdetect : UU)
		perror("erreur: adresse du device I2C ");
		close(fdPortI2C);
		return 1;
	}

	printf("ID CAPTEUR = %#04X\n", Lire_ID_Capteur(fdPortI2C));

	close(fdPortI2C);
	return 0;
}

int Lire_ID_Capteur(int fdPortI2C)
{
	uint8_t Identification; 		// emplacement memoire pour stocker la donnee lue
	uint8_t Registre16bit[2];
	
	Registre16bit[0] = (uint8_t)(CAPTEUR_REGID >> 8);
	Registre16bit[1] = (uint8_t)CAPTEUR_REGID;
	
	if (write(fdPortI2C, Registre16bit, 2) != 2)
	{
		perror("erreur: I2C_ecrire ");
		return -1;
	}
	if (read(fdPortI2C, &Identification, 1) != 1)
	{
		perror("erreur: I2C_Lire ");
		return -1;
	}
	
	return Identification;
}
