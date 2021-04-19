<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

if (!extension_loaded("mysqli")) {
  die("skip: mysqli extension required");
}

/*
 * Prevent triggered errors/warnings/notices from interfering with the
 * test runner's handling of SKIPIF, which requires the "skip: ..."
 * message to be the first line of output. This is trickier that it
 * ought to be. With the mysqlnd driver, it's necessary to set the
 * reporting mode to strict, AND to apply the error control operator '@'
 * the connect call.
 */

mysqli_report(MYSQLI_REPORT_STRICT);   // prevent warning if connect fails

try {
    $link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);

    mysqli_close($link);
} catch (Exception $e) {
    /* Error connecting to DB.  Will attempt to create and populate a new one. */
  try {
    echo "Will try to create new DB after not connecting to DB." . $e->getMessage() . "\n";
    $link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD);

    /* Create database. */
    $sql = "CREATE DATABASE " .  $MYSQL_DB;
    $result = mysqli_query($link, $sql);
    mysqli_select_db($link, $MYSQL_DB);

    $result = mysqli_query($link, $sql);
    if (FALSE === $result) {
        die("skip: Error with database.");
    }

    /* Create table `COLUMNS`. */
    if (mysqli_query($link,"DESCRIBE  COLUMNS ")){
        /* Already exists */
    }else{
        $sql = "CREATE TABLE COLUMNS(
            id INT(6) UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            code VARCHAR(30) NOT NULL
            )";
        mysqli_query($link, $sql);
    }


    /* Create table `STATISTICS`. */
    if (mysqli_query($link,"DESCRIBE  STATISTICS ")){
        /* Already exists */
    }else{
        $sql = "CREATE TABLE STATISTICS(
            id INT(6) UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            code VARCHAR(30) NOT NULL
            )";
        mysqli_query($link, $sql);
    }

    /* Create table `TRIGGERS`. */
    if (mysqli_query($link,"DESCRIBE  TRIGGERS ")){
        /* Already exists */
    }else{
        $sql = "CREATE TABLE TRIGGERS(
            id INT(6) UNSIGNED AUTO_INCREMENT PRIMARY KEY,
            code VARCHAR(30) NOT NULL
            )";
        mysqli_query($link, $sql);
    }


    /* commit transaction */
    if (!mysqli_commit($link)) {
        print("Transaction commit failed.\n");
    } else {
        echo "Transaction commit succeeded.\n";
    }

    mysqli_close($link);

    } catch (Exception $e) {
        die("skip: " . $e->getMessage() . "\n");
    }
}
