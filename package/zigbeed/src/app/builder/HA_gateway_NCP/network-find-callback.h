// This file is generated by AppBuilder. Please do not edit manually.
// 
//

/**
 * @addtogroup callback Application Framework V2 callback interface Reference
 * This header provides callback function prototypes for the Network Find
 * application framework plugin.
 * @{
 */


/** @brief Finished
 *
 * This callback is fired when the network-find plugin is finished with the
 * forming or joining process.  The result of the operation will be returned
 * in the status parameter.
 *
 * @param status   Ver.: always
 */
void emberAfPluginNetworkFindFinishedCallback(EmberStatus status);

/** @brief Get Radio Power For Channel
 *
 * This callback is called by the framework when it is setting the radio power
 * during the discovery process. The framework will set the radio power
 * depending on what is returned by this callback.
 *
 * @param channel   Ver.: always
 */
int8s emberAfPluginNetworkFindGetRadioPowerForChannelCallback(int8u channel);

/** @brief Join
 *
 * This callback is called by the plugin when a joinable network has been
 * found.  If the application returns TRUE, the plugin will attempt to join
 * the network.  Otherwise, the plugin will ignore the network and continue
 * searching.  Applications can use this callback to implement a network
 * blacklist.
 *
 * @param networkFound   Ver.: always
 * @param lqi   Ver.: always
 * @param rssi   Ver.: always
 */
boolean emberAfPluginNetworkFindJoinCallback(EmberZigbeeNetwork * networkFound,
                                             int8u lqi,
                                             int8s rssi);



/** @} END addtogroup */
