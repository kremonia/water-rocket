/*
 Name:		water-rocket.ino
 Created:	6/9/2019 6:08:29 PM
 Author:	Alexander Cheorny
 Board:		Arduino Pro Micro (Compact Arduino Leonardo)

 Description:

*/
/*#include <Wire.h>*/
#include <Adafruit_BMP280.h>
#include <SPI.h>
#include <EEPROM.h>


/*------Altimeter-Settings--------*/
#define BMP_CS 10
/*Adafruit_BMP280 bmp; // I2C*/
Adafruit_BMP280 bmp(BMP_CS); // Hardware SPI
/*Adafruit_BMP280 bmp(BMP_CS, BMP_MOSI, BMP_MISO, BMP_SCK); //Software SPI*/

/*------EEPROM--------------------*/
#define CFG_BEGIN 0
#define DATA_BEGIN 8
#define MAX_EEPROM_WCOUNT 100000

/*------LED-----------------------*/
#define RXLED 17

/*------Button-pin----------------*/

#define BUTTN 3

enum FlightMode {
	Standby		= 0, /* Just do nothing. */
	Prelaunch	= 1, /* To initialize prelaunch sequence. */
	Ascending	= 2, /* Inflight mode to rapid sensors poll. */
	Descending	= 4  /* Inflight mode to stop sensors poll and to write telemetry to EEPROM */
}FMode;

enum AltitudeMode {
	SeaLevel = 0,
	GroundLevel = 1
};

enum ActivationMode {
	OnReset		= 0,
	OnButton	= 1,
};

//TODO: Figure out needed data to store between launches. Need to adjust DATA_BEGIN on changes.
//Configuration structure to hold config data.
struct ConfigStr {
	FlightMode StartMode		: 1;	/* Typically Prelaunch or Standby.*/
	AltitudeMode AltMode		: 1;	/* See or Ground level.*/
	ActivationMode Activation	: 1;	/* Trigger prelaunch activation automatically on reset or on button press*/
	uint8_t Alignment			: 5;
	uint8_t LaunchCount;				/* Launch count. Up to 255! Just for pleasure and complacency.*/
	uint16_t ServiceData;				/* Basically info related to EEPROM write count to keep it healthy.*/
	float ReferencePressure;
} Config;

void PrintConfig(ConfigStr* cfg);

float ReferencePressure = 1013.25F;
float AltitudeReadings[250];

float last_altituted;
int index = 0;

void setup() {
	
	EEPROM.get(CFG_BEGIN, Config);
	if (Config.AltMode == AltitudeMode::GroundLevel) ReferencePressure = Config.ReferencePressure;

	if (Config.Activation == ActivationMode::OnButton) {
		pinMode(BUTTN, INPUT_PULLUP);
		attachInterrupt(digitalPinToInterrupt(BUTTN), ButtonClick, FALLING);
	}

	EEPROM.get(DATA_BEGIN, AltitudeReadings);

	pinMode(RXLED, OUTPUT);
	digitalWrite(RXLED, LOW);
	FMode = FlightMode::Standby;
	Serial.begin(115200);
	//while (!Serial);
	
	
	Serial.println(F("System Test:"));

	if (!bmp.begin()) {
		Serial.println(F("BMP-280: ERROR!"));
		Serial.println(F("\a\tCould not find a BMP-280 sensor, check wiring!"));
		while (true);
	} else {
		Serial.println(F("BMP-280: OK"));
	}

	PrintConfig(&Config);
	/*int command = Serial.read();*/

	/*switch (command)
	{
	case 1: 
		break;
	}*/
	// Highest power consumption settings with hishest sampling rate and accuracy for altitude measurements
	for (int i = 0; i < 250; i++) {
		Serial.println(AltitudeReadings[i]);
	}

	bmp.begin();
	bmp.setSampling(
		Adafruit_BMP280::MODE_NORMAL,	// Operating Mode.
		Adafruit_BMP280::SAMPLING_X2,	// Temp. oversampling.
		Adafruit_BMP280::SAMPLING_X16,  // Pressure oversampling.
		Adafruit_BMP280::FILTER_X16,	// Filtering.
		Adafruit_BMP280::STANDBY_MS_1);	// Standby time.
	ReferencePressure = bmp.readPressure();
	last_altituted = bmp.readAltitude(ReferencePressure);
}

// The loop function runs over and over again until power down or reset
void loop() {

	if (FMode == FlightMode::Prelaunch)
	{
		if (bmp.readAltitude(ReferencePressure) - last_altituted > 0.45f)
		{
			last_altituted = bmp.readAltitude(ReferencePressure);
			FMode = FlightMode::Ascending;
			return;
		}
	}
	else if (FMode == FlightMode::Ascending)
	{
		AltitudeReadings[index] = bmp.readAltitude(ReferencePressure);

		if (last_altituted - AltitudeReadings[index] > 0.45f)
		{
			FMode = FlightMode::Descending;
		}

		last_altituted = AltitudeReadings[index];
		index++;
	}
	else if (FMode == FlightMode::Descending)
	{
		FMode = FlightMode::Standby;
		EEPROM.put(DATA_BEGIN, AltitudeReadings);
	}
}

void PrintConfig(ConfigStr* cfg)
{
	if (Serial) {
		Serial.print(F("Current Flight Mode:\t\t")); (cfg->StartMode == FlightMode::Standby) ? Serial.println(F("STANDBY")) : Serial.println(F("PRELAUNCH"));
		Serial.print(F("Current Altimeter Mode:\t\t")); (cfg->AltMode == AltitudeMode::SeaLevel) ? Serial.println(F("SEALEVEL")) : Serial.println(F("GROUNDLEVEL"));
		Serial.print(F("Total Launches Past:\t\t")); Serial.println(cfg->LaunchCount);
		Serial.print(F("Cuurent EEPROM Health:\t\t")); Serial.println(float((MAX_EEPROM_WCOUNT - cfg->ServiceData) / 1000));
	}
}

void ButtonClick() {
	FMode = FlightMode::Prelaunch;
	digitalWrite(RXLED, HIGH);
}
