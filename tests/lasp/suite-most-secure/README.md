Suite: Most Secure
==================================================
The tests in this suite use the security token in `security-token.txt`. These values should correspond to a testing account. For the tests to run properly, the policy values for this account should be as follows:

    record_sql                      enabled:false
    attributes_include              enabled:false
    allow_raw_exception_messages    enabled:false
    custom_events                   enabled:false
    custom_parameters               enabled:false
    custom_instrumentation_editor   enabled:false
    message_parameters              enabled:false
    job_arguments                   enabled:false

These tests may be invoked with the integration runner with a make invocation (run from the root repository folder) that looks like this.

    make lasp-test SUITE_LASP=suite-most-secure
