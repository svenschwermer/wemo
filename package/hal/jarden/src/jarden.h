#ifndef _JARDEN_H_
#define _JARDEN_H_

#define MAX_COOK_TIME	(20*60)	

#define MAX_DELAY_TIME  (24*60)

// Hardware abstraction layer specific errors
#ifndef ERR_HAL_BASE
#define ERR_HAL_BASE								-10100
#endif

// Internal HAL error: HAL Daemon returned an unexpected response
#define ERR_HAL_INVALID_RESPONSE_TYPE		(ERR_HAL_BASE - 1)

// Communications with HAL Daemon failed.
#define ERR_HAL_DAEMON_TIMEOUT				(ERR_HAL_BASE - 2)

// Attempted to set cook time when temperature not set
#define ERR_HAL_WRONG_MODE					(ERR_HAL_BASE - 3)

//Attempted wrong command for a product
#define ERR_HAL_WRONG_CMD                   (ERR_HAL_BASE - 4)

typedef enum {
	MODE_UNKNOWN = 1,
	MODE_OFF,
	MODE_IDLE,       // just turned on, no temp or time set yet
	MODE_WARM,
	MODE_LOW,
	MODE_HIGH,
	MODE_ON,
	NUM_MODES
} JARDEN_MODE;

// Initialize Hardware Abstraction Layer for use
// Returns:
// 	  0 - SUCCESS
// 	> 0 - standard Linux errno error code
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_Init();

// Return the current mode decoded from the product LEDS.
//		< 0 - HAL error code (see ERR_HAL_*)
JARDEN_MODE HAL_GetMode(void);

// Set cooking mode
// Legal values are MODE_OFF, MODE_WARM, MODE_LOW or MODE_HIGH.
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_SetMode(JARDEN_MODE Mode);

// Return number of minutes of cook time remaining from product LED display. 
// A value of 0 indicates that the cockpot is not in a cooking mode
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_GetCookTime(void);

// Set number of minutes to cook time at currently selected temperature.
// The current mode must be MODE_LOW or MODE_HIGH to set a cook time. 
//  
// Note: requested value will be rounded up to nearest 30 minute increment
// (CrockPot hardware limitations) 
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_SetCookTime(int Minutes);

/*
 * Return number of minutes of delay time remaining from product.
 * < 0 - error
 */
int HAL_GetDelayTime(void);

/*
 * Set number of minutes to delay the togle operation.
 * If the device is ON, it will be powered OFF after this delay minutes
 * If the device is OFF, it will be powered ON after this delay minutes
 * < 0 - error
 */
int HAL_SetDelayTime(int Minutes);

// Shutdown the HAL deamon. 
// 
// 	  0 - SUCCESS
// 	> 0 - standard Linux errno error code
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_Shutdown();


// Free and release resources used by HAL
// 
// 	  0 - SUCCESS
// 	> 0 - standard Linux errno error code
//		< 0 - HAL error code (see ERR_HAL_*)
int HAL_Cleanup();

// Return return string describing error number
const char *HAL_strerror(int Err);

#endif  // _JARDEN_H_
