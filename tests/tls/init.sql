--
-- Copyright 2026 New Relic Corporation. All rights reserved.
-- SPDX-License-Identifier: Apache-2.0
--
-- Users for the TLS integration tests. Each password is the user name. They use
-- the server's default authentication plugin, which PHP 7.4 and later support;
-- the TLS tests are skipped on older PHP versions.
--
CREATE USER 'tls_optional'@'%' IDENTIFIED BY 'tls_optional';
CREATE USER 'tls_required'@'%' IDENTIFIED BY 'tls_required' REQUIRE SSL;
CREATE USER 'tls_clientcert'@'%' IDENTIFIED BY 'tls_clientcert' REQUIRE X509;
CREATE USER 'nr_one_conn'@'%' IDENTIFIED BY 'nr_one_conn' WITH MAX_USER_CONNECTIONS 1;
-- A user without a password.
CREATE USER 'nr_nopass'@'%';

GRANT ALL ON `database`.* TO 'tls_optional'@'%';
GRANT ALL ON `database`.* TO 'tls_required'@'%';
GRANT ALL ON `database`.* TO 'tls_clientcert'@'%';
GRANT ALL ON `database`.* TO 'nr_one_conn'@'%';
GRANT ALL ON `database`.* TO 'nr_nopass'@'%';

-- A table for the test that changes user and database with
-- mysqli_change_user(); the test only reads it.
CREATE TABLE IF NOT EXISTS `database`.nr_change_user_tmp (id INT);

-- A user that may read a view but not the table behind it. MySQL refuses EXPLAIN
-- on the view for this user.
CREATE TABLE IF NOT EXISTS `database`.nr_view_priv_t (id INT);
INSERT INTO `database`.nr_view_priv_t SELECT 1 FROM DUAL
  WHERE NOT EXISTS (SELECT 1 FROM `database`.nr_view_priv_t);
CREATE OR REPLACE SQL SECURITY DEFINER VIEW `database`.nr_view_priv_v
  AS SELECT id FROM `database`.nr_view_priv_t;
CREATE USER 'nr_view_only'@'%' IDENTIFIED BY 'nr_view_only';
GRANT SELECT ON `database`.nr_view_priv_v TO 'nr_view_only'@'%';
