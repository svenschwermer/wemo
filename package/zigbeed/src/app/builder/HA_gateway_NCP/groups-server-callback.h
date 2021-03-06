// This file is generated by AppBuilder. Please do not edit manually.
// 
//

/**
 * @addtogroup callback Application Framework V2 callback interface Reference
 * This header provides callback function prototypes for the Groups server cluster
 * application framework plugin.
 * @{
 */


/** @brief Get Group Name
 *
 * This function returns the name of a group with the provided group ID,
 * should it exist.
 *
 * @param endpoint Endpoint  Ver.: always
 * @param groupId Group ID  Ver.: always
 * @param groupName Group Name  Ver.: always
 */
void emberAfPluginGroupsServerGetGroupNameCallback(int8u endpoint,
                                                   int16u groupId,
                                                   int8u * groupName);

/** @brief Set Group Name
 *
 * This function sets the name of a group with the provided group ID.
 *
 * @param endpoint Endpoint  Ver.: always
 * @param groupId Group ID  Ver.: always
 * @param groupName Group Name  Ver.: always
 */
void emberAfPluginGroupsServerSetGroupNameCallback(int8u endpoint,
                                                   int16u groupId,
                                                   int8u * groupName);

/** @brief Group Names Supported
 *
 * This function returns whether or not group names are supported.
 *
 * @param endpoint Endpoint  Ver.: always
 */
boolean emberAfPluginGroupsServerGroupNamesSupportedCallback(int8u endpoint);



/** @} END addtogroup */
