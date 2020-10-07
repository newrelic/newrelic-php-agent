<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

namespace NewRelic\Integration;

use InvalidArgumentException;
use stdClass;

/**
 * A class representing datastore instance metadata attached to a slow SQL or
 * transaction trace node.
 */
class DatastoreInstance
{
    /**
     * The database name.
     *
     * @var string
     */
    public $databaseName;

    /**
     * The host name.
     *
     * @var string
     */
    public $host;

    /**
     * The port (which will be an integer), or socket path (which will be a
     * string).
     *
     * @var integer|string
     */
    public $portPathOrId;

    /**
     * Constructs a new datastore instance based on the given parameters.
     *
     * @internal
     * @throws InvalidArgumentException
     */
    public function __construct(stdClass $params)
    {
        if (!static::hasAttributes($params)) {
            throw new InvalidArgumentException('Missing one or more instance fields');
        }

        $this->databaseName = $params->database_name;
        $this->host = $params->host;
        $this->portPathOrId = $params->port_path_or_id;
    }

    /**
     * Checks if the datastore instance matches the given host.
     *
     * If the given host is considered to be localhost by the agent, then the
     * host name is actually checked against the system host name (as would be
     * expected per the ICAPM spec).
     *
     * @param string $host
     * @return boolean
     */
    public function isHost($host)
    {
        if (newrelic_is_localhost($host)) {
            $host = newrelic_get_hostname();
        }

        return ($host === $this->host);
    }

    /**
     * Checks if the datastore instance host is localhost.
     *
     * This is generally only useful for datastore instrumentation that
     * distinguishes between UNIX domain sockets and TCP sockets: a UNIX domain
     * socket test would likely use this method to check if the host was set
     * correctly to the local host name.
     *
     * @return boolean
     */
    public function isLocalhost()
    {
        return (newrelic_is_localhost($this->host) || ($this->host === newrelic_get_hostname()));
    }

    /**
     * Helper method to check if the given parameters object contains the
     * fields expected if datastore instance metadata was attached.
     *
     * @internal
     * @param stdClass $params
     * @return boolean
     */
    static public function hasAttributes(stdClass $params)
    {
        return isset($params->host, $params->port_path_or_id, $params->database_name);
    }
}
