// This file is generated by AppBuilder. Please do not edit manually.
// 
//

/**
 * @addtogroup callback Application Framework V2 callback interface Reference
 * This header provides callback function prototypes for the Button Form/Join Code
 * application framework plugin.
 * @{
 */


/** @brief Button Event
 *
 * This allows another module to get notification when a button is pressed and
 * released but the button joining plugin did not handle it.  This callback is
 * NOT called in ISR context so there are no restrictions on what code can
 * execute.
 *
 * @param buttonNumber The button number that was pressed.  Ver.: always
 * @param buttonPressDurationMs The length of time button was held down before
 * it was released.  Ver.: always
 */
void emberAfPluginButtonJoiningButtonEventCallback(int8u buttonNumber,
                                                   int32u buttonPressDurationMs);



/** @} END addtogroup */
