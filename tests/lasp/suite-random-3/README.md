Suite: Random 3
==================================================
The tests in this suite use the security token in `security-token.txt`. These values should correspond to a testing account. For the tests to run properly, the policy values for this account should be as follows:

    record_sql                      enabled:false
    attributes_include              enabled:true
    allow_raw_exception_messages    enabled:true
    custom_events                   enabled:true
    custom_parameters               enabled:false
    custom_instrumentation_editor   enabled:true
    message_parameters              enabled:true
    job_arguments                   enabled:true

These tests may be invoked with the integration runner with a make invocation (run from the root repository folder) that looks like this.

    make lasp-test SUITE_LASP=suite-random-3
