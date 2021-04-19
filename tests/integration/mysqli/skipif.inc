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

/*
$link = @mysql_connect($MYSQL_SERVER, $MYSQL_USER, $MYSQL_PASSWD);
if (FALSE === $link) {
    die("skip: " . mysql_error() . "\n");
}

if (!empty($MYSQL_DB)) {
    if (FALSE === mysql_select_db($MYSQL_DB, $link)) {
        die("skip: " . mysql_error() . "\n");
    }
}
*/


$link = @mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD);

if (FALSE === $link) {
    die("skip: " . mysqli_error() . "\n");
}

if (!empty($MYSQL_DB)) {
    if (FALSE === mysqli_select_db($link, $MYSQL_DB)) {
        /* Database doesn't exist, will try to create. */
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
    }
}

mysqli_close($link);


