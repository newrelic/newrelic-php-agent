This directory contains unit tests for Axiom. Building and running the tests is
a standard part of the Jenkins PR builder, and developers should be constantly 
improving these tests, especially to catch regressions. 

Tests are written using the *tlib* framework. This framework compiles the basic 
functions needed by most other tests and provides a mechanism for reporting 
test success and failure in a somewhat friendly manner. See `tlib_main.h` for 
further details.
