

// List of available simulator types
enum simTypes{
	HEART_RATE_MONITOR,
	SPEED_DISTANCE_MONITOR,
	BIKE_POWER,
	BIKE_SPDCAD,
	BIKE_CADENCE,
	BIKE_SPEED,
	WEIGHT_SCALE,
	MULTISPORT_SPEED_DISTANCE,
	LIGHT_ELECTRIC_VEHICLE,
	GEOCACHE,
   AUDIO,
   CUSTOM,
	UNSELECTED  //Leave 'unselected' last so the index numbers of the combo box match
};


#define SIM_BASE_MASK   ((UCHAR) 0x03)
#define SIM_SENSOR      ((UCHAR) 0x01)
#define SIM_DISPLAY     ((UCHAR) 0x02)
