// This file is generated by AppBuilder. Please do not edit manually.
// 
//

/**
 * @addtogroup callback Application Framework V2 callback interface Reference
 * This header provides callback function prototypes for the ZLL On/Off server
 * cluster enhancements application framework plugin.
 * @{
 */


/** @brief Off With Effect
 *
 * This callback is called by the ZLL On/Off Server plugin whenever an
 * OffWithEffect command is received.  The application should implement the
 * effect and variant requested in the command and return
 * ::EMBER_ZCL_STATUS_SUCCESS if successful or an appropriate error status
 * otherwise.
 *
 * @param endpoint   Ver.: always
 * @param effectId   Ver.: always
 * @param effectVariant   Ver.: always
 */
EmberAfStatus emberAfPluginZllOnOffServerOffWithEffectCallback(int8u endpoint,
                                                               int8u effectId,
                                                               int8u effectVariant);



/** @} END addtogroup */