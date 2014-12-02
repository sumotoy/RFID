/*
 * RFID.cpp - Library to use ARDUINO RFID MODULE KIT 13.56 MHZ WITH TAGS SPI W AND R BY COOQROBOT.
 * Based on code Dr.Leong   ( WWW.B2CQSHOP.COM )
 * Created by Miguel Balboa, Jan, 2012. 
 * Modified by Paul Kourany to run on Spark Core with added support for Software SPI, Mar, 2014.
 * Released into the public domain.
 */

/******************************************************************************
 * Includes
 ******************************************************************************/
#include "RFID.h"
#include <SPI.h>

#ifdef SPI_HAS_TRANSACTION
static SPISettings mySettings;
#endif
/******************************************************************************
 * User API
 ******************************************************************************/

/**
 * Construct RFID
 * uint8_t chipSelectPin RFID /ENABLE pin
 */
RFID::RFID(uint8_t chipSelectPin, uint8_t NRSTPD)
{
	_chipSelectPin = chipSelectPin;
  _NRSTPD = NRSTPD;
}



/******************************************************************************
 * User API
 ******************************************************************************/

 bool RFID::isCard() 
 {
	uint8_t status;
	uint8_t str[MAX_LEN];
	status = MFRC522Request(PICC_REQIDL, str);	
    if (status == MI_OK) return true;
	return false; 
 }

 bool RFID::readCardSerial(){
	uint8_t status;
	uint8_t str[MAX_LEN];
	status = anticoll(str);
	memcpy(serNum, str, 5);
	if (status == MI_OK) return true;
	return false;
 }

/******************************************************************************
 * Dr.Leong   ( WWW.B2CQSHOP.COM )
 ******************************************************************************/

void RFID::init()
{
	SPI.begin();
	pinMode(_chipSelectPin, OUTPUT);
	digitalWrite(_chipSelectPin, HIGH);
	pinMode(_NRSTPD,OUTPUT);					// Set digital pin, Not Reset and Power-down
	digitalWrite(_NRSTPD, HIGH);
#if defined(SPI_HAS_TRANSACTION)
	mySettings = SPISettings(1000000, MSBFIRST, SPI_MODE0);
#else//do not use SPItransactons
	SPI.setClockDivider(SPI_CLOCK_DIV8);
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE0);
#endif
	if (digitalRead(_NRSTPD) == LOW) {	//The MFRC522 chip is in power down mode.
		digitalWrite(_NRSTPD, HIGH);		// Exit power down mode. This triggers a hard reset.
		// Section 8.8.2 in the datasheet says the oscillator start-up time is the start up time of the crystal + 37,74?s. Let us be generous: 50ms.
		delay(50);
	} else { // Perform a soft reset
		reset();
	}
	
	//Timer: TPrescaler*TreloadVal/6.78MHz = 24ms
    writeMFRC522(TModeReg, 0x80);//	0x8D	//Tauto=1; f(Timer) = 6.78MHz/TPreScaler
    writeMFRC522(TPrescalerReg, 0xA9);//0x3E	//TModeReg[3..0] + TPrescalerReg
    writeMFRC522(TReloadRegL, 0x03);//30           
    writeMFRC522(TReloadRegH, 0xE8);//0
	writeMFRC522(TxAutoReg, 0x40);		//100%ASK
	writeMFRC522(ModeReg, 0x3D);		// CRC valor inicial de 0x6363
	antennaOn();		//Abre  la antena
}
void RFID::reset()
{
	writeMFRC522(CommandReg, PCD_RESETPHASE);
}


void RFID::writeMFRC522(uint8_t addr, uint8_t val)
{
	startSend();
    SPI.transfer((addr<<1)&0x7E);	
    SPI.transfer(val);
	endSend();
}


/*
 *  Read_MFRC522 Nombre de la funciÃƒÂ³n: Read_MFRC522
 *  DescripciÃƒÂ³n: Desde el MFRC522 leer un byte de un registro de datos
 *  Los parÃƒÂ¡metros de entrada: addr - la direcciÃƒÂ³n de registro
 *  Valor de retorno: Devuelve un byte de datos de lectura
 */
uint8_t RFID::readMFRC522(uint8_t addr)
{
    uint8_t val;
	startSend();
    SPI.transfer(((addr<<1)&0x7E) | 0x80);	
    val = SPI.transfer(0x00);
	endSend();
    return val;	
}


void RFID::antennaOn(void)
{
	uint8_t value = readMFRC522(TxControlReg);
	if ((value & 0x03) != 0x03) writeMFRC522(TxControlReg, value | 0x03);
}


void RFID::setBitMask(uint8_t reg, uint8_t mask)  
{ 
    uint8_t tmp;
    tmp = readMFRC522(reg);
    writeMFRC522(reg, tmp | mask);  // set bit mask
}

void RFID::clearBitMask(uint8_t reg, uint8_t mask)  
{
    uint8_t tmp;
    tmp = readMFRC522(reg);
    writeMFRC522(reg, tmp & (~mask));  // clear bit mask
} 

void RFID::calculateCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData)
{
    uint8_t i, n;
    clearBitMask(DivIrqReg, 0x04);			//CRCIrq = 0
    setBitMask(FIFOLevelReg, 0x80);			//Claro puntero FIFO
    //Write_MFRC522(CommandReg, PCD_IDLE);
	//Escribir datos en el FIFO	
    for (i=0; i<len; i++) writeMFRC522(FIFODataReg, *(pIndata+i));   
    writeMFRC522(CommandReg, PCD_CALCCRC);

	// Esperar a la finalizaciÃƒÂ³n de cÃƒÂ¡lculo del CRC
    i = 0xFF;
    do {
        n = readMFRC522(DivIrqReg);
        i--;
    }
    while ((i!=0) && !(n&0x04));			//CRCIrq = 1
	//Lea el cÃƒÂ¡lculo de CRC
    pOutData[0] = readMFRC522(CRCResultRegL);
    pOutData[1] = readMFRC522(CRCResultRegM);
}

uint8_t RFID::MFRC522ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen)
{
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
	uint8_t lastBits;
    uint8_t n;
    uint16_t i;

    switch (command){
        case PCD_AUTHENT:		// Tarjetas de certificaciÃƒÂ³n cerca
		{
			irqEn = 0x12;
			waitIRq = 0x10;
			break;
		}
		case PCD_TRANSCEIVE:	//La transmisiÃƒÂ³n de datos FIFO
		{
			irqEn = 0x77;
			waitIRq = 0x30;
			break;
		}
		default:
			break;
    }

    writeMFRC522(CommIEnReg, irqEn|0x80);	//De solicitud de interrupciÃƒÂ³n
    clearBitMask(CommIrqReg, 0x80);			// Borrar todos los bits de peticiÃƒÂ³n de interrupciÃƒÂ³n
    setBitMask(FIFOLevelReg, 0x80);			//FlushBuffer=1, FIFO de inicializaciÃƒÂ³n
	writeMFRC522(CommandReg, PCD_IDLE);	//NO action;Y cancelar el comando
	//Escribir datos en el FIFO
    for (i=0; i<sendLen; i++) writeMFRC522(FIFODataReg, sendData[i]);    
	//???? ejecutar el comando
	writeMFRC522(CommandReg, command);
    if (command == PCD_TRANSCEIVE) setBitMask(BitFramingReg, 0x80);		//StartSend=1,transmission of data starts  
	// A la espera de recibir datos para completar
	i = 2000;	//i????????,??M1???????25ms	??? i De acuerdo con el ajuste de frecuencia de reloj, el tiempo mÃƒÂ¡ximo de espera operaciÃƒÂ³n M1 25ms tarjeta??
    do {
		//CommIrqReg[7..0]
		//Set1 TxIRq RxIRq IdleIRq HiAlerIRq LoAlertIRq ErrIRq TimerIRq
        n = readMFRC522(CommIrqReg);
        i--;
    }
    while ((i!=0) && !(n&0x01) && !(n&waitIRq));
    clearBitMask(BitFramingReg, 0x80);			//StartSend=0
    if (i != 0){    
        if(!(readMFRC522(ErrorReg) & 0x1B))	{//BufferOvfl Collerr CRCErr ProtecolErr
            status = MI_OK;
            if (n & irqEn & 0x01) status = MI_NOTAGERR;			//??   
            if (command == PCD_TRANSCEIVE){
               	n = readMFRC522(FIFOLevelReg);
              	lastBits = readMFRC522(ControlReg) & 0x07;
                if (lastBits){   
					*backLen = (n-1)*8 + lastBits;   
				} else {   
					*backLen = n*8;   
				}
                if (n == 0)  n = 1;    
                if (n > MAX_LEN)  n = MAX_LEN;   
				//??FIFO??????? Lea los datos recibidos en el FIFO
                for (i=0; i<n; i++) backData[i] = readMFRC522(FIFODataReg);    
            }
        } else {   
			status = MI_ERR;  
		}
    }
    //SetBitMask(ControlReg,0x80);           //timer stops
    //Write_MFRC522(CommandReg, PCD_IDLE); 
    return status;
}


/*
 *  Nombre de la funciÃƒÂ³n: MFRC522_Request
 *  DescripciÃƒÂ³n: Buscar las cartas, leer el nÃƒÂºmero de tipo de tarjeta
 *  Los parÃƒÂ¡metros de entrada: reqMode - encontrar el modo de tarjeta,
 *			   Tagtype - Devuelve el tipo de tarjeta
 *			 	0x4400 = Mifare_UltraLight
 *				0x0400 = Mifare_One(S50)
 *				0x0200 = Mifare_One(S70)
 *				0x0800 = Mifare_Pro(X)
 *				0x4403 = Mifare_DESFire
 *  Valor de retorno: el retorno exitoso MI_OK
 */
uint8_t  RFID::MFRC522Request(uint8_t reqMode, uint8_t *TagType)
{
	uint8_t status;  
	uint16_t backBits;			//   RecibiÃƒÂ³ bits de datos
	writeMFRC522(BitFramingReg, 0x07);		//TxLastBists = BitFramingReg[2..0]	???
	TagType[0] = reqMode;
	status = MFRC522ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);
	if ((status != MI_OK) || (backBits != 0x10)) status = MI_ERR;
	return status;
}

/**
 *  MFRC522Anticoll -> anticoll
 *  Anti-detecciÃƒÂ³n de colisiones, la lectura del nÃƒÂºmero de serie de la tarjeta de tarjeta
 *  @param serNum - devuelve el nÃƒÂºmero de tarjeta 4 bytes de serie, los primeros 5 bytes de bytes de paridad
 *  @return retorno exitoso MI_OK
 */
uint8_t RFID::anticoll(uint8_t *serNum){
    uint8_t status;
    uint8_t i;
	uint8_t serNumCheck=0;
    uint16_t unLen;
    //ClearBitMask(Status2Reg, 0x08);		//TempSensclear
    //ClearBitMask(CollReg,0x80);			//ValuesAfterColl
	writeMFRC522(BitFramingReg, 0x00);		//TxLastBists = BitFramingReg[2..0]
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = MFRC522ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);
    if (status == MI_OK){
		//?????? Compruebe el nÃƒÂºmero de serie de la tarjeta
		for (i=0; i<4; i++) serNumCheck ^= serNum[i];
		if (serNumCheck != serNum[i])  status = MI_ERR;    
    }
    //SetBitMask(CollReg, 0x80);		//ValuesAfterColl=1
    return status;
}

/* 
 * MFRC522Auth -> auth
 * Verificar la contraseÃƒÂ±a de la tarjeta
 * Los parÃƒÂ¡metros de entrada: AuthMode - Modo de autenticaciÃƒÂ³n de contraseÃƒÂ±a
                 0x60 = A 0x60 = validaciÃƒÂ³n KeyA
                 0x61 = B 0x61 = validaciÃƒÂ³n KeyB
             BlockAddr--  bloque de direcciones
             Sectorkey-- sector contraseÃƒÂ±a
             serNum--,4? Tarjeta de nÃƒÂºmero de serie, 4 bytes
 * MI_OK Valor de retorno: el retorno exitoso MI_OK
 */
uint8_t RFID::auth(uint8_t authMode, uint8_t BlockAddr, uint8_t *Sectorkey, uint8_t *serNum)
{
    uint8_t status;
    uint16_t recvBits;
    uint8_t i;
	uint8_t buff[12]; 
	//????+???+????+???? Verifique la direcciÃƒÂ³n de comandos de bloques del sector + + contraseÃƒÂ±a + nÃƒÂºmero de la tarjeta de serie
    buff[0] = authMode;
    buff[1] = BlockAddr;
    for (i=0; i<6; i++) buff[i+2] = *(Sectorkey+i);   
    for (i=0; i<4; i++) buff[i+8] = *(serNum+i);   
    status = MFRC522ToCard(PCD_AUTHENT, buff, 12, buff, &recvBits);
    if ((status != MI_OK) || (!(readMFRC522(Status2Reg) & 0x08))) status = MI_ERR;   
    return status;
}

/*
 * MFRC522Read -> read
 * Lectura de datos de bloque
 * Los parÃƒÂ¡metros de entrada: blockAddr - direcciÃƒÂ³n del bloque; recvData - leer un bloque de datos
 * MI_OK Valor de retorno: el retorno exitoso MI_OK
 */
uint8_t RFID::read(uint8_t blockAddr, uint8_t *recvData)
{
    uint8_t status;
    uint16_t unLen;
    recvData[0] = PICC_READ;
    recvData[1] = blockAddr;
    calculateCRC(recvData,2, &recvData[2]);
    status = MFRC522ToCard(PCD_TRANSCEIVE, recvData, 4, recvData, &unLen);

    if ((status != MI_OK) || (unLen != 0x90)) status = MI_ERR;
    return status;
}

/*
 * MFRC522Write -> write
 * La escritura de datos de bloque
 * blockAddr - direcciÃƒÂ³n del bloque; WriteData - para escribir 16 bytes del bloque de datos
 * Valor de retorno: el retorno exitoso MI_OK
 */
uint8_t RFID::write(uint8_t blockAddr, uint8_t *writeData)
{
    uint8_t status;
    uint16_t recvBits;
    uint8_t i;
	uint8_t buff[18]; 
    buff[0] = PICC_WRITE;
    buff[1] = blockAddr;
    calculateCRC(buff, 2, &buff[2]);
    status = MFRC522ToCard(PCD_TRANSCEIVE, buff, 4, buff, &recvBits);
    if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A)) status = MI_ERR;   
    if (status == MI_OK){
        for (i=0; i<16; i++) buff[i] = *(writeData+i);   
        calculateCRC(buff, 16, &buff[16]);
        status = MFRC522ToCard(PCD_TRANSCEIVE, buff, 18, buff, &recvBits);
		if ((status != MI_OK) || (recvBits != 4) || ((buff[0] & 0x0F) != 0x0A)) status = MI_ERR;   
    }
    return status;
}


/*
 * MFRC522Halt -> halt
 * Cartas de Mando para dormir
 * Los parÃƒÂ¡metros de entrada: Ninguno
 * Valor devuelto: Ninguno
 */
void RFID::halt()
{
    //uint8_t status;
    uint16_t unLen;
    uint8_t buff[4];
    buff[0] = PICC_HALT;
    buff[1] = 0;
    calculateCRC(buff, 2, &buff[2]);
    clearBitMask(Status2Reg, 0x08); // turn off encryption
    MFRC522ToCard(PCD_TRANSCEIVE, buff, 4, buff,&unLen);
}


void RFID::startSend() {
#if defined(SPI_HAS_TRANSACTION)
	SPI.beginTransaction(mySettings);  // gain control of SPI bus
	digitalWrite(_chipSelectPin, LOW);		// Select slave
#else
	digitalWrite(_chipSelectPin, LOW);		// Select slave
#endif
}

void RFID::endSend() {
#if defined(SPI_HAS_TRANSACTION)
	digitalWrite(_chipSelectPin, HIGH);		// Select slave
	SPI.endTransaction();              // release the SPI bus
#else
	digitalWrite(_chipSelectPin, HIGH);		// Select slave
#endif
}

