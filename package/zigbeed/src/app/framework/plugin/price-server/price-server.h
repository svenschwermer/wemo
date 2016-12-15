// *******************************************************************
// * price-server.h
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifndef EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE
  #define EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE 5
#endif

#define ZCL_PRICE_CLUSTER_PRICE_ACKNOWLEDGEMENT_MASK 0x01
#define ZCL_PRICE_CLUSTER_RESERVED_MASK              0xFE

#define ZCL_PRICE_CLUSTER_START_TIME_NOW         0x00000000UL
#define ZCL_PRICE_CLUSTER_END_TIME_NEVER         0xFFFFFFFFUL
#define ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED 0xFFFF
#define ZCL_PRICE_CLUSTER_NUMBER_OF_EVENTS_ALL   0x00

/**
 * @brief The price and metadata used by the Price server plugin.
 *
 * The application can get and set the prices used by the plugin by calling
 * ::emberAfPriceGetPriceTableEntry and
 * ::emberAfPriceSetPriceTableEntry.
 */
typedef struct {
  int8u   rateLabel[ZCL_PRICE_CLUSTER_MAXIMUM_RATE_LABEL_LENGTH + 1];
  int32u  providerId;
  int32u  issuerEventID;
  int32u  startTime;
  int32u  price;
  int32u  generationPrice;
  int32u  alternateCostDelivered;
  int16u  currency;
  int16u  duration; // in minutes
  int8u   unitOfMeasure;
  int8u   priceTrailingDigitAndTier;
  int8u   numberOfPriceTiersAndTier; // added later in errata
  int8u   priceRatio;
  int8u   generationPriceRatio;
  int8u   alternateCostUnit;
  int8u   alternateCostTrailingDigit;
  int8u   numberOfBlockThresholds;
  int8u   priceControl;
} EmberAfScheduledPrice;

#define ZCL_PRICE_INVALID_INDEX 0xFF

/** 
 * @brief Clear all prices in the price table. 
 * 
 * @param endpoint The endpoint in question
 **/
void emberAfPriceClearPriceTable(int8u endpoint);

/**
 * @brief Get a price used by the Price server plugin.
 *
 * This function can be used to get a price and metadata that the plugin will
 * send to clients.  For "start now" prices that are current or scheduled, the
 * duration is adjusted to reflect how many minutes remain for the price.
 * Otherwise, the start time and duration of "start now" prices reflect the
 * actual start and the original duration.
 *
 * @param endpoint The relevant endpoint
 * @param index The index in the price table.
 * @param price The ::EmberAfScheduledPrice structure describing the price.
 * @return TRUE if the price was found or FALSE is the index is invalid.
 */
boolean emberAfPriceGetPriceTableEntry(int8u endpoint, 
                                       int8u index,
                                       EmberAfScheduledPrice *price);

/**
 * @brief Set a price used by the Price server plugin.
 *
 * This function can be used to set a price and metadata that the plugin will
 * send to clients.  Setting the start time to zero instructs clients to start
 * the price now.  For "start now" prices, the plugin will automatically adjust
 * the duration reported to clients based on the original start time of the
 * price.
 *
 * @param endpoint The relevant endpoint
 * @param index The index in the price table.
 * @param price The ::EmberAfScheduledPrice structure describing the price.  If
 * NULL, the price is removed from the server.
 * @return TRUE if the price was set or removed or FALSE is the index is
 * invalid.
 */
boolean emberAfPriceSetPriceTableEntry(int8u endpoint, 
                                       int8u index,
                                       const EmberAfScheduledPrice *price);

/**
 * @brief Get the current price used by the Price server plugin.
 *
 * This function can be used to get the current price and metadata that the
 * plugin will send to clients.  For "start now" prices, the duration is
 * adjusted to reflect how many minutes remain for the price.  Otherwise, the
 * start time and duration reflect the actual start and the original duration.
 *
 * @param endpoint The relevant endpoint
 * @param price The ::EmberAfScheduledPrice structure describing the price.
 * @return TRUE if the current price was found or FALSE is there is no current
 * price.
 */
boolean emberAfGetCurrentPrice(int8u endpoint, EmberAfScheduledPrice *price);

/**
 * @brief Find the first free index in the price table
 *
 * This function looks through the price table and determines whether
 * the entry is in-use or scheduled to be in use; if not, it's 
 * considered "free" for the purposes of the user adding a new price
 * entry to the server's table, and the index is returned.
 *
 * @param endpoint The relevant endpoint
 * @return The index of the first free (unused/unscheduled) entry in
 * the requested endpoint's price table, or ZCL_PRICE_INVALID_INDEX
 * if no available entry could be found.
 */
int8u emberAfPriceFindFreePriceIndex(int8u endpoint);
 
void emberAfPricePrint(const EmberAfScheduledPrice *price);
void emberAfPricePrintTable(int8u endpoint);
void emberAfPluginPriceServerPublishPriceMessage(EmberNodeId nodeId,
                                                 int8u srcEndpoint,
                                                 int8u dstEndpoint,
                                                 int8u priceIndex);
