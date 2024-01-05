# Coding Style Guidelines for C

_"This does not contain everything, but what it contains is true"._

1. Document all constants, enums, typedefs, struct 
   fields and function declarations. Use the following style for function 
   comments headers:

```
  /*
   * Purpose: All function declarations in .h files in the repository are documented. 
   * Every function has a Purpose, lists its Params as well
   * as its Return value.  
   * 
   * Params:  1. parameter1  Long descriptions must be indented for easier 
   *                         reading. Notice how this description is long but 
   *                         is indented with the parameter description to set 
   *                         it apart from the next param.
   *          2. parameter2  Here's that other param.
   *
   *
   *          3. parameter3  But when descriptions are short.
   *          4. parameter4  It's fine for them to omit the tabs.
   *
   */
``` 

0. Use snake case (all lowercase, words separated by an underscore) for all
   identifiers, because this makes for a consistent experience when reading 
   code.
   ```c
   nr_datastore_t ds_type;
   ```

0. Add a `_t` postfix to all typedef identifiers, because this makes it clear an
   identifier is a typedef.
   ```c
   typedef struct _newrelic_segment_t newrelic_segment_t;
   ```

0. Use bool for values that can only be true or false, because it improves
   readability and reduces ambiguity.
   ```c
   bool create_successful = false;
   ```

0. Prefix all non-static internal functions with `nr_`, and all public functions
   with `newrelic_`, because this makes it obvious where a function belongs.
   ```c
   nr_status_t newrelic_connect_app(newrelic_app_t* app,
                                    unsigned short timeout_ms);
   bool nr_txn_ignore(nrtxn_t* txn);
   ```

0. Prefix all internal enum values with `NR_`, and all public enum values with
   `NEWRELIC_`, because this makes it obvious where an enum value belongs.
   ```c
   typedef enum _newrelic_loglevel_t {
       NEWRELIC_LOG_ERROR,
       NEWRELIC_LOG_WARNING,
       NEWRELIC_LOG_INFO,
       NEWRELIC_LOG_DEBUG,
   } newrelic_loglevel_t;

   typedef enum _nr_status_t {                                                     
       NR_SUCCESS = 0,                                                               
       NR_FAILURE = -1,                                                              
   } nr_status_t;  
   ```

0. Document all public functions in header files, because this makes it possible
   to use a function and know about its caveats without digging into the 
   implementation.
   ```c
   /**
    * @brief Ignore the current transaction
    *
    * Given a transaction, this function instructs the C SDK to not send data to
    * New Relic for that transaction.
    *
    * @param [in] transaction A transaction.
    *
    * @return true on success.
    */
   bool newrelic_ignore_transaction(newrelic_txn_t* transaction);
   ```

0. Use Yoda conditions, because this reduces the risk of accidental assignment.
   ```c
   if (NULL == attribute) {                                                      
     return;                                                                     
   } 
   ```

0. Use NULL instead of 0 for all null pointer values, because this makes it
   clear that it's a pointer and not a numeric value.
   ```c
   newrelic_txn_t* transaction = NULL;
   ```

0. Use designated struct initialization wherever possible, because it increases
   readability.
   ```c
   nr_segment_datastore_params_t params = {
       .operation = segment->type.datastore.operation,
       .collection = segment->type.datastore.collection,
       .instance = &segment->type.datastore.instance
   };
   ```

0. Allocate structs on the stack wherever possible, because this avoids more
   expensive heap allocations.
   ```c
   nr_segment_datastore_params_t params = {
       .operation = segment->type.datastore.operation,
       .collection = segment->type.datastore.collection,
       .instance = &segment->type.datastore.instance
   };
   nr_segment_datastore_end(segment->segment, &params);
   ```

0. Avoid vague TODO and FIXME comments, because those never get done.
   ```
   72877ec1ce (A Dev                1998-09-20 07:15:30 -0700  846)   return -1; /* TODO: dodgy */
   ```

0. Use const for function parameters passed by reference where the function
   does not modify the data, because this prevents accidental changes and makes
   it easier to reason about code parts.
   ```c
   newrelic_custom_event_t* newrelic_create_custom_event(const char* event_type);
   ```

0. Format code with
   [clang-format 3.8](https://releases.llvm.org/3.8.0/tools/clang/docs/ClangFormat.html) 
   or later using our [style configuration file](.clang-format).

0. Disregard directives from this list when other approaches are more
   beneficial to the overall goal. Do not blindly follow rules.

