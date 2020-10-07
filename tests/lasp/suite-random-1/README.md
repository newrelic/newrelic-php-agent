Suite: Random 1
==================================================
The tests in this suite use the security token in `security-token.txt`. These values should correspond to a testing account. For the tests to run properly, the policy values for this account should be as follows:

    record_sql                      enabled:true
    attributes_include              enabled:true
    allow_raw_exception_messages    enabled:false
    custom_events                   enabled:true
    custom_parameters               enabled:true
    custom_instrumentation_editor   enabled:true
    message_parameters              enabled:false
    job_arguments                   enabled:true

These tests may be invoked with the integration runner with a make invocation (run from the root repository folder) that looks like this.

    make lasp-test SUITE_LASP=suite-random-1
