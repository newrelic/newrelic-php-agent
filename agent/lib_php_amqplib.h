/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Functions relating to instrumenting AWS-SDK-PHP.
 */
#ifndef LIB_PHP_AMQPLIB
#define LIB_PHP_AMQPLIB

#define RABBITMQ_LIBRARY_NAME "RabbitMQ"
#define RABBITMQ_MESSAGING_SYSTEM "rabbitmq"

#define AMQP_CONSTRUCT_PARAMS_SERVER_INDEX 0
#define AMQP_CONSTRUCT_PARAMS_PORT_INDEX 1

/*
 * Purpose : Enable the library after detection.
 *
 * Params  : None
 *
 * Returns : None
 */
extern void nr_aws_php_amqplib_enable();

/*
 * Purpose : Detect the version and create package and metrics.
 *
 * Params  : None
 *
 * Returns : None
 */
extern void nr_php_amqplib_handle_version();

#endif /* LIB_PHP_AMQPLIB */
